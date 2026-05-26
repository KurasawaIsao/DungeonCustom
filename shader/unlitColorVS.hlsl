
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
                skinnedPos += mul(In.Position, BoneMatrices[In.BoneIndices[i]]) * weight;
                skinnedNormal += mul(float4(In.Normal.xyz, 0.0), BoneMatrices[In.BoneIndices[i]]).xyz * weight;
            }
        }
    }
	// 頂点変換(必ず変更)
	matrix wvp;					// ワールドビュープロジェクション行列
	wvp = mul(World, View);		// wvp = World * View
	wvp = mul(wvp, Projection);	// wvp = wvp * Projection
    Out.Position = mul(skinnedPos, wvp);; // 頂点座標を行列で変換

	// 座標以外の要素を出力
	Out.Normal = In.Normal;
	Out.TexCoord = In.TexCoord;
	Out.Diffuse = In.Diffuse;


}



