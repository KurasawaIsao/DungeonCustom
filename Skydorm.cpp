#include "Skydorm.h"
#include "renderer.h"
#include "input.h"
#include "scene.h"
#include "camera.h"
#include"manager.h"

ModelRenderer* Skydorm::m_modelrenderer;
void Skydorm::Load()
{
	m_modelrenderer = new ModelRenderer();
	m_modelrenderer->Load("Asset\\Model\\sky.obj");
}
void Skydorm::Init()
{

	Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout, "shader\\unlitTextureVS.cso");
	Renderer::CreatePixelShader(&m_PixelShader, "shader\\unlitTexturePS.cso");
	m_Position = Vector3(0.0f, 0.0f, 0.0f); // ここで安全に初期化
	m_Scale = Vector3(100.0f, 100.0f, 100.0f);
}


void Skydorm::Update()
{
	Camera* pCamera = Manager::GetScene()->GetGameObject<Camera>();
	m_Position = pCamera->GetPosition();
	//適当に回転
	m_Rotation.y += 0.008f;

	/*Camera* camera=*/
}

void Skydorm::Draw()
{
	//入力レイアウト
	Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);

	Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);


	//移動マトリクス設定(座標変換)
	XMMATRIX world, trans, rot, scale;
	scale = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
	rot = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y + XM_PI, m_Rotation.z);
	trans = XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
	world = scale * rot * trans;

	Renderer::SetWorldMatrix(world);

	m_modelrenderer->Draw();
}

void Skydorm::Uninit()
{
	m_PixelShader->Release();

	m_VertexShader->Release();
	m_VertexLayout->Release();
	//delete m_modelrenderer;
}