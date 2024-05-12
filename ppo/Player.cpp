#include "Player.h"

XMFLOAT3 MultipleVelocity(const XMFLOAT3& dir, const XMFLOAT3& scalar)
{
	return XMFLOAT3(dir.x * scalar.x, dir.y * scalar.y, dir.z * scalar.z);
}

Player::Player() : GameObject()
{
	InitPlayer();
}

void Player::ChangeState(PlayerState* nextState)
{
	if (nextState->ID() == mCurrentState->ID())
		return;

	if ((UINT)nextState->ID() < 0 || (UINT)nextState->ID() >= (UINT)StateId::Count)
		return;

	mCurrentState->Exit(*this);
	delete mCurrentState;
	mCurrentState = nextState;
	mCurrentState->Enter(*this);
}

Player::Player(const string name, XMFLOAT4X4 world, XMFLOAT4X4 texTransform) : 
    GameObject(name, world, texTransform)
{
	InitPlayer();
}

Player::Player(const string name, XMMATRIX world, XMMATRIX texTransform) :
    GameObject(name, world, texTransform)
{
	InitPlayer();
}

Player::~Player()
{
	if (mCamera)
		delete mCamera;
	if (m_pMasteringVoice) {
		m_pMasteringVoice->DestroyVoice();
	}
	m_pXAudio2.Reset();
	CoUninitialize();
}

void Player::Update(const GameTimer& gt)
{
	float deltaTime = gt.DeltaTime();

	// �߶�
	mVelocity.y -= mGravity * deltaTime;
	float fallSpeed = sqrt(mVelocity.y * mVelocity.y);
	if ((mVelocity.y * mVelocity.y) > (mMaxVelocityY * mMaxVelocityY) && mVelocity.y < 0) {
		mVelocity.y = -mMaxVelocityY;
	}

	// �ִ� �ӵ� ����
	float maxVelocityXZ = mIsRun ? mMaxRunVelocityXZ : mMaxWalkVelocityXZ;
	float groundSpeed = sqrt(mVelocity.x * mVelocity.x + mVelocity.z * mVelocity.z);
	if (groundSpeed > maxVelocityXZ) {
		mVelocity.x *= maxVelocityXZ / groundSpeed;
		mVelocity.z *= maxVelocityXZ / groundSpeed;
	}

	// ����
	XMFLOAT3 friction;
	XMStoreFloat3(&friction, -XMVector3Normalize(XMVectorSet(mVelocity.x, 0.0f, mVelocity.z, 0.0f)) * mFriction * deltaTime);
	mVelocity.x = (mVelocity.x >= 0.0f) ? max(0.0f, mVelocity.x + friction.x) : min(0.0f, mVelocity.x + friction.x);
	mVelocity.z = (mVelocity.z >= 0.0f) ? max(0.0f, mVelocity.z + friction.z) : min(0.0f, mVelocity.z + friction.z);

	SetPosition(Vector3::Add(GetPosition(), mVelocity));

	if (GetPosition().y < 0) {
		SetPosition(GetPosition().x, 0.0f, GetPosition().z);
		mVelocity.y = 0.0f;
		mIsFalling = false;
	}

	// ī�޶� �̵�
	UpdateCamera();
	UpdateState(deltaTime);

	mAnimationTime += deltaTime;

	SetFrameDirty();
}

void Player::UpdateCamera()
{
	if (mMode == THIRD_PERSON) {
		XMVECTOR cameraLook = XMVector3TransformNormal(XMLoadFloat3(&GetLook()), XMMatrixRotationAxis(XMLoadFloat3(&GetRight()), mPitch));
		XMVECTOR playerPosition = XMLoadFloat3(&GetPosition()) + XMLoadFloat3(&mCameraOffsetPosition) + XMVectorSet(0.0f, 50.0f, 0.f, 0.0f);
		XMVECTOR cameraPosition = playerPosition - cameraLook * 500.f;// + XMVectorSet(0.0f, 50.0f, 0.f, 0.0f); // distance�� ī�޶�� �÷��̾� ������ �Ÿ�

		XMMATRIX viewMatrix = XMMatrixLookAtLH(cameraPosition, playerPosition, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
		mCamera->LookAt(cameraPosition, playerPosition, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));

		mCamera->UpdateViewMatrix();
	}
	// if (mMode == FIRST_PERSON) {
	// 	// �÷��̾��� ��ġ�� ������ ������
	// 	XMVECTOR playerPosition = XMLoadFloat3(&GetPosition()) + XMLoadFloat3(&mCameraOffsetPosition);
	// 	XMVECTOR playerLook = XMLoadFloat3(&GetLook());
	// 
	// 	// ī�޶� �÷��̾ �߽����� ȸ���ϵ��� ������ ���� ���͸� ���
	// 	XMMATRIX rotationMatrix = XMMatrixRotationAxis(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), mPitch); // X���� �߽����� ȸ��
	// 	XMVECTOR cameraLook = XMVector3Transform(playerLook, rotationMatrix);
	// 
	// 	// �÷��̾ �߽����� �ϴ� ī�޶� ��ġ ���
	// 	XMVECTOR cameraPosition = playerPosition + cameraLook * 30.0f; // ī�޶� �÷��̾� �ڷ� �̵�
	// 
	// 	// ī�޶��� �� ��� ����
	// 	mCamera->SetViewMatrix(XMMatrixLookToLH(cameraPosition, cameraLook, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f))); // ��� ���͸� �������� �� ��� ����
	// }

	else if (mMode == FIRST_PERSON) {
		// Get player position and look direction
		XMVECTOR cameraLook = XMVector3TransformNormal(XMLoadFloat3(&GetLook()), XMMatrixRotationAxis(XMLoadFloat3(&GetRight()), mPitch));
		XMVECTOR playerPosition = XMLoadFloat3(&GetPosition()) + XMLoadFloat3(&mCameraOffsetPosition) + XMVectorSet(0.0f, 50.0f, 0.f, 0.0f);

		// Calculate the camera position
		XMVECTOR cameraPosition = playerPosition + cameraLook * mZoomFactor;

		// Set the camera view matrix
		mCamera->SetViewMatrix(XMMatrixLookToLH(cameraPosition, cameraLook, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)));
	}

	// if (mMode == FIRST_PERSON) {
	// 	// �÷��̾��� ��ġ�� ������ ������
	// 	XMVECTOR playerPosition = XMLoadFloat3(&GetPosition()) + XMLoadFloat3(&mCameraOffsetPosition);
	// 	XMVECTOR playerLook = XMLoadFloat3(&GetLook());
	// 
	// 	// ī�޶� �÷��̾ �߽����� ȸ���ϵ��� ������ ���� ���͸� ���
	// 	XMMATRIX rotationMatrix = XMMatrixRotationAxis(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), mPitch); // X���� �߽����� ȸ��
	// 	XMVECTOR cameraLook = XMVector3Transform(playerLook, rotationMatrix);
	// 
	// 	// �÷��̾ �߽����� �ϴ� ī�޶� ��ġ ���
	// 	XMVECTOR cameraPosition = playerPosition + cameraLook * 30.0f; // ī�޶� �÷��̾� �ڷ� �̵�
	// 
	// 	// ī�޶� �־��� ������ŭ �¿�� ȸ��
	// 	float rotationAngle = 0.1f; // ���� ����
	// 	mCamera->RotateY(rotationAngle); // ȸ�� ������ ���ϴ� ������ �����ؾ� �մϴ�.
	// 
	// 	// ī�޶��� �� ��� ����
	// 	mCamera->SetViewMatrix(XMMatrixLookToLH(cameraPosition, cameraLook, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f))); // ��� ���͸� �������� �� ��� ����
	// }
}

void Player::UpdateState(const float deltaTime)
{
	if (GetPosition().y > 0 && !mIsFalling) {
		mIsFalling = true;
		ChangeState(new PlayerStateFall);
	}

	if (!mIsFalling) {
		float groundSpeed = sqrt(mVelocity.x * mVelocity.x + mVelocity.z * mVelocity.z);
		if (groundSpeed <= 0.1f) {
			if (GetStateId() != (UINT)StateId::Land && GetStateId() != (UINT)StateId::Fall)
				ChangeState(new PlayerStateIdle);
		}
		else if (groundSpeed > mMaxWalkVelocityXZ) {
			ChangeState(new PlayerStateRun);
		}
		else {
			ChangeState(new PlayerStateWalk);
		}
	}

	mCurrentState->Update(*this, deltaTime);
}

void Player::KeyboardInput(float dt)
{
	mIsRun = false;
	if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
		mIsRun = true;

	XMFLOAT3 velocity;

	if (mIsFalling)
		velocity = Vector3::ScalarProduct(mAcceleration, dt / 2, false);
	else if (mIsRun)
		velocity = Vector3::ScalarProduct(mAcceleration, dt * 2, false);
	else
		velocity = Vector3::ScalarProduct(mAcceleration, dt, false);

	if (GetAsyncKeyState('W') & 0x8000) {
		if (!mIsFalling && !isPlayingW && !isPlayingA && !isPlayingS && !isPlayingD) {
			isPlayingW = true;
			// �Ҹ��� ����ϴ� ������ ����
			std::thread soundThread([this]() {
				while (isPlayingW) { // isPlayingW�� true�� ������ �ݺ�
					PlaySoundThread(L"Sound/walk1.wav");
				}
				});
			soundThread.detach(); // �����带 ����� ���� ������� ���������� ����			
		}
		mVelocity = Vector3::Add(mVelocity, MultipleVelocity(GetLook(), velocity));
	}
	else {
		if (isPlayingW) {
			// W Ű�� ������ �ʾ��� �� �Ҹ� ��� ���̸� ����
			// HRESULT hr = stopSound(); // �Ҹ� ��� ����			
			isPlayingW = false;
		}
	}
	if (GetAsyncKeyState('S') & 0x8000) {
		if (!mIsFalling && !isPlayingW && !isPlayingA && !isPlayingS && !isPlayingD) {
			isPlayingS = true;
			// �Ҹ��� ����ϴ� ������ ����
			std::thread soundThread([this]() {
				while (isPlayingS) { // isPlayingS�� true�� ������ �ݺ�
					PlaySoundThread(L"Sound/walk1.wav");
				}
				});
			soundThread.detach(); // �����带 ����� ���� ������� ���������� ����			
		}
		mVelocity = Vector3::Add(mVelocity, MultipleVelocity(Vector3::ScalarProduct(GetLook(), -1), velocity));
	}
	else {
		if (isPlayingS) {
			// S Ű�� ������ �ʾ��� �� �Ҹ� ��� ���̸� ����
			HRESULT hr = stopSound(); // �Ҹ� ��� ����			
			isPlayingS = false;
		}
	}
	if (GetAsyncKeyState('D') & 0x8000) {
		if (!mIsFalling && !isPlayingW && !isPlayingA && !isPlayingS && !isPlayingD) {
			isPlayingD = true;
			// �Ҹ��� ����ϴ� ������ ����
			std::thread soundThread([this]() {
				while (isPlayingD)  // isPlayingD�� true�� ������ �ݺ�
					PlaySoundThread(L"Sound/walk1.wav");
				});
			soundThread.detach(); // �����带 ����� ���� ������� ���������� ����			
		}
		mVelocity = Vector3::Add(mVelocity, MultipleVelocity(GetRight(), velocity));
	}
	else {
		if (isPlayingD) {
			// D Ű�� ������ �ʾ��� �� �Ҹ� ��� ���̸� ����
			HRESULT hr = stopSound(); // �Ҹ� ��� ����			
			isPlayingD = false;
		}
	}

	if (GetAsyncKeyState('A') & 0x8000) {
		if (!mIsFalling && !isPlayingW && !isPlayingA && !isPlayingS && !isPlayingD) {
			isPlayingA = true;
			// �Ҹ��� ����ϴ� ������ ����
			std::thread soundThread([this]() {
				while (isPlayingA) // isPlayingA�� true�� ������ �ݺ�
					PlaySoundThread(L"Sound/walk1.wav");
				});
			soundThread.detach(); // �����带 ����� ���� ������� ���������� ����			
		}
		mVelocity = Vector3::Add(mVelocity, MultipleVelocity(Vector3::ScalarProduct(GetRight(), -1), velocity));		
	}
	else {
		if (isPlayingA) {
			// A Ű�� ������ �ʾ��� �� �Ҹ� ��� ���̸� ����
			// HRESULT hr = stopSound(); // �Ҹ� ��� ����			
			isPlayingA = false;
		}
	}

	if (GetAsyncKeyState('I') & 0x8000 && mZoomFactor < 500.f && mMode == FIRST_PERSON) {	//  
		float in = mZoomFactor;
		in += 1.f;
		SetZoomFactor(in);
	}
	if (GetAsyncKeyState('O') & 0x8000 && mZoomFactor > 25.f && mMode == FIRST_PERSON) {
		float in = mZoomFactor;
		in -= 1.f;
		SetZoomFactor(in);
	}
	if (GetAsyncKeyState('T') & 0x8000)
		mMode = THIRD_PERSON;
	if (GetAsyncKeyState('Z') & 0x8000)
		mMode = FIRST_PERSON;
}

void Player::OnKeyboardMessage(UINT nMessageID, WPARAM wParam)
{
	switch (nMessageID)
	{
	case WM_KEYDOWN:
		switch (wParam)
		{
		case VK_SPACE:
			if (!mIsFalling) {
				HRESULT hr = stopSound(); // �Ҹ� ��� ����
				mVelocity.y += mJumpForce;
				mIsFalling = true;
				ChangeState(new PlayerStateJump);				
			}
			break;
		}
		break;
	}
}

void Player::MouseInput(float dx, float dy)
{
	Rotate(0.f, dx, 0.f);

	float maxPitchRaidan = XMConvertToRadians(MAX_PLAYER_CAMERA_PITCH);
	mPitch += dy;
	mPitch = (mPitch < -maxPitchRaidan) ? -maxPitchRaidan : (maxPitchRaidan < mPitch) ? maxPitchRaidan : mPitch;
}

HRESULT Player::InitializeXAudio2()
{
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(hr)) {
		return hr;
	}

#ifdef USING_XAUDIO2_7_DIRECTX
	HMODULE mXAudioDLL = LoadLibraryExW(L"XAudioD2_7.DLL", nullptr, 0x00000800 /* LOAD_LIBRARY_SEARCH_SYSTEM32 */);
	if (!mXAudioDLL) {
		CoUninitialize();
		return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
	}
#endif // USING_XAUDIO2_7_DIRECTX

	UINT32 flags = 0;
#if defined(USING_XAUDIO2_7_DIRECTX) && defined(_DEBUG)
	flags |= XAUDIO2_DEBUG_ENGINE;
#endif

	hr = XAudio2Create(m_pXAudio2.GetAddressOf(), flags);
	if (FAILED(hr)) {
		CoUninitialize();
		return hr;
	}

#if !defined(USING_XAUDIO2_7_DIRECTX) && defined(_DEBUG)
	XAUDIO2_DEBUG_CONFIGURATION debug = {};
	debug.TraceMask = XAUDIO2_LOG_ERRORS | XAUDIO2_LOG_WARNINGS;
	debug.BreakMask = XAUDIO2_LOG_ERRORS;
	m_pXAudio2->SetDebugConfiguration(&debug, 0);
#endif

	if (FAILED(hr = m_pXAudio2->CreateMasteringVoice(&m_pMasteringVoice))) {
		m_pXAudio2.Reset();
		CoUninitialize();
		return hr;
	}

	return S_OK;
}

HRESULT Player::playSound(LPCWSTR szFilename)
{
	if (!m_pXAudio2) {
		return E_FAIL;
	}

	HRESULT hr = PlayWave(m_pXAudio2.Get(), szFilename);
	if (FAILED(hr)) {
		return hr;
	}

	return S_OK;
}

HRESULT Player::stopSound()
{
	// �Ҹ��� ����ϱ� ���� �Ҹ� ������ �ʱ�ȭ�Ǿ����� Ȯ���մϴ�.
	if (!m_pXAudio2 || !m_pMasteringVoice)
	{
		// �Ҹ� ������ �ʱ�ȭ���� �ʾ����� ������ ��ȯ�մϴ�.
		return E_FAIL;
	}

	// ���� �Ҹ��� ��� ���� �ҽ� ���̽��� �����մϴ�.
	// �� ���ÿ����� �ܼ��� ��� �ҽ� ���̽��� ������ŵ�ϴ�.
	m_pXAudio2->StopEngine();

	return S_OK;
}

void Player::PlaySoundThread(LPCWSTR szFilename)
{
	HRESULT hr = InitializeXAudio2(); // XAudio2 �ʱ�ȭ
	hr = playSound(szFilename);
}

void Player::StopAllSounds()
{
	if (isPlayingW)
		SetIsPlayingW(false);
	if (isPlayingA)
		SetIsPlayingA(false);
	if (isPlayingS)
		SetIsPlayingS(false);
	if (isPlayingD)
		SetIsPlayingD(false);
	HRESULT hr = stopSound(); // �Ҹ� ��� ����
}

void Player::StartAllSounds()
{
	if (!isPlayingW)
		SetIsPlayingW(true);
	if (!isPlayingA)
		SetIsPlayingA(true);
	if (!isPlayingS)
		SetIsPlayingS(true);
	if (!isPlayingD)
		SetIsPlayingD(true);
}

void Player::InitPlayer()
{
	// Set Camera
	mCamera = new Camera();

	mCameraOffsetPosition = XMFLOAT3(0.0f, 100.0f, 0.0f);
	UpdateCamera();

	mAnimationIndex[0] = 0;
	mAnimationIndex[1] = 0;
	mAnimationIndex[2] = 1;
	mAnimationIndex[3] = 2;
	mAnimationIndex[4] = 3;
	mAnimationIndex[5] = 4;
	mAnimationIndex[6] = 5;

	mCurrentState = new PlayerStateIdle;
	mCurrentState->Enter(*this);
}

