#include "common.hlsl"

Texture2D g_Texture : register(t0);
SamplerState g_SamplerState : register(s0);

float Hash21(float2 p)
{
    p = frac(p * float2(123.34f, 456.21f));
    p += dot(p, p + 45.32f);
    return frac(p.x * p.y);
}

float3 QuantizeToonLight(float ndotl)
{
    float softLight = ndotl * 0.65f + 0.35f;
    float band = 0.56f;
    band = lerp(band, 0.72f, smoothstep(0.25f, 0.40f, softLight));
    band = lerp(band, 0.88f, smoothstep(0.52f, 0.68f, softLight));
    band = lerp(band, 1.00f, smoothstep(0.78f, 0.92f, softLight));

    float3 ambient = max(Light.Ambient.rgb, float3(0.8f, 0.8f, 0.8f));
    return ambient + Light.Diffuse.rgb * band * 0.55f;
}

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    float4 baseColor = Material.Diffuse * In.Diffuse;

    if (Material.TextureEnable)
    {
        baseColor *= g_Texture.Sample(g_SamplerState, In.TexCoord);
    }

    float3 normal = normalize(In.Normal.xyz);
    float ndotl = saturate(-dot(normal, normalize(Light.Direction.xyz)));
    float3 toonLight = QuantizeToonLight(ndotl);

    float paperNoise = Hash21(floor(In.WorldPosition.xz * 4.0f + In.WorldPosition.y));
    float handShade = lerp(0.995f, 1.015f, paperNoise);

    outDiffuse = baseColor;
    outDiffuse.rgb *= toonLight * handShade;

    float rim = 1.0f - saturate(abs(dot(normal, float3(0.0f, 1.0f, 0.0f))));
    outDiffuse.rgb *= lerp(1.0f, 0.98f, rim * 0.06f);
    outDiffuse.a = baseColor.a;
}
