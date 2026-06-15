
#include "main.h"
#include "renderer.h"
#include <io.h>


// Rendererは静的クラスとして扱い、アプリケーション全体で1組の描画環境を共有する。
D3D_FEATURE_LEVEL       Renderer::m_FeatureLevel = D3D_FEATURE_LEVEL_11_0;

// Direct3D本体と、ウィンドウへ最終結果を表示するためのオブジェクト。
ID3D11Device*           Renderer::m_Device{};
ID3D11DeviceContext*    Renderer::m_DeviceContext{};
IDXGISwapChain*         Renderer::m_SwapChain{};
ID3D11RenderTargetView* Renderer::m_RenderTargetView{};
ID3D11DepthStencilView* Renderer::m_DepthStencilView{};

// 各シェーダーへ描画情報を渡す定数バッファ。
ID3D11Buffer*			Renderer::m_WorldBuffer{};
ID3D11Buffer*			Renderer::m_ViewBuffer{};
ID3D11Buffer*			Renderer::m_ProjectionBuffer{};
ID3D11Buffer*			Renderer::m_MaterialBuffer{};
ID3D11Buffer*			Renderer::m_LightBuffer{};
ID3D11Buffer* Renderer::m_CameraBuffer{};
ID3D11Buffer* Renderer::m_ParameterBuffer{};

// 奥行き判定とステンシル処理の設定を保持する。
ID3D11DepthStencilState* Renderer::m_DepthStateEnable{};
ID3D11DepthStencilState* Renderer::m_DepthStateDisable{};

ID3D11DepthStencilState* Renderer::m_StencilWrite;
ID3D11DepthStencilState* Renderer::m_StencilRead;

// 色の合成方法と、レンダーターゲットへの書き込み方法を保持する。
ID3D11BlendState*		Renderer::m_BlendState{};
ID3D11BlendState*		Renderer::m_BlendStateATC{};
ID3D11BlendState* Renderer::m_BlendStateAdd{};
ID3D11BlendState* Renderer::m_BlendStateMask{};

// ポリゴンの裏面を描画するかどうかを制御する。
ID3D11RasterizerState* Renderer::m_RasterizerStateCullBack;
ID3D11RasterizerState* Renderer::m_RasterizerStateCullNone;

// 3Dモデル描画で共通利用するBlinn-Phongシェーダー。
ID3D11VertexShader* Renderer::m_VS = nullptr;
ID3D11PixelShader* Renderer::m_PS = nullptr;
ID3D11InputLayout* Renderer::m_Layout = nullptr;

// アニメーションモデルのスキニングに使用するボーン行列バッファ。
ID3D11Buffer* Renderer::m_BoneBuffer = nullptr;

// Direct3Dが描画する対象ウィンドウ。
HWND  Renderer::m_hWnd;

void Renderer::Init()
{
	// Direct3Dの作成結果を受け取る。必要に応じて失敗判定へ利用できる。
	HRESULT hr = S_OK;




	// GPUリソースを生成するデバイスと、完成した画面を表示するスワップチェーンを作成する。
	DXGI_SWAP_CHAIN_DESC swapChainDesc{};
	// ダブルバッファのうち、DirectX側で保持するバックバッファを1枚にする。
	swapChainDesc.BufferCount = 1;
	swapChainDesc.BufferDesc.Width = SCREEN_WIDTH;
	swapChainDesc.BufferDesc.Height = SCREEN_HEIGHT;
	// Direct2Dなどとの連携にも対応できるBGRA形式を使用する。
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
	swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.OutputWindow = Application::GetWindow();
	// 現在はMSAAを使用せず、1ピクセルにつき1サンプルで描画する。
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.Windowed = TRUE;

	// BGRA形式のサーフェスを利用できるようにしてデバイスを作成する。
	UINT createDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

	hr = D3D11CreateDeviceAndSwapChain( NULL,
										D3D_DRIVER_TYPE_HARDWARE,
										NULL,
										createDeviceFlags,
										NULL,
										0,
										D3D11_SDK_VERSION,
										&swapChainDesc,
										&m_SwapChain,
										&m_Device,
										&m_FeatureLevel,
										&m_DeviceContext );






	// スワップチェーンのバックバッファを取得し、色を書き込める描画先として登録する。
	ID3D11Texture2D* renderTarget{};
	m_SwapChain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), ( LPVOID* )&renderTarget );
	m_Device->CreateRenderTargetView( renderTarget, NULL, &m_RenderTargetView );
	// ビュー側がバックバッファを参照するため、取得時の参照だけを解放する。
	renderTarget->Release();


	// 24ビットの奥行き値と8ビットのステンシル値を保持するテクスチャを作成する。
	ID3D11Texture2D* depthStencile{};
	D3D11_TEXTURE2D_DESC textureDesc{};
	textureDesc.Width = swapChainDesc.BufferDesc.Width;
	textureDesc.Height = swapChainDesc.BufferDesc.Height;
	textureDesc.MipLevels = 1;
	textureDesc.ArraySize = 1;
	textureDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	textureDesc.SampleDesc = swapChainDesc.SampleDesc;
	textureDesc.Usage = D3D11_USAGE_DEFAULT;
	textureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	textureDesc.CPUAccessFlags = 0;
	textureDesc.MiscFlags = 0;
	m_Device->CreateTexture2D(&textureDesc, NULL, &depthStencile);

	// 作成したテクスチャを、深度・ステンシルの描画先として利用できる形にする。
	D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc{};
	depthStencilViewDesc.Format = textureDesc.Format;
	depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depthStencilViewDesc.Flags = 0;
	m_Device->CreateDepthStencilView(depthStencile, &depthStencilViewDesc, &m_DepthStencilView);
	depthStencile->Release();

	// 出力マージャーへ色と深度の描画先を同時に設定する。
	m_DeviceContext->OMSetRenderTargets(1, &m_RenderTargetView, m_DepthStencilView);





	// 描画結果をウィンドウ全体へ対応付けるビューポートを設定する。
	D3D11_VIEWPORT viewport;
	viewport.Width = (FLOAT)SCREEN_WIDTH;
	viewport.Height = (FLOAT)SCREEN_HEIGHT;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	m_DeviceContext->RSSetViewports( 1, &viewport );



	// ポリゴンを塗りつぶし、画面外の深度を切り捨てるラスタライザ設定を作成する。
	D3D11_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	rasterizerDesc.DepthClipEnable = TRUE;
	rasterizerDesc.MultisampleEnable = FALSE;

	// 通常の3D描画用として裏面を除外するステートを作成する。
	rasterizerDesc.CullMode = D3D11_CULL_BACK;
	m_Device->CreateRasterizerState(&rasterizerDesc, &m_RasterizerStateCullBack);

	// 板ポリゴンなどを両面表示するため、カリングしないステートも作成する。
	rasterizerDesc.CullMode = D3D11_CULL_NONE;
	m_Device->CreateRasterizerState(&rasterizerDesc, &m_RasterizerStateCullNone);

	// 初期状態ではポリゴンの裏面を描画しない。
	m_DeviceContext->RSSetState(m_RasterizerStateCullBack);



	// 描画元のアルファ値を使って背景色と合成する、通常の半透明設定を作成する。
	D3D11_BLEND_DESC blendDesc{};
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;


	m_Device->CreateBlendState(&blendDesc, &m_BlendState);

	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;

	// 色を書き込まず、深度やステンシルなど別の情報だけを更新するための設定。
	blendDesc.RenderTarget[0].RenderTargetWriteMask = 0;
	m_Device->CreateBlendState(&blendDesc, &m_BlendStateMask);
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	// アルファ値をMSAAのサンプル範囲へ変換する設定も用意する。
	blendDesc.AlphaToCoverageEnable = TRUE;
	m_Device->CreateBlendState(&blendDesc, &m_BlendStateATC);

	// 初期状態では通常のアルファブレンドを使用し、全サンプルへ書き込む。
	float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	m_DeviceContext->OMSetBlendState(m_BlendState, blendFactor, 0xffffffff);



	// 手前のピクセルを優先し、描画した奥行きを深度バッファへ保存する設定。
	D3D11_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	depthStencilDesc.StencilEnable = FALSE;

	m_Device->CreateDepthStencilState(&depthStencilDesc, &m_DepthStateEnable);//深度有効ステート

	// 深度比較は残したまま、UIなどが深度バッファを書き換えない設定。
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	m_Device->CreateDepthStencilState(&depthStencilDesc, &m_DepthStateDisable);//深度書き込み無効ステート

	// 初期状態では深度比較と深度書き込みの両方を有効にする。
	m_DeviceContext->OMSetDepthStencilState(m_DepthStateEnable, NULL);

	// ポリゴンの表面と裏面の通過回数をステンシルへ記録する設定。
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

	depthStencilDesc.StencilEnable = TRUE;
	depthStencilDesc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	depthStencilDesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;

	depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_INCR;
	depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	depthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_DECR;
	depthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	m_Device->CreateDepthStencilState(&depthStencilDesc, &m_StencilWrite);

	// 記録済みのステンシル値を比較し、条件を満たす領域だけ描画する設定。
	depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_LESS;

	m_Device->CreateDepthStencilState(&depthStencilDesc, &m_StencilRead);

	// テクスチャを斜めから見た場合のぼやけを抑える異方性フィルタを設定する。
	D3D11_SAMPLER_DESC samplerDesc{};
	samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.MaxAnisotropy = 4;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	ID3D11SamplerState* samplerState{};
	m_Device->CreateSamplerState( &samplerDesc, &samplerState );

	// ピクセルシェーダーのサンプラースロットs0へ設定する。
	m_DeviceContext->PSSetSamplers( 0, 1, &samplerState );



	// 行列を頂点シェーダーへ渡すため、4x4行列1個分の定数バッファを作成する。
	D3D11_BUFFER_DESC bufferDesc{};
	bufferDesc.ByteWidth = sizeof(XMFLOAT4X4);
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	bufferDesc.StructureByteStride = sizeof(float);

	// 頂点シェーダーのb0へ、モデルごとのワールド行列を接続する。
	m_Device->CreateBuffer( &bufferDesc, NULL, &m_WorldBuffer );
	m_DeviceContext->VSSetConstantBuffers( 0, 1, &m_WorldBuffer);

	// 頂点シェーダーのb1へ、カメラのビュー行列を接続する。
	m_Device->CreateBuffer( &bufferDesc, NULL, &m_ViewBuffer );
	m_DeviceContext->VSSetConstantBuffers( 1, 1, &m_ViewBuffer );

	// 頂点シェーダーのb2へ、透視投影または平行投影の行列を接続する。
	m_Device->CreateBuffer( &bufferDesc, NULL, &m_ProjectionBuffer );
	m_DeviceContext->VSSetConstantBuffers( 2, 1, &m_ProjectionBuffer );

	// ピクセルシェーダーのb5へ、鏡面反射計算に使うカメラ位置を接続する。
	bufferDesc.ByteWidth = sizeof(XMFLOAT4);
	m_Device->CreateBuffer(&bufferDesc, NULL, &m_CameraBuffer);
	m_DeviceContext->PSSetConstantBuffers(5, 1, &m_CameraBuffer);
	// 初回描画でも未初期化値を参照しないよう、カメラ位置を原点で初期化する。
	SetCameraPosition(Vector3(0.0f, 0.0f, 0.0f));

	// 頂点・ピクセルシェーダーのb6へ、エフェクト固有の汎用値を接続する。
	m_Device->CreateBuffer(&bufferDesc, NULL, &m_ParameterBuffer);
	m_DeviceContext->VSSetConstantBuffers(6, 1, &m_ParameterBuffer);
	m_DeviceContext->PSSetConstantBuffers(6, 1, &m_ParameterBuffer);

	// 最大256本のボーン行列を保持できるスキニング用定数バッファを作成する。
	D3D11_BUFFER_DESC boneDesc{};
	boneDesc.ByteWidth = sizeof(XMMATRIX) * 256;
	boneDesc.Usage = D3D11_USAGE_DEFAULT;
	boneDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	m_Device->CreateBuffer(&boneDesc, NULL, &m_BoneBuffer);



	// 頂点・ピクセルシェーダーのb3へ、モデル表面の材質情報を接続する。
	bufferDesc.ByteWidth = sizeof(MATERIAL);

	m_Device->CreateBuffer( &bufferDesc, NULL, &m_MaterialBuffer );
	m_DeviceContext->VSSetConstantBuffers( 3, 1, &m_MaterialBuffer );
	m_DeviceContext->PSSetConstantBuffers( 3, 1, &m_MaterialBuffer );


	// 頂点・ピクセルシェーダーのb4へ、シーン内のライト情報を接続する。
	bufferDesc.ByteWidth = sizeof(LIGHT);

	m_Device->CreateBuffer( &bufferDesc, NULL, &m_LightBuffer );
	m_DeviceContext->VSSetConstantBuffers( 4, 1, &m_LightBuffer );
	m_DeviceContext->PSSetConstantBuffers( 4, 1, &m_LightBuffer );





	// ライトが未設定のモデルも描画できるよう、真下向きの平行光源を初期値にする。
	LIGHT light{};
	light.Enable = true;
	light.Direction = XMFLOAT4(0.0f, -1.0f, 0.0f, 0.0f);
	light.Ambient = XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f);
	light.Diffuse = XMFLOAT4(1.5f, 1.5f, 1.5f, 1.0f);
	SetLight(light);



	// マテリアル未設定を見つけやすくするため、初期色を目立つマゼンタにする。
	MATERIAL material{};
	material.Diffuse = XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f);
	material.Ambient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	SetMaterial(material);




}



void Renderer::Uninit()
{
	// シェーダーへ値を渡すために作成した定数バッファを解放する。
	m_WorldBuffer->Release();
	m_ViewBuffer->Release();
	m_ProjectionBuffer->Release();
	m_LightBuffer->Release();
	m_MaterialBuffer->Release();
	m_ParameterBuffer->Release();


	// パイプラインに設定中のリソース参照を解除してから、Direct3D本体を解放する。
	m_DeviceContext->ClearState();
	m_RenderTargetView->Release();
	m_SwapChain->Release();
	m_DeviceContext->Release();
	m_Device->Release();

	// 色書き込みを制御するブレンドステートを解放する。
	m_BlendStateMask->Release();


}




void Renderer::Begin()
{
	// 前フレームの色、深度、ステンシル値を初期化して新しいフレームを開始する。
	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	m_DeviceContext->ClearRenderTargetView( m_RenderTargetView, clearColor );
	m_DeviceContext->ClearDepthStencilView( m_DepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
	m_DeviceContext->ClearDepthStencilView(m_DepthStencilView,
		D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

}



void Renderer::End()
{
	// 垂直同期を待ち、バックバッファへ描画した結果をウィンドウに表示する。
	m_SwapChain->Present(1, 0);

	// 別の描画先が設定されていても、次フレーム用にメインの描画先へ戻す。
	m_DeviceContext->OMSetRenderTargets(1, &m_RenderTargetView, m_DepthStencilView);

}




void Renderer::SetDepthEnable( bool Enable )
{
	// 有効時は深度比較と書き込みを行い、無効時は深度比較だけを残して書き込みを止める。
	if( Enable )
		m_DeviceContext->OMSetDepthStencilState( m_DepthStateEnable, NULL );
	else
		m_DeviceContext->OMSetDepthStencilState( m_DepthStateDisable, NULL );

}



void Renderer::SetWorldViewProjection2D()
{
	// 2D座標をそのまま画面座標として扱うため、モデル変換とカメラ変換を初期化する。
	SetWorldMatrix(XMMatrixIdentity());
	SetViewMatrix(XMMatrixIdentity());

	// 左上を原点とし、右方向をX正、下方向をY正とする平行投影行列を作成する。
	XMMATRIX projection;
	projection = XMMatrixOrthographicOffCenterLH(0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f);
	SetProjectionMatrix(projection);
}


void Renderer::SetWorldMatrix(XMMATRIX WorldMatrix)
{
	// HLSL側の行列配置に合わせて転置し、頂点シェーダーのb0を更新する。
	XMFLOAT4X4 worldf;
	XMStoreFloat4x4(&worldf, XMMatrixTranspose(WorldMatrix));
	m_DeviceContext->UpdateSubresource(m_WorldBuffer, 0, NULL, &worldf, 0, 0);
}

void Renderer::SetViewMatrix(XMMATRIX ViewMatrix)
{
	// カメラの位置と向きを表すビュー行列を、頂点シェーダーのb1へ送る。
	XMFLOAT4X4 viewf;
	XMStoreFloat4x4(&viewf, XMMatrixTranspose(ViewMatrix));
	m_DeviceContext->UpdateSubresource(m_ViewBuffer, 0, NULL, &viewf, 0, 0);
}

void Renderer::SetProjectionMatrix(XMMATRIX ProjectionMatrix)
{
	// 3D座標を画面へ投影する行列を、頂点シェーダーのb2へ送る。
	XMFLOAT4X4 projectionf;
	XMStoreFloat4x4(&projectionf, XMMatrixTranspose(ProjectionMatrix));
	m_DeviceContext->UpdateSubresource(m_ProjectionBuffer, 0, NULL, &projectionf, 0, 0);

}
void Renderer::SetCameraPosition(const Vector3& Position)
{
	// 鏡面反射の視線ベクトル計算に使うため、現在のカメラ座標をピクセルシェーダーへ渡す。
	const XMFLOAT4 cameraPosition(Position.x, Position.y, Position.z, 1.0f);
	m_DeviceContext->UpdateSubresource(m_CameraBuffer, 0, NULL, &cameraPosition, 0, 0);
	m_DeviceContext->PSSetConstantBuffers(5, 1, &m_CameraBuffer);
}


void Renderer::SetMaterial( MATERIAL Material )
{
	// モデルの色や反射特性を、頂点・ピクセルシェーダーが参照するb3へ送る。
	m_DeviceContext->UpdateSubresource( m_MaterialBuffer, 0, NULL, &Material, 0, 0 );
}
void Renderer::SetLight(LIGHT Light)
{
	// シーンのライト情報を定数バッファへ転送する。
	m_DeviceContext->UpdateSubresource(m_LightBuffer, 0, NULL, &Light, 0, 0);

	// 頂点・ピクセルシェーダーの両方からb4として参照できるように設定する。
	m_DeviceContext->VSSetConstantBuffers(4, 1, &m_LightBuffer);
	m_DeviceContext->PSSetConstantBuffers(4, 1, &m_LightBuffer);
}
void Renderer::SetParameter(XMFLOAT4 Parameter)
{
	// 描画対象ごとの追加パラメーターを、頂点・ピクセルシェーダーのb6へ送る。
	m_DeviceContext->UpdateSubresource(m_ParameterBuffer, 0, NULL, &Parameter, 0, 0);
	m_DeviceContext->VSSetConstantBuffers(6, 1, &m_ParameterBuffer);
	m_DeviceContext->PSSetConstantBuffers(6, 1, &m_ParameterBuffer);
}
void Renderer::SetCullMode(D3D11_CULL_MODE CullMode)
{
	// 指定されたカリング方法を使うラスタライザステートを一時的に作成する。
	D3D11_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	rasterizerDesc.DepthClipEnable = TRUE;
	rasterizerDesc.CullMode = CullMode; 

	ID3D11RasterizerState* rasterizerState;
	m_Device->CreateRasterizerState(&rasterizerDesc, &rasterizerState);

	// 作成したステートをラスタライザーステージへ反映する。
	m_DeviceContext->RSSetState(rasterizerState);

	// デバイスコンテキストが参照を保持するため、作成時に得た参照を解放する。
	rasterizerState->Release();
}
void Renderer::CreateVertexShader( ID3D11VertexShader** VertexShader, ID3D11InputLayout** VertexLayout, const char* FileName )
{

	// コンパイル済み頂点シェーダーをバイナリファイルとして読み込む。
	FILE* file;
	long int fsize;

	file = fopen(FileName, "rb");
	assert(file);

	fsize = _filelength(_fileno(file));
	unsigned char* buffer = new unsigned char[fsize];
	fread(buffer, fsize, 1, file);
	fclose(file);

	// 読み込んだバイトコードからGPU用の頂点シェーダーを作成する。
	m_Device->CreateVertexShader(buffer, fsize, NULL, VertexShader);


	// VERTEX_3Dの各メンバーを、HLSL側の入力セマンティクスへ対応付ける。
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 4 * 3, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 4 * 6, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 4 * 10, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "BONEINDEX",  0, DXGI_FORMAT_R32G32B32A32_UINT,  0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "BONEWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 64, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	UINT numElements = ARRAYSIZE(layout);

	// 頂点シェーダーのバイトコードを使って、頂点データの入力レイアウトを作成する。
	m_Device->CreateInputLayout(layout,
		numElements,
		buffer,
		fsize,
		VertexLayout);

	// シェーダー作成後はCPU側の読み込みバッファが不要になる。
	delete[] buffer;
}



void Renderer::CreatePixelShader( ID3D11PixelShader** PixelShader, const char* FileName )
{
	// コンパイル済みピクセルシェーダーをバイナリファイルとして読み込む。
	FILE* file;
	long int fsize;

	file = fopen(FileName, "rb");
	assert(file);

	fsize = _filelength(_fileno(file));
	unsigned char* buffer = new unsigned char[fsize];
	fread(buffer, fsize, 1, file);
	fclose(file);

	// 読み込んだバイトコードからGPU用のピクセルシェーダーを作成する。
	m_Device->CreatePixelShader(buffer, fsize, NULL, PixelShader);

	// シェーダー作成後はCPU側の読み込みバッファが不要になる。
	delete[] buffer;
}


void Renderer::InitCommonShader()
{
	// ピクセル単位のBlinn-Phongライティングを行う共通シェーダーを読み込む。
	CreateVertexShader(&m_VS, &m_Layout,
		"shader\\pixelLightingBlinnPhongVS.cso");
	CreatePixelShader(&m_PS,
		"shader\\pixelLightingBlinnPhongPS.cso");
}

void Renderer::SetCommonShader()
{
	// 入力レイアウト、頂点シェーダー、ピクセルシェーダーを描画パイプラインへ設定する。
	auto* ctx = GetDeviceContext();
	ctx->IASetInputLayout(m_Layout);
	ctx->VSSetShader(m_VS, nullptr, 0);
	ctx->PSSetShader(m_PS, nullptr, 0);
}
