
// unlitColorVS.hlsl

#include "common.hlsl"	// 必ずインクルード

void main(in VS_IN In, out PS_IN Out)
{
    float4 skinnedPos = float4(0, 0, 0, 0);
    float3 skinnedNormal = float3(0, 0, 0);

    float weightSum = In.BoneWeights[0] + In.BoneWeights[1] + In.BoneWeights[2] + In.BoneWeights[3];

    // ウェイトが存在しない（静的メッシュなどの）場合はそのままの座標を使う
    if (weightSum <= 0.001f)
    {
        skinnedPos = In.Position;
        skinnedNormal = In.Normal.xyz;
    }
    else
    {
        for (int i = 0; i < 4; i++)
        {
            float weight = In.BoneWeights[i];
            if (weight > 0.0f)
            {
                skinnedPos += mul(BoneMatrices[In.BoneIndices[i]], In.Position) * weight;
                skinnedNormal += mul(BoneMatrices[In.BoneIndices[i]], float4(In.Normal.xyz, 0.0)).xyz * weight;
            }
        }
    }
	// 頂点変換(必ず変更)
    matrix wvp; // ワールドビュープロジェクション行列
    wvp = mul(World, View); // wvp = World * View
    wvp = mul(wvp, Projection); // wvp = wvp * Projection
	// ポリゴンの頂点を変換行列で変換して出力
    Out.Position = mul(skinnedPos, wvp); // 頂点座標を行列で変換
	
    Out.TexCoord = In.TexCoord; // 頂点のテクスチャ座標を出力
    Out.Diffuse = In.Diffuse; // 頂点色の出力
    Out.Normal = In.Normal;
    Out.Depth = Out.Position.z/* / Out.Position.y*/;

}




