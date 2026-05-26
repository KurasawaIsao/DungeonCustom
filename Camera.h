#pragma once
#include "main.h"
#include "GameObject.h"
#include "Vector3.h"
class Camera :public GameObject
{
private:
	XMMATRIX m_Projection;
	XMMATRIX m_View;
	// true: Player を追従、false: m_TargetO などの固定注視を使う。
	bool m_SetPlayer;
	// Player 以外の対象を注視するモード。Title 画面の演出カメラで使う。
	bool m_SetOtherObj;


	float m_TitleTimer = 0.0f;
	float m_TitleDuration = 3.0f; // 演出時間（秒）


	Vector3 m_Target{ 0.0f,0.0f,0.0f };
	// Other target の略。m_SetOtherObj=true の時にカメラが見る固定対象。
	Vector3 m_TargetO;

	Vector3 m_Pivot;   // 見下ろす中心点（自由移動）
	float   m_Distance = 10.0f;


	//タイトル演出用
	bool m_IsTitleCamera = false;

	// Camera.h
	float m_TitlePitch = -10.0f;
	float m_TitlePitchMin = -70.0f;
	float m_TitlePitchMax = 25.0f;

	float m_TitleStartPitch;
	float m_TitleEndPitch;

	float m_TitleYaw = 0.0f; // 今回は固定でOK（左右振りたければ使う）

	float m_TitleLookYOffset = -0.4f;  // 初期：少し見上げ
	float m_TitleLookYOffsetMin = -1.5f;
	float m_TitleLookYOffsetMax = 0.5f;


	Vector3 m_TitleStartPos;
	Vector3 m_TitleEndPos;

	Vector3 m_TitleTarget;

	Vector3 m_TitleForward;

public:
	void Init();
	void Update();
	void Draw();
	void Uninit();
	Vector3 GetForward()override
	{
		Vector3 forward = m_Target - m_Position;
		forward.normalize();
		return forward;
	}
	XMMATRIX GetViewMatrix() { return m_View; };
	XMMATRIX GetProjectionMatrix() { return m_Projection; };
	void SetPlayerEnable(bool s) 
	{
		m_SetPlayer = s;
	}
	void SetOtherObjEnable(bool s)
	{
		m_SetOtherObj = s;
	}
	void SetPivot(const Vector3& pivot) {
		m_Pivot = pivot;
		m_Target = pivot; 
	}
	void StartTitleCamera(
		const Vector3& startPos,
		const Vector3& endPos,
		float startPitch,
		float endPitch,
		float duration);
	void UpdateTitleCamera();


	void SetTarget(Vector3 target) { m_TargetO = target; }
};