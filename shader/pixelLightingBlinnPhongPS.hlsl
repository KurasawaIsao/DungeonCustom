#include "common.hlsl"


Texture2D g_Texture : register(t0);
SamplerState g_SamplerState : register(s0);

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    //ピクセルの法線を正規化
    float4 normal = normalize(In.Normal);
    float light = -dot(normal.xyz, Light.Direction.xyz); // 光源計算をする
    light = saturate(light); //
    
    //テクスチャのピクセル色を取得
    if (Material.TextureEnable)
    {

        outDiffuse = g_Texture.Sample(g_SamplerState, In.TexCoord) * In.Diffuse.a;

        //outDiffuse *= In.Diffuse;←aはブレンド専用目的で計算するので乗算までしなくていい
    }
    else
    {
        outDiffuse = In.Diffuse;
        
        outDiffuse *= Material.Diffuse;
    }
   
    outDiffuse.rgb *= In.Diffuse.rgb * light + Light.Ambient.rgb; // 明るさを乗算
    outDiffuse.a *= In.Diffuse.a; // αに明るさは関係ないので別計算

    
    // カメラからピクセルへ向かうベクトル
    float3 eyev = In.WorldPosition.xyz - CameraPosition.xyz;
    eyev = normalize(eyev);
    
    // ハーフベクトルの作成
    float3 halfv = eyev + Light.Direction.xyz;  // 視線とライトベクトルを加算
    halfv = normalize(halfv); // 正規化する
    
    float specular = -dot(halfv, normal.xyz);   // ハーフベクトルと法線の内積を計算
    specular = saturate(specular);
    specular = pow(specular, max(Material.Shininess, 1.0f));
    
    outDiffuse.rgb += specular * Material.Specular.rgb * Light.Diffuse.rgb;
    
}
