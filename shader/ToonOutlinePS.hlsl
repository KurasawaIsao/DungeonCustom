struct OUTLINE_PS_IN
{
    float4 Position : SV_POSITION;
    float4 WorldPosition : POSITION0;
};

float Hash21(float2 p)
{
    p = frac(p * float2(234.34f, 435.35f));
    p += dot(p, p + 34.23f);
    return frac(p.x * p.y);
}

void main(in OUTLINE_PS_IN In, out float4 outDiffuse : SV_Target)
{
    outDiffuse = float4(0.0f, 0.0f, 0.0f, 1.0f);
}
