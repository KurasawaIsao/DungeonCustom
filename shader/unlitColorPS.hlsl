// ピクセルシェーダー
// unlitColorPS.hlsl

#include "common.hlsl"	// 必ずインクルード

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
	// 入力されたピクセル色をそのまま出力
	outDiffuse = In.Diffuse;
}


