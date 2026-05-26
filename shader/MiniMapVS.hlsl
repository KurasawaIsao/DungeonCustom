
// unlitColorVS.hlsl

#include "common.hlsl"	// 必ずインクルード

struct MinimapInstance
{
    float3 pos; // スクリーン座標 px
    float size; // タイルサイズ
    float4 color; // 色
};
StructuredBuffer<MinimapInstance> InstBuf : register(t0);

struct VS_OUT
{
    float4 Position : SV_POSITION;
    float4 Diffuse : COLOR0;
    float2 TexCoord : TEXCOORD0;
};

// ScreenW, ScreenH を渡す
cbuffer ScreenInfo : register(b7)
{
    float ScreenW;
    float ScreenH;
    float2 padding;
};

VS_OUT main(VS_IN input)
{
    VS_OUT o;

    // Instance データ読み込み
    MinimapInstance inst = InstBuf[input.InstanceId];

    // Quad のローカル位置（-0.5～0.5）を px に変換
    float2 pixelPos = float2(
        inst.pos.x + input.Position.x * inst.size,
        inst.pos.y - input.Position.y * inst.size
    );

    // ピクセル座標 → NDC
    float ndcX = pixelPos.x / ScreenW * 2.0f - 1.0f;
    float ndcY = 1.0f - pixelPos.y / ScreenH * 2.0f;

    o.Position = float4(ndcX, ndcY, 0, 1);
    o.TexCoord = input.TexCoord;

    // 色はインスタンスから
    o.Diffuse = inst.color;

    return o;
}

