#pragma once

#include "GameObject.h"
#include <string>
class Object3D :public GameObject
{
private:
	ID3D11Buffer* m_VertexBuffer;
	class AnimationModel* m_AnimationModel = nullptr;

	ID3D11VertexShader* m_VertexShader;
	ID3D11PixelShader* m_PixelShader;
	ID3D11InputLayout* m_VertexLayout;

	int m_frame;
	//float m_Angle;
	//XMFLOAT2 m_pos;
public:

	void Init();
	Object3D* Init(const char* FileName);
	void Update();
	void Draw();
	void Uninit();
};