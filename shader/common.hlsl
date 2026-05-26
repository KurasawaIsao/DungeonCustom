// このファイルは他のシェーダーファイルへインクルードされます
// 各種マトリクスやベクトルを受け取る変数を用意

cbuffer WorldBuffer : register(b0)
{
    matrix World;
}
cbuffer ViewBuffer : register(b1)
{
    matrix View;
}
cbuffer ProjectionBuffer : register(b2)
{
    matrix Projection;
}
 
 
 
struct MATERIAL
{
    float4 Ambient;
    float4 Diffuse;
    float4 Specular;
    float4 Emission;
    float Shininess;
    bool TextureEnable;
    float2 Dummy;
};
 
cbuffer MaterialBuffer : register(b3)
{
    MATERIAL Material;
}
 
 
 
struct LIGHT
{
    bool Enable; // boolとはいうがfloat型で上手くごまかしている
    bool3 Dummy; // 配置アドレスを既定の倍数にするためのパディング
    float4 Direction; // 実はC言語でも同じでVisualStudioがやってくれる
    float4 Diffuse;
    float4 Ambient;

    float4 Position; // 光の位置
    float4 PointLightParam; // 光の届く位置
	
    float4 SkyColor;
    float4 GroundColor;
    float4 GroundNormal;

    float4 Angle;
	
};
 
cbuffer LightBuffer : register(b4)
{
    LIGHT Light;
}
cbuffer CameraBuffer : register(b5) // バッファの5番とする
{
    float4 CameraPosition; // カメラ座標を受け取る変数
}

cbuffer ParmeterBuffer : register(b6)
{
    float4 Parmeter; // シェーダー内で使う変数名
}
 
cbuffer BoneBuffer : register(b7) 
{
    matrix BoneMatrices[256];
}
// 頂点シェーダーへ入力されるデータを構造体の形で表現
// これは頂点バッファの内容そのもの
struct VS_IN
{
    float4 Position : POSITION0;
    float4 Normal : NORMAL0;
    float4 Diffuse : COLOR0;
    float2 TexCoord : TEXCOORD0;
    
    uint4 BoneIndices : BONEINDEX0;
    float4 BoneWeights : BONEWEIGHT0;

    uint InstanceId : SV_InstanceID;
};

struct PS_IN
{
    float4 Position : SV_POSITION;
    float4 WorldPosition : POSITION0;
    float4 Normal : NORMAL0;
    float4 Diffuse : COLOR0;
    float2 TexCoord : TEXCOORD0;
    float Depth : DEPTH0;
};



