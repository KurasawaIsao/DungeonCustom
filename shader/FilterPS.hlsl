
// unlitColorVS.hlsl

#include "common.hlsl"
void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{


    outDiffuse = In.Diffuse;
        
    outDiffuse *= Material.Diffuse;
    //outDiffuse.a = 1.0f;
	// さらにピクセルのデフューズ色を乗算しておく(町転職にテクスチャ色影響される)
    
}
