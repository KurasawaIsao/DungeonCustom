#include "common.hlsl"

struct OUTLINE_PS_IN
{
    float4 Position : SV_POSITION;
    float4 WorldPosition : POSITION0;
};

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

void main(in VS_IN In, out OUTLINE_PS_IN Out)
{
    float4 skinnedPos;
    float3 skinnedNormal;
    SkinVertex(In, skinnedPos, skinnedNormal);

    float4 worldPos = mul(skinnedPos, World);
    float4 viewPos = mul(worldPos, View);
    float4 clipPos = mul(viewPos, Projection);

    float2 screenSize = float2(1280.0f, 720.0f);
    clipPos.xy += Parmeter.xy * (2.0f / screenSize) * clipPos.w;

    Out.Position = clipPos;
    Out.WorldPosition = worldPos;
}
