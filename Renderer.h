#pragma once
#include "Vector3.h"
#include "main.h"



struct VERTEX_3D
{
	// 1頂点が持つ基本情報。入力レイアウトと同じ順序で定義する。
	XMFLOAT3 Position;
	XMFLOAT3 Normal;
	XMFLOAT4 Diffuse;
	XMFLOAT2 TexCoord;

	// スキニング時に参照するボーン番号と、それぞれの影響度。
	uint32_t BoneIndices[4]{};
	float    BoneWeights[4]{};
};
#define MAX_BONES (256)

struct CONSTANT_BUFFER_BONE
{
	// 頂点シェーダーへ送る全ボーンの変換行列。
	XMMATRIX BoneMatrices[MAX_BONES];
};


struct MATERIAL
{
	// モデル表面の色や反射特性をシェーダーへ渡す。
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
	// 平行光源、点光源、半球ライトに必要な情報をまとめて保持する。
	BOOL		Enable;
	BOOL		Dummy[3];// 定数バッファの16バイト境界に合わせる。
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

	// 実際に作成されたDirect3Dデバイスの機能レベル。
	static D3D_FEATURE_LEVEL       m_FeatureLevel;

	// Direct3Dの基本オブジェクトと、画面へ描画するための出力先。
	static ID3D11Device*           m_Device;
	static ID3D11DeviceContext*    m_DeviceContext;
	static IDXGISwapChain*         m_SwapChain;
	static ID3D11RenderTargetView* m_RenderTargetView;
	static ID3D11DepthStencilView* m_DepthStencilView;

	// CPU側の描画情報をシェーダーへ渡す定数バッファ。
	static ID3D11Buffer*			m_WorldBuffer;
	static ID3D11Buffer*			m_ViewBuffer;
	static ID3D11Buffer*			m_ProjectionBuffer;
	static ID3D11Buffer*			m_MaterialBuffer;
	static ID3D11Buffer*			m_LightBuffer;
	static ID3D11Buffer* m_CameraBuffer;
	static ID3D11Buffer* m_ParameterBuffer;


	// 奥行き判定、奥行き書き込み、ステンシル処理を切り替えるステート。
	static ID3D11DepthStencilState* m_DepthStateEnable;
	static ID3D11DepthStencilState* m_DepthStateDisable;
	static ID3D11DepthStencilState* m_StencilWrite;
	static ID3D11DepthStencilState* m_StencilRead;

	// 通常の半透明、加算、Alpha To Coverage、色書き込み禁止用のブレンドステート。
	static ID3D11BlendState*		m_BlendState;
	static ID3D11BlendState*	m_BlendStateAdd;
	static ID3D11BlendState*		m_BlendStateATC;
	static ID3D11BlendState* m_BlendStateMask;

	// ポリゴン裏面を除外する場合と、両面を描画する場合のラスタライザステート。
	static ID3D11RasterizerState* m_RasterizerStateCullBack;
	static ID3D11RasterizerState* m_RasterizerStateCullNone;

	// 多くの3D描画で共有するシェーダーと頂点入力レイアウト。
	static ID3D11VertexShader* m_VS;
	static ID3D11PixelShader* m_PS;
	static ID3D11InputLayout* m_Layout;

	// 描画対象となるアプリケーションウィンドウのハンドル。
	static HWND m_hWnd;

	// スキニング用のボーン行列を保持する定数バッファ。
	static ID3D11Buffer* m_BoneBuffer;

public:
	// Direct3Dの各リソースを生成・解放する。
	static void Init();
	static void Uninit();

	// 1フレームの描画開始処理と、完成した画面の表示処理。
	static void Begin();
	static void End();

	// 描画パイプラインの各状態や、シェーダーへ渡す値を更新する。
	static void SetDepthEnable(bool Enable);
	static void SetWorldViewProjection2D();
	static void SetWorldMatrix(XMMATRIX WorldMatrix);
	static void SetViewMatrix(XMMATRIX ViewMatrix);
	static void SetProjectionMatrix(XMMATRIX ProjectionMatrix);
	static void SetCameraPosition(const Vector3& Position);
	static void SetMaterial(MATERIAL Material);
	static void SetLight(LIGHT Light);
	static void SetParameter(XMFLOAT4 Parameter);

	// 外部システムと共有するウィンドウおよびDirect3Dオブジェクトを取得する。
	static void SetWindowHandle(HWND hwnd) { m_hWnd = hwnd; }
	static HWND GetWindowHandle() { return m_hWnd; }

	static ID3D11Device* GetDevice( void ){ return m_Device; }
	static ID3D11DeviceContext* GetDeviceContext( void ){ return m_DeviceContext; }

	// コンパイル済みシェーダーファイルからGPU用シェーダーを生成する。
	static void CreateVertexShader(ID3D11VertexShader** VertexShader, ID3D11InputLayout** VertexLayout, const char* FileName);
	static void CreatePixelShader(ID3D11PixelShader** PixelShader, const char* FileName);

	// 別の描画処理がメイン画面へアクセスするときに使用する。
	static IDXGISwapChain* GetSwapChain() { return m_SwapChain; };
	static ID3D11RenderTargetView* GetMainRenderTargetView() { return m_RenderTargetView; };

	// 指定した面を描画対象から除外するカリングモードを設定する。
	static void SetCullMode(D3D11_CULL_MODE CullMode);

	// 共通のBlinn-Phongシェーダーを生成し、描画パイプラインへ設定する。
	static void InitCommonShader();
	static void SetCommonShader();
};

