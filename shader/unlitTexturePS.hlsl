
#include "common.hlsl"	// 必ずインクルード

// DirextXのサンプラーステート設定を受け継ぐ
Texture2D g_Texture : register(t0); // テクスチャ画像を表す変数 レジスタ0 = テクスチャ0番

Texture2D g_Texture2 : register(t1);

// DirextXのサンプラーステート設定を受け継ぐ
SamplerState g_SamplerState : register(s0); // テクスチャサンプラー0番


void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    if (Material.TextureEnable)
    {
        // テクスチャの色を取得
        float4 texColor = g_Texture.Sample(g_SamplerState, In.TexCoord) * In.Diffuse.a;
        texColor += g_Texture2.Sample(g_SamplerState, In.TexCoord) * (1.0 - In.Diffuse.a);
        


        // ★ここでマテリアルの色（SetAlphaで設定した色）を掛け合わせる
        outDiffuse = texColor * Material.Diffuse;
    }
    else
    {
        outDiffuse = In.Diffuse;
        outDiffuse *= Material.Diffuse;
    }

    // 霧の計算などはそのまま
    float3 fogColor = float3(1.0, 1.0, 1.0);
    float fog = In.Depth * 0.009;
    outDiffuse.rgb = outDiffuse.rgb * (1.0 - fog) + fogColor * fog;
}