#include "player.h"
#include "renderer.h"
#include "input.h"
#include "camera.h"
#include"manager.h"
#include "animationModel.h"
#include "Easing.h"
#include "Object3D.h"

void Object3D::Init()
{


}
Object3D* Object3D::Init(const char* FileName)
{
	m_AnimationModel = new AnimationModel();
	m_AnimationModel->Load(FileName);
	//"model\\box.obj"
	Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout, "shader\\unlitTextureVS.cso");
	Renderer::CreatePixelShader(&m_PixelShader, "shader\\unlitTexturePS.cso");

	m_Position = { 0.0f,1.0f,0.0f };
	m_Scale = { 0.5f,0.5f,0.5f };
	return this;
}

void Object3D::Update()
{
}

void Object3D::Draw()
{

	//入力レイアウト
	Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);

	Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);

	XMMATRIX ParentMatrix;

	//移動マトリクス設定(親)
	//{}をわざわざ書くのは寿命の設定をするため(同じ名前でそれぞれ違う役割をさせたい時に)
	{
		XMMATRIX world, trans, rot, scale;
		scale = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
		rot = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);
		trans = XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
		world = scale * rot * trans;
		ParentMatrix = world;

		Renderer::SetWorldMatrix(world);

		m_AnimationModel->Draw();
	}
}

void Object3D::Uninit()
{


	m_PixelShader->Release();

	m_VertexShader->Release();
	m_VertexLayout->Release();
	delete m_AnimationModel;

}