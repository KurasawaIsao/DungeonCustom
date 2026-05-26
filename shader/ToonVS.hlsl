#include "common.hlsl"

void SkinVertex(in VS_IN In, out float4 skinnedPos, out float3 skinnedNormal)
{
    skinnedPos = float4(0.0f, 0.0f, 0.0f, 0.0f);
    skinnedNormal = float3(0.0f, 0.0f, 0.0f);

    float weightSum = In.BoneWeights.x + In.BoneWeights.y + In.BoneWeights.z + In.BoneWeights.w;

    if (weightSum <= 0.001f)
    {
        skinnedPos = In.Position;
        skinnedNormal = In.Normal.xyz;
        return;
    }

    [unroll]
    for (int i = 0; i < 4; i++)
    {
        float weight = In.BoneWeights[i];
        if (weight > 0.0f)
        {
            skinnedPos += mul(BoneMatrices[In.BoneIndices[i]], In.Position) * weight;
            skinnedNormal += mul(BoneMatrices[In.BoneIndices[i]], float4(In.Normal.xyz, 0.0f)).xyz * weight;
        }
    }
}

void main(in VS_IN In, out PS_IN Out)
{
    float4 skinnedPos;
    float3 skinnedNormal;
    SkinVertex(In, skinnedPos, skinnedNormal);

    matrix wv = mul(World, View);
    matrix wvp = mul(wv, Projection);

    Out.Position = mul(skinnedPos, wvp);
    Out.WorldPosition = mul(skinnedPos, World);

    float4 worldNormal = mul(float4(skinnedNormal, 0.0f), World);
    Out.Normal = float4(normalize(worldNormal.xyz), 0.0f);

    Out.Diffuse = In.Diffuse;
    Out.TexCoord = In.TexCoord;
    Out.Depth = Out.Position.z / Out.Position.w;
}
