#pragma once
#include "Vector3.h"
#include "main.h"



struct VERTEX_3D
{
	XMFLOAT3 Position;
	XMFLOAT3 Normal;
	XMFLOAT4 Diffuse;
	XMFLOAT2 TexCoord;

	uint32_t BoneIndices[4]{};
	float    BoneWeights[4]{};
};
#define MAX_BONES (256)

struct CONSTANT_BUFFER_BONE
{
	XMMATRIX BoneMatrices[MAX_BONES];
};


struct MATERIAL
{
	XMFLOAT4	Ambient;
	XMFLOAT4	Diffuse;
	XMFLOAT4	Specular;
	XMFLOAT4	Emission;
	float		Shininess;
	BOOL		TextureEnable;
	float		Dummy[2];
};



struct LIGHT
{
	BOOL		Enable;
	BOOL		Dummy[3];//16byte‹«ŠE—p
	XMFLOAT4	Direction;
	XMFLOAT4	Diffuse;
	XMFLOAT4	Ambient;

	XMFLOAT4	Position;
	XMFLOAT4	PointLightParam;

	XMFLOAT4	SkyColor;
	XMFLOAT4	GroundColor;
	XMFLOAT4	GroundNormal;

	XMFLOAT4	Angle;

};



class Renderer
{
private:

	static D3D_FEATURE_LEVEL       m_FeatureLevel;

	static ID3D11Device*           m_Device;
	static ID3D11DeviceContext*    m_DeviceContext;
	static IDXGISwapChain*         m_SwapChain;
	static ID3D11RenderTargetView* m_RenderTargetView;
	static ID3D11DepthStencilView* m_DepthStencilView;

	static ID3D11Buffer*			m_WorldBuffer;
	static ID3D11Buffer*			m_ViewBuffer;
	static ID3D11Buffer*			m_ProjectionBuffer;
	static ID3D11Buffer*			m_MaterialBuffer;
	static ID3D11Buffer*			m_LightBuffer;
	static ID3D11Buffer* m_CameraBuffer;
	static ID3D11Buffer* m_ParameterBuffer;


	static ID3D11DepthStencilState* m_DepthStateEnable;
	static ID3D11DepthStencilState* m_DepthStateDisable;
	static ID3D11DepthStencilState* m_StencilWrite;
	static ID3D11DepthStencilState* m_StencilRead;

	static ID3D11BlendState*		m_BlendState;
	static ID3D11BlendState*	m_BlendStateAdd;
	static ID3D11BlendState*		m_BlendStateATC;
	static ID3D11BlendState* m_BlendStateMask;

	static ID3D11RasterizerState* m_RasterizerStateCullBack;
	static ID3D11RasterizerState* m_RasterizerStateCullNone;

	static ID3D11VertexShader* s_VS;
	static ID3D11PixelShader* s_PS;
	static ID3D11InputLayout* s_Layout;
	static HWND m_hWnd;

	static ID3D11Buffer* m_BoneBuffer;

public:
	static void Init();
	static void Uninit();
	static void Begin();
	static void End();

	static void SetDepthEnable(bool Enable);
	static void SetWorldViewProjection2D();
	static void SetWorldMatrix(XMMATRIX WorldMatrix);
	static void SetViewMatrix(XMMATRIX ViewMatrix);
	static void SetProjectionMatrix(XMMATRIX ProjectionMatrix);
	static void SetBoneMatrices(const XMMATRIX* matrices, UINT count);
	static void SetMaterial(MATERIAL Material);
	static void SetLight(LIGHT Light);
	static void SetParameter(XMFLOAT4 Parameter);
	static void SetWindowHandle(HWND hwnd) { m_hWnd = hwnd; }
	static HWND GetWindowHandle() { return m_hWnd; }

	static ID3D11Device* GetDevice( void ){ return m_Device; }
	static ID3D11DeviceContext* GetDeviceContext( void ){ return m_DeviceContext; }


	static ID3D11Buffer* GetMaterialbuffer() { return m_MaterialBuffer; };

	static void CreateVertexShader(ID3D11VertexShader** VertexShader, ID3D11InputLayout** VertexLayout, const char* FileName);
	static void CreatePixelShader(ID3D11PixelShader** PixelShader, const char* FileName);

	static IDXGISwapChain* GetSwapChain() { return m_SwapChain; };
	static ID3D11RenderTargetView* GetMainRenderTargetView() { return m_RenderTargetView; };

	static void SetCullMode(D3D11_CULL_MODE CullMode);

	static void InitCommonShader();
	static void SetCommonShader();
};

