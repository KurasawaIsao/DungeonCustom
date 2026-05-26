
// unlitColorVS.hlsl

#include "common.hlsl"

void main(in VS_IN In, out PS_IN Out)
{
	// 頂点変換(必ず変更)
    matrix wvp; // ワールドビュープロジェクション行列
    wvp = mul(World, View); // wvp = World * View
    wvp = mul(wvp, Projection); // wvp = wvp * Projection
    Out.Position = mul(In.Position, wvp); // 頂点座標を行列で変換

	// 座標以外の要素を出力
    Out.Normal = In.Normal;
    Out.TexCoord = In.TexCoord;
    Out.Diffuse = In.Diffuse;


}



