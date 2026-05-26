#include "common.hlsl"

Texture2D tex0 : register(t1);
SamplerState smp : register(s0);

float4 main(PS_IN input) : SV_Target
{
    float4 base = tex0.Sample(smp, input.TexCoord);
    return base * input.Diffuse;
}
