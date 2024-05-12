#include "Sound.h"
#include "XAudio2Versions.h"
#include "WAVFileReader.h"

HRESULT InitializeXAudio2(ComPtr<IXAudio2>& pXAudio2, IXAudio2MasteringVoice** ppMasteringVoice)
{
    // Initialize COM
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr))
    {
        wprintf(L"Failed to init COM: %#X\n", static_cast<unsigned long>(hr));
        return hr;
    }

#ifdef USING_XAUDIO2_7_DIRECTX
    HMODULE mXAudioDLL = LoadLibraryExW(L"XAudioD2_7.DLL", nullptr, 0x00000800 /* LOAD_LIBRARY_SEARCH_SYSTEM32 */);
    if (!mXAudioDLL)
    {
        wprintf(L"Failed to find XAudio 2.7 DLL");
        CoUninitialize();
        return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
    }
#endif // USING_XAUDIO2_7_DIRECTX

    UINT32 flags = 0;
#if defined(USING_XAUDIO2_7_DIRECTX) && defined(_DEBUG)
    flags |= XAUDIO2_DEBUG_ENGINE;
#endif

    hr = XAudio2Create(pXAudio2.GetAddressOf(), flags);
    if (FAILED(hr))
    {
        wprintf(L"Failed to init XAudio2 engine: %#X\n", hr);
        CoUninitialize();
        return hr;
    }

#if !defined(USING_XAUDIO2_7_DIRECTX) && defined(_DEBUG)
    XAUDIO2_DEBUG_CONFIGURATION debug = {};
    debug.TraceMask = XAUDIO2_LOG_ERRORS | XAUDIO2_LOG_WARNINGS;
    debug.BreakMask = XAUDIO2_LOG_ERRORS;
    pXAudio2->SetDebugConfiguration(&debug, 0);
#endif

    // Create a mastering voice
    if (FAILED(hr = pXAudio2->CreateMasteringVoice(ppMasteringVoice)))
    {
        wprintf(L"Failed creating mastering voice: %#X\n", hr);
        pXAudio2.Reset();
        CoUninitialize();
        return hr;
    }

    return S_OK;
}

HRESULT PlayWave(IXAudio2* pXaudio2, LPCWSTR szFilename)
{
    if (!pXaudio2)
        return E_INVALIDARG;

    //
    // Locate the wave file
    //
    WCHAR strFilePath[MAX_PATH] = {};
    HRESULT hr = FindMediaFileCch(strFilePath, MAX_PATH, szFilename);
    if (FAILED(hr))
    {
        wprintf(L"Failed to find media file: %s\n", szFilename);
        return hr;
    }

    //
    // Read in the wave file
    //
    std::unique_ptr<uint8_t[]> waveFile;
    DirectX::WAVData waveData;
    if (FAILED(hr = DirectX::LoadWAVAudioFromFileEx(strFilePath, waveFile, waveData)))
    {
        wprintf(L"Failed reading WAV file: %#X (%s)\n", hr, strFilePath);
        return hr;
    }

    //
    // Play the wave using a XAudio2SourceVoice
    //

    // Create the source voice
    IXAudio2SourceVoice* pSourceVoice = nullptr;
    if (FAILED(hr = pXaudio2->CreateSourceVoice(&pSourceVoice, waveData.wfx)))
    {
        wprintf(L"Error %#X creating source voice\n", hr);
        return hr;
    }

    // Submit the wave sample data using an XAUDIO2_BUFFER structure
    XAUDIO2_BUFFER buffer = {};
    buffer.pAudioData = waveData.startAudio;
    buffer.Flags = XAUDIO2_END_OF_STREAM;  // tell the source voice not to expect any data after this buffer
    buffer.AudioBytes = waveData.audioBytes;

    if (waveData.loopLength > 0)
    {
        buffer.LoopBegin = waveData.loopStart;
        buffer.LoopLength = waveData.loopLength;
        buffer.LoopCount = 1; // We'll just assume we play the loop twice
    }

#if defined(USING_XAUDIO2_7_DIRECTX) || defined(USING_XAUDIO2_9)
    if (waveData.seek)
    {
        XAUDIO2_BUFFER_WMA xwmaBuffer = {};
        xwmaBuffer.pDecodedPacketCumulativeBytes = waveData.seek;
        xwmaBuffer.PacketCount = waveData.seekCount;
        if (FAILED(hr = pSourceVoice->SubmitSourceBuffer(&buffer, &xwmaBuffer)))
        {
            wprintf(L"Error %#X submitting source buffer (xWMA)\n", hr);
            pSourceVoice->DestroyVoice();
            return hr;
        }
    }
#else
    if (waveData.seek)
    {
        wprintf(L"This platform does not support xWMA or XMA2\n");
        pSourceVoice->DestroyVoice();
        return hr;
    }
#endif
    else if (FAILED(hr = pSourceVoice->SubmitSourceBuffer(&buffer)))
    {
        wprintf(L"Error %#X submitting source buffer\n", hr);
        pSourceVoice->DestroyVoice();
        return hr;
    }

    hr = pSourceVoice->Start(0);

    // Wait for the sound to finish playing
    BOOL isRunning = TRUE;
    while (SUCCEEDED(hr) && isRunning)
    {
        XAUDIO2_VOICE_STATE state;
        pSourceVoice->GetState(&state);
        isRunning = (state.BuffersQueued > 0) != 0;
        Sleep(10);
    }

    pSourceVoice->DestroyVoice();

    return hr;
}

HRESULT FindMediaFileCch(WCHAR* strDestPath, int cchDest, LPCWSTR strFilename)
{
    bool bFound = false;

    if (!strFilename || strFilename[0] == 0 || !strDestPath || cchDest < 10)
        return E_INVALIDARG;

    // Get the exe name, and exe path
    WCHAR strExePath[MAX_PATH] = {};
    WCHAR strExeName[MAX_PATH] = {};
    WCHAR* strLastSlash = nullptr;
    GetModuleFileName(nullptr, strExePath, MAX_PATH);
    strExePath[MAX_PATH - 1] = 0;
    strLastSlash = wcsrchr(strExePath, TEXT('\\'));
    if (strLastSlash)
    {
        wcscpy_s(strExeName, MAX_PATH, &strLastSlash[1]);

        // Chop the exe name from the exe path
        *strLastSlash = 0;

        // Chop the .exe from the exe name
        strLastSlash = wcsrchr(strExeName, TEXT('.'));
        if (strLastSlash)
            *strLastSlash = 0;
    }

    wcscpy_s(strDestPath, cchDest, strFilename);
    if (GetFileAttributes(strDestPath) != 0xFFFFFFFF)
        return S_OK;

    // Search all parent directories starting at .\ and using strFilename as the leaf name
    WCHAR strLeafName[MAX_PATH] = {};
    wcscpy_s(strLeafName, MAX_PATH, strFilename);

    WCHAR strFullPath[MAX_PATH] = {};
    WCHAR strFullFileName[MAX_PATH] = {};
    WCHAR strSearch[MAX_PATH] = {};
    WCHAR* strFilePart = nullptr;

    GetFullPathName(L".", MAX_PATH, strFullPath, &strFilePart);
    if (!strFilePart)
        return E_FAIL;

    while (strFilePart && *strFilePart != '\0')
    {
        swprintf_s(strFullFileName, MAX_PATH, L"%s\\%s", strFullPath, strLeafName);
        if (GetFileAttributes(strFullFileName) != 0xFFFFFFFF)
        {
            wcscpy_s(strDestPath, cchDest, strFullFileName);
            bFound = true;
            break;
        }

        swprintf_s(strFullFileName, MAX_PATH, L"%s\\%s\\%s", strFullPath, strExeName, strLeafName);
        if (GetFileAttributes(strFullFileName) != 0xFFFFFFFF)
        {
            wcscpy_s(strDestPath, cchDest, strFullFileName);
            bFound = true;
            break;
        }

        swprintf_s(strSearch, MAX_PATH, L"%s\\..", strFullPath);
        GetFullPathName(strSearch, MAX_PATH, strFullPath, &strFilePart);
    }
    if (bFound)
        return S_OK;

    // On failure, return the file as the path but also return an error code
    wcscpy_s(strDestPath, cchDest, strFilename);

    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
}