#include "main.h"
#include "Polygon.h"
#include "renderer.h"
#include "texture.h"

void Polygon2D::Init(float x, float y, float width, float height, const char* FileName,float alpha)
{
	VERTEX_3D vertex[4]{};

	vertex[0].Position = XMFLOAT3(x ,y, 0.0f);
	vertex[0].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
	vertex[0].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, alpha);
	vertex[0].TexCoord = XMFLOAT2(0.0f, 0.0f);

	vertex[1].Position = XMFLOAT3(x+width, y, 0.0f);
	vertex[1].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
	vertex[1].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, alpha);
	vertex[1].TexCoord = XMFLOAT2(1.0f, 0.0f);

	vertex[2].Position = XMFLOAT3(x, y+height, 0.0f);
	vertex[2].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
	vertex[2].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, alpha);
	vertex[2].TexCoord = XMFLOAT2(0.0f, 1.0f);

	vertex[3].Position = XMFLOAT3(x + width, y + height, 0.0f);
	vertex[3].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
	vertex[3].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, alpha);
	vertex[3].TexCoord = XMFLOAT2(1.0f, 1.0f);

	//頂点バッファ作成
	D3D11_BUFFER_DESC	bd{};//{}を後に書くと自動で初期化してくれる
	/*ZeroMemory(&bd, sizeof(bd));*/
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(VERTEX_3D) * 4;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA sd{};
	sd.pSysMem = vertex;

	/*TexMetadata metadata;
	ScratchImage image;

	CreateShaderResourceView(Renderer::GetDevice(), image.GetImages(), image.GetImageCount(), metadata, &m_texture);
	assert(m_texture);*/
	if (FileName != nullptr)
	{
		m_texture = Texture::Load(FileName);
	}
	else
	{
		m_texture = nullptr;   // SRV は SetTexture でセットする
	}


	Renderer::GetDevice()->CreateBuffer(&bd, &sd, &m_VertexBuffer);

	Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout, "shader\\unlitTextureVS.cso");
	Renderer::CreatePixelShader(&m_PixelShader, "shader\\unlitTexturePS.cso");
}

void Polygon2D::Init()
{
	

}

void Polygon2D::Update()
{

}

void Polygon2D::Draw()
{
	//入力レイアウト
	Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);

	Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);
	// 2D用マトリクス設定
	Renderer::SetWorldViewProjection2D();

	//頂点バッファ設定(GPUメモリに生成した頂点データの設定)
	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, &m_VertexBuffer, &stride, &offset);

	//移動マトリクス設定(座標変換)
	XMMATRIX world, trans, rot, scale;
	scale = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
	rot = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);
	trans = XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
	world = scale * rot * trans;
	Renderer::SetWorldMatrix(world);

	// マテリアル設定：ここでメンバ変数の m_Color を渡す
	// マテリアルの更新
	MATERIAL material{};
	material.Diffuse = m_Color; // SetAlphaなどで変更した値を代入
	material.TextureEnable = (m_texture != nullptr);
	Renderer::SetMaterial(material); // これでシェーダー内の Material.Diffuse が更新される

	// テクスチャ設定
	Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &m_texture);

	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);


	//ポリゴン描画
	Renderer::GetDeviceContext()->Draw(4, 0);



}

void Polygon2D::Uninit()
{
	m_PixelShader->Release();

	
	m_VertexShader->Release();
	m_VertexLayout->Release();
	m_VertexBuffer->Release();
}

