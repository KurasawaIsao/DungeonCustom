#include "camera.h"
#include"renderer.h"
#include "manager.h"
#include "player.h"
#include "input.h"
#include "scene.h"
void Camera::Init()
{
	m_Rotation = { -90.0f,0.0f,0.0f };
	Player* player = Manager::GetScene()->GetGameObject<Player>();
	if (player)
	{

		m_Pivot = player->GetPosition();
	}
}

void Camera::Update()
{
	
	if (m_IsTitleCamera)
	{
		UpdateTitleCamera();
		
		return;
	}
	if (m_SetPlayer)
	{
	}
	else {
		// --- エディタ用：斜め見下ろし  直線移動 ---
		float speed = 0.8f;

		// 入力に対してワールド座標の軸をそのまま動かす
		if (Input::GetKeyPress(VK_UP))    m_Pivot.z += speed;
		if (Input::GetKeyPress(VK_DOWN))  m_Pivot.z -= speed;
		if (Input::GetKeyPress(VK_LEFT))  m_Pivot.x -= speed;
		if (Input::GetKeyPress(VK_RIGHT)) m_Pivot.x += speed;

		// 高度（ズーム）調整
		if (Input::GetKeyPress(VK_TAB)) m_Distance = std::max(5.0f, m_Distance - speed);
		if (Input::GetKeyPress(VK_CONTROL)) m_Distance = std::min(100.0f, m_Distance + speed);

		// 注視点は移動させたPivot（地面）
		m_Target = m_Pivot;
		m_Position = m_Pivot + Vector3(0.0f, m_Distance * 0.8f, -m_Distance * 0.6f);

		m_Rotation.x = XMConvertToRadians(-55.0f);
		m_Rotation.y = 0.0f;
	}


	
	if (m_SetPlayer)
	{
		Player* player = Manager::GetScene()->GetGameObject<Player>();
		m_Target = player->GetPosition() + Vector3(0.0f, 0.0f, 0.0f);//ちょっと上にずらす

		m_Position = m_Target + Vector3(-sinf(m_Rotation.y), -sinf(m_Rotation.x) * 1.5f, -cosf(m_Rotation.y)) * 10.0f;
	}
	else if (m_SetOtherObj && !m_IsTitleCamera)
	{
		m_Target = m_TargetO;
		m_Position = m_Target + Vector3(-sinf(m_Rotation.y), -sinf(m_Rotation.x) * 1.5f, -cosf(m_Rotation.y)) * 10.0f;
	}
	
	
}

void Camera::Draw()
{
	//プロジェクション行列を作成
	m_Projection = XMMatrixPerspectiveFovLH(1.0f,(float)SCREEN_WIDTH / SCREEN_HEIGHT,1.0f,1000.0f);
	
	//DirectXへセット
	Renderer::SetProjectionMatrix(m_Projection);

	if (m_IsTitleCamera)
	{
		XMVECTOR pos = XMLoadFloat3((XMFLOAT3*)&m_Position);
		XMVECTOR forward = XMVectorSet(
			m_TitleForward.x,
			m_TitleForward.y,
			m_TitleForward.z,
			0.0f
		);
		XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

		m_View = XMMatrixLookToLH(pos, forward, up);
	}
	else if (m_SetOtherObj&& !m_IsTitleCamera)
	{

		XMFLOAT3 up = { 0.0f, 1.0f, 0.0f };
		m_View = XMMatrixLookAtLH(
			XMLoadFloat3((XMFLOAT3*)&m_Position),
			XMLoadFloat3((XMFLOAT3*)&m_Target),
			XMLoadFloat3(&up)
		);
	}
	else
	{
		XMFLOAT3 up = { 0.0f, 1.0f, 0.0f };
		m_View = XMMatrixLookAtLH(
			XMLoadFloat3((XMFLOAT3*)&m_Position),
			XMLoadFloat3((XMFLOAT3*)&m_Target),
			XMLoadFloat3(&up)
		);
	}

	//行列をセット
	Renderer::SetViewMatrix(m_View);

	/*Renderer::SetCameraPosition(m_Position);*/
}

void Camera::Uninit()
{
}

void Camera::StartTitleCamera(
	const Vector3& startPos,
	const Vector3& endPos,
	float startPitch,
	float endPitch,
	float duration)
{
	m_IsTitleCamera = true;

	m_TitleStartPos = startPos;
	m_TitleEndPos = endPos;

	m_TitleStartPitch = startPitch;
	m_TitleEndPitch = endPitch;
	m_TitlePitch = startPitch;

	m_TitleDuration = duration;
	m_TitleTimer = 0.0f;

	m_Position = startPos;

	// ★ ここを追加（超重要）
	float rad = XMConvertToRadians(startPitch);
	m_TitleForward = Vector3(
		0.0f,
		sinf(rad),
		-cosf(rad)
	);
	m_TitleForward.normalize();
}



void Camera::UpdateTitleCamera()
{
	

	m_TitleTimer += 1.0f / 60.0f;
	float t = m_TitleTimer / m_TitleDuration;
	t = Vector3::Clamp(t, 0.0f, 1.0f);

	// （演出向け）イージング
	float easeT = t * t * (3.0f - 2.0f * t);

	// ① 位置補間
	m_Position = Vector3::Lerp(
		m_TitleStartPos,
		m_TitleEndPos,
		easeT
	);

	// ② 視線角度補間
	m_TitlePitch = Vector3::Lerp(
		m_TitleStartPitch,
		m_TitleEndPitch,
		easeT
	);

	// ③ Forward 生成
	float pitchRad = XMConvertToRadians(m_TitlePitch);

	Vector3 forward(
		0.0f,
		sinf(pitchRad),
		-cosf(pitchRad)
	);

	forward.normalize();
	m_TitleForward = forward;
}
