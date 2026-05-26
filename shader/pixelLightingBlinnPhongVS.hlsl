#include "common.hlsl"


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
    // 頂点変換処理 この処理は必ず実行する
    matrix wvp; // 行列変数を作成
    wvp = mul(World, View);                 // wvp = ワールド行列 * カメラ行列
    wvp = mul(wvp, Projection);             // wvp = wvp * プロジェクション行列
    Out.Position = mul(skinnedPos, wvp);; // 変換結果を出力
    
    // 頂点法線をワールド行列で回転させる
    float4 normal = float4(skinnedNormal, 0.0);
    float4 worldNormal = mul(normal, World);
    worldNormal = normalize(worldNormal);
    Out.Normal = worldNormal; // 開店後の法線出力
    
    Out.Diffuse = In.Diffuse;               // 頂点の物をそのまま出力
    Out.TexCoord = In.TexCoord;             // 頂点のUV座標をそのまま出力
    
    // ワールド変換した頂点座標を出力
    Out.WorldPosition = mul(skinnedPos, World);
    
    
}
