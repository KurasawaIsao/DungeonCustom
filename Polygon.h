#pragma once
#include "main.h"
#include "GameObject.h"
class Polygon2D :public GameObject
{
private:	
	ID3D11Buffer* m_VertexBuffer;

	ID3D11VertexShader* m_VertexShader;
	ID3D11PixelShader* m_PixelShader;
	ID3D11InputLayout* m_VertexLayout;
	ID3D11ShaderResourceView* m_texture;

	XMFLOAT4 m_Color = { 1.0f, 1.0f, 1.0f, 1.0f }; // ’Ç‰Á

public:
	void Init(float x,float y, float width,float height,const char* FileName, float alpha = 1.0f);
	void Init();
	void Update();
	void Draw();
	void Uninit();
	void SetTexture(ID3D11ShaderResourceView* tex)
	{
		m_texture = tex;
	}
	ID3D11ShaderResourceView* GetTexture() { return m_texture; };

	void SetColor(XMFLOAT4 color) { m_Color = color; }
	void SetAlpha(float alpha) { m_Color.w = alpha; }

};