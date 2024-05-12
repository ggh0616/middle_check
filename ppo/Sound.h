#pragma once

#include <Windows.h>
#include <wrl\client.h>
#include <xaudio2.h>

using Microsoft::WRL::ComPtr;

HRESULT InitializeXAudio2(ComPtr<IXAudio2>& pXAudio2, IXAudio2MasteringVoice** ppMasteringVoice);
HRESULT PlayWave(IXAudio2* pXaudio2, LPCWSTR szFilename);
HRESULT FindMediaFileCch(WCHAR* strDestPath, int cchDest, LPCWSTR strFilename);