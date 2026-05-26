#include "main.h"
#include "modelRenderer.h"
#include "GameObject.h"
class Skydorm :public GameObject
{
private:
	ID3D11VertexShader* m_VertexShader;
	ID3D11PixelShader* m_PixelShader;
	ID3D11InputLayout* m_VertexLayout;
	static ModelRenderer* m_modelrenderer;

public:
	static void Load();

	void Init();
	void Update();
	void Draw();
	void Uninit();
};