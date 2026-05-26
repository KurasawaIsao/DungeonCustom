#pragma once

#include "main.h"

struct UIRect
{
	float x;
	float y;
	float w;
	float h;
};

class UIRenderer
{
public:
	static bool Init();
	static void Uninit();

	static void DrawWindow(const UIRect& rect);
	static void DrawWindow(
		const UIRect& rect,
		const XMFLOAT4& color);
	static void DrawSolidRect(
		const UIRect& rect,
		const XMFLOAT4& color);
	static void DrawTexture(
		ID3D11ShaderResourceView* texture,
		const UIRect& dstRect,
		const UIRect& uvRect,
		const XMFLOAT4& color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));

private:
	static void DrawNineSlice(
		ID3D11ShaderResourceView* texture,
		const UIRect& rect,
		float textureWidth,
		float textureHeight,
		float border,
		const XMFLOAT4& color);

	static ID3D11Buffer* m_VertexBuffer;
	static ID3D11VertexShader* m_VertexShader;
	static ID3D11PixelShader* m_PixelShader;
	static ID3D11InputLayout* m_VertexLayout;
	static ID3D11ShaderResourceView* m_WindowTexture;
};
