#pragma once

#include "Camera.h"
#include "GameObject.h"
#include "Sound.h"
#include <thread>

#define MAX_PLAYER_CAMERA_PITCH 85.0f

class Player;

enum class StateId : UINT
{
	Defalut = 0,
	Idle,
	Walk,
	Run,
	Jump,
	Fall,
	Land,
	Count
};

enum CameraMode
{
	FIRST_PERSON,
	THIRD_PERSON
};

class PlayerState
{
public:
	PlayerState() {}
	~PlayerState() {}

	StateId ID() { return mId; }

	virtual void Enter(Player& player) {};
	virtual void Update(Player& player, const float deltaTime) {};
	virtual void Exit(Player& player) {};
protected:
	StateId mId = StateId::Defalut;
};

class Player : public GameObject
{
public:
	Player();
	Player(const string name, XMFLOAT4X4 world, XMFLOAT4X4 texTransform);
	Player(const string name, XMMATRIX world, XMMATRIX texTransform);
	~Player();

	virtual void Update(const GameTimer& gt);
	void UpdateCamera();
	void UpdateState(const float deltaTime);		// PlayerState 변환

	void KeyboardInput(float dt);
	void OnKeyboardMessage(UINT nMessageID, WPARAM wParam);
	void MouseInput(float dx, float dy);

	Camera* GetCamera() { return mCamera; }

	void ChangeState(PlayerState* nextState);
	UINT GetStateId() { return (UINT)mCurrentState->ID(); }

	void SetAnimationTime() { mAnimationTime = 0.0f; }
	float GetAnimationTime() { return mAnimationTime; }
	UINT GetAnimationIndex() { return mAnimationIndex[GetStateId()]; }

	bool IsFalling() { return mIsFalling; }

	void SetZoomFactor(float zoom) { mZoomFactor = zoom; }

	HRESULT InitializeXAudio2();
	HRESULT playSound(LPCWSTR szFilename);
	HRESULT stopSound();

	virtual void PlaySoundThread(LPCWSTR szFilename);
	void StopAllSounds();
	void StartAllSounds();
	void SetIsPlayingW(bool isPlaying) { isPlayingW = isPlaying; }
	void SetIsPlayingA(bool isPlaying) { isPlayingA = isPlaying; }
	void SetIsPlayingS(bool isPlaying) { isPlayingS = isPlaying; }
	void SetIsPlayingD(bool isPlaying) { isPlayingD = isPlaying; }

	CameraMode GetMode() { return mMode; }
	
private:
	void InitPlayer();

	float mPitch = 0.0f;
	
	XMFLOAT3 mVelocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3 mAcceleration = XMFLOAT3(30.0f, 0.0f, 30.0f);

	float mJumpForce = 10.0f;
	float mGravity = 10.0f;
	float mMaxWalkVelocityXZ = 3.0f;
	float mMaxRunVelocityXZ = 6.0f;
	bool mIsRun = false;
	bool mIsFalling = false;
	float mMaxVelocityY = 10.0f;
	float mFriction = 10.0f;
	
	float mZoomFactor = 100.f;

	CameraMode mMode = THIRD_PERSON;

	Camera* mCamera = nullptr;
	XMFLOAT3 mCameraOffsetPosition = XMFLOAT3(0.0f, 0.0f, 0.0f);

	float mAnimationTime = 0.0f;

	PlayerState* mCurrentState = nullptr;
	UINT mAnimationIndex[(UINT)StateId::Count];

	ComPtr<IXAudio2> m_pXAudio2;
	IXAudio2MasteringVoice* m_pMasteringVoice;
	bool isPlayingW = false;
	bool isPlayingA = false;
	bool isPlayingS = false;
	bool isPlayingD = false;
};

class PlayerStateIdle : public PlayerState
{
public:
	PlayerStateIdle() { mId = StateId::Idle; }

	void Enter(Player& player) override {}
	void Update(Player& player, const float deltaTime) override {}
	void Exit(Player& player) override {}
};

class PlayerStateWalk : public PlayerState
{
public:
	PlayerStateWalk() { mId = StateId::Walk; }

	void Enter(Player& player) override {}
	void Update(Player& player, const float deltaTime) override {}
	void Exit(Player& player) override {}
};

class PlayerStateRun : public PlayerState
{
public:
	PlayerStateRun() { mId = StateId::Run; }

	void Enter(Player& player) override {}
	void Update(Player& player, const float deltaTime) override {}
	void Exit(Player& player) override {}
};

class PlayerStateLand : public PlayerState
{
public:
	PlayerStateLand() { mId = StateId::Land; }

	virtual void Enter(Player& player) override
	{
		player.StopAllSounds();
		// 소리를 재생하는 스레드 시작
		std::thread soundThread([&player]() {
			player.PlaySoundThread(L"Sound/landingSound.wav");
			});
		soundThread.detach(); // 스레드를 떼어내어 메인 스레드와 독립적으로 실행
		player.SetAnimationTime();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	virtual void Update(Player& player, const float deltaTime) override
	{
		if (player.GetAnimationTime() > 0.85f) {
			player.SetAnimationTime();
			player.ChangeState(new PlayerStateIdle);
		}
	}
	virtual void Exit(Player& player) override { }
};

class PlayerStateFall : public PlayerState
{
public:
	PlayerStateFall() { mId = StateId::Fall; }

	void Enter(Player& player) override {}
	void Update(Player& player, const float deltaTime) override
	{
		if (!player.IsFalling()) {
			player.SetAnimationTime();
			player.ChangeState(new PlayerStateLand);
		}
	}
	void Exit(Player& player) override {}
};

class PlayerStateJump : public PlayerState
{
public:
	PlayerStateJump() { mId = StateId::Jump; }

	virtual void Enter(Player& player) override
	{
		player.StopAllSounds();
		// 소리를 재생하는 스레드 시작
		std::thread soundThread([&player]() {
			player.PlaySoundThread(L"Sound/JumpSound.wav");
			});
		soundThread.detach(); // 스레드를 떼어내어 메인 스레드와 독립적으로 실행
		player.SetAnimationTime();		
		// 스레드가 실행 중일 때까지 대기
		if (soundThread.joinable())
			soundThread.join();
	}
	virtual void Update(Player& player, const float deltaTime) override
	{
		if (player.GetAnimationTime() > 0.85f) {
			player.SetAnimationTime();
			player.ChangeState(new PlayerStateFall);
		}
	}
	virtual void Exit(Player& player) override { }
};
