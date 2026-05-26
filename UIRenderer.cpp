#include "UIRenderer.h"
#include "Renderer.h"
#include "Texture.h"

#include <algorithm>
#include <cstring>

ID3D11Buffer* UIRenderer::m_VertexBuffer = nullptr;
ID3D11VertexShader* UIRenderer::m_VertexShader = nullptr;
ID3D11PixelShader* UIRenderer::m_PixelShader = nullptr;
ID3D11InputLayout* UIRenderer::m_VertexLayout = nullptr;
ID3D11ShaderResourceView* UIRenderer::m_WindowTexture = nullptr;

bool UIRenderer::Init()
{
	D3D11_BUFFER_DESC bufferDesc{};
	bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufferDesc.ByteWidth = sizeof(VERTEX_3D) * 4;
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	HRESULT hr = Renderer::GetDevice()->CreateBuffer(&bufferDesc, nullptr, &m_VertexBuffer);
	if (FAILED(hr)) return false;

	Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout, "shader\\unlitTextureVS.cso");
	Renderer::CreatePixelShader(&m_PixelShader, "shader\\unlitTexturePS.cso");

	m_WindowTexture = Texture::Load("Asset\\Texture\\Window.png");
	return m_VertexShader && m_PixelShader && m_VertexLayout && m_WindowTexture;
}

void UIRenderer::Uninit()
{
	if (m_VertexLayout) m_VertexLayout->Release();
	if (m_PixelShader) m_PixelShader->Release();
	if (m_VertexShader) m_VertexShader->Release();
	if (m_VertexBuffer) m_VertexBuffer->Release();

	m_VertexLayout = nullptr;
	m_PixelShader = nullptr;
	m_VertexShader = nullptr;
	m_VertexBuffer = nullptr;
	m_WindowTexture = nullptr;
}

void UIRenderer::DrawWindow(const UIRect& rect)
{
	DrawWindow(rect, XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
}

void UIRenderer::DrawWindow(
	const UIRect& rect,
	const XMFLOAT4& color)
{
	DrawNineSlice(m_WindowTexture, rect, 128.0f, 128.0f, 16.0f, color);
}

void UIRenderer::DrawSolidRect(
	const UIRect& rect,
	const XMFLOAT4& color)
{
	if (!m_VertexBuffer) return;

	VERTEX_3D vertex[4]{};
	const float left = rect.x;
	const float right = rect.x + rect.w;
	const float top = rect.y;
	const float bottom = rect.y + rect.h;

	vertex[0].Position = XMFLOAT3(left, top, 0.0f);
	vertex[0].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
	vertex[0].Diffuse = color;
	vertex[0].TexCoord = XMFLOAT2(0.0f, 0.0f);

	vertex[1].Position = XMFLOAT3(right, top, 0.0f);
	vertex[1].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
	vertex[1].Diffuse = color;
	vertex[1].TexCoord = XMFLOAT2(1.0f, 0.0f);

	vertex[2].Position = XMFLOAT3(left, bottom, 0.0f);
	vertex[2].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
	vertex[2].Diffuse = color;
	vertex[2].TexCoord = XMFLOAT2(0.0f, 1.0f);

	vertex[3].Position = XMFLOAT3(right, bottom, 0.0f);
	vertex[3].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
	vertex[3].Diffuse = color;
	vertex[3].TexCoord = XMFLOAT2(1.0f, 1.0f);

	D3D11_MAPPED_SUBRESOURCE mapped{};
	HRESULT hr = Renderer::GetDeviceContext()->Map(m_VertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	if (FAILED(hr)) return;

	memcpy(mapped.pData, vertex, sizeof(vertex));
	Renderer::GetDeviceContext()->Unmap(m_VertexBuffer, 0);

	Renderer::SetDepthEnable(false);
	Renderer::SetWorldViewProjection2D();

	Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);
	Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, nullptr, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, nullptr, 0);

	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, &m_VertexBuffer, &stride, &offset);

	Renderer::SetWorldMatrix(XMMatrixIdentity());

	MATERIAL material{};
	material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	material.TextureEnable = FALSE;
	Renderer::SetMaterial(material);

	ID3D11ShaderResourceView* nullTexture = nullptr;
	Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &nullTexture);
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	Renderer::GetDeviceContext()->Draw(4, 0);

	Renderer::SetDepthEnable(true);
}

void UIRenderer::DrawTexture(
	ID3D11ShaderResourceView* texture,
	const UIRect& dstRect,
	const UIRect& uvRect,
	const XMFLOAT4& color)
{
	if (!texture || !m_VertexBuffer) return;

	VERTEX_3D vertex[4]{};
	const float left = dstRect.x;
	const float right = dstRect.x + dstRect.w;
	const float top = dstRect.y;
	const float bottom = dstRect.y + dstRect.h;
	const float u0 = uvRect.x;
	const float v0 = uvRect.y;
	const float u1 = uvRect.x + uvRect.w;
	const float v1 = uvRect.y + uvRect.h;

	vertex[0].Position = XMFLOAT3(left, top, 0.0f);
	vertex[0].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
	vertex[0].Diffuse = color;
	vertex[0].TexCoord = XMFLOAT2(u0, v0);

	vertex[1].Position = XMFLOAT3(right, top, 0.0f);
	vertex[1].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
	vertex[1].Diffuse = color;
	vertex[1].TexCoord = XMFLOAT2(u1, v0);

	vertex[2].Position = XMFLOAT3(left, bottom, 0.0f);
	vertex[2].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
	vertex[2].Diffuse = color;
	vertex[2].TexCoord = XMFLOAT2(u0, v1);

	vertex[3].Position = XMFLOAT3(right, bottom, 0.0f);
	vertex[3].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
	vertex[3].Diffuse = color;
	vertex[3].TexCoord = XMFLOAT2(u1, v1);

	D3D11_MAPPED_SUBRESOURCE mapped{};
	HRESULT hr = Renderer::GetDeviceContext()->Map(m_VertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	if (FAILED(hr)) return;

	memcpy(mapped.pData, vertex, sizeof(vertex));
	Renderer::GetDeviceContext()->Unmap(m_VertexBuffer, 0);

	Renderer::SetDepthEnable(false);
	Renderer::SetWorldViewProjection2D();

	Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);
	Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, nullptr, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, nullptr, 0);

	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, &m_VertexBuffer, &stride, &offset);

	Renderer::SetWorldMatrix(XMMatrixIdentity());

	MATERIAL material{};
	material.Diffuse = color;
	material.TextureEnable = TRUE;
	Renderer::SetMaterial(material);

	Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &texture);
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	Renderer::GetDeviceContext()->Draw(4, 0);

	Renderer::SetDepthEnable(true);
}

// ウィンドウの9分割り描画。borderはテクスチャのうち、角からどれだけの範囲を引き伸ばさずに描画するか。
void UIRenderer::DrawNineSlice(
	ID3D11ShaderResourceView* texture,
	const UIRect& rect,
	float textureWidth,
	float textureHeight,
	float border,
	const XMFLOAT4& color)
{
	if (!texture) return;

	const float dstBorderX = (std::min)(border, rect.w * 0.5f);
	const float dstBorderY = (std::min)(border, rect.h * 0.5f);
	const float uBorder = border / textureWidth;
	const float vBorder = border / textureHeight;

	float dx[4] = { rect.x, rect.x + dstBorderX, rect.x + rect.w - dstBorderX, rect.x + rect.w };
	float dy[4] = { rect.y, rect.y + dstBorderY, rect.y + rect.h - dstBorderY, rect.y + rect.h };
	float du[4] = { 0.0f, uBorder, 1.0f - uBorder, 1.0f };
	float dv[4] = { 0.0f, vBorder, 1.0f - vBorder, 1.0f };

	for (int y = 0; y < 3; ++y)
	{
		for (int x = 0; x < 3; ++x)
		{
			UIRect dst{ dx[x], dy[y], dx[x + 1] - dx[x], dy[y + 1] - dy[y] };
			UIRect uv{ du[x], dv[y], du[x + 1] - du[x], dv[y + 1] - dv[y] };
			DrawTexture(texture, dst, uv, color);
		}
	}
}
