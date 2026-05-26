#include "common.hlsl"

Texture2D g_Texture : register(t0);
SamplerState g_SamplerState : register(s0);

void main(in PS_IN In, out float4 OutColor : SV_Target)
{
    float4 col = g_Texture.Sample(g_SamplerState, In.TexCoord);
    col *= In.Diffuse; // Diffuse.a ‚É Alpha
    clip(col.a - 0.01f);
    OutColor = col;
}
