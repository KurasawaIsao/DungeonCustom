#pragma once
#include"main.h"
#include "renderer.h"

namespace Easing
{
template <typename T>
T Lerp(const T& start, const T& end, float t)
{
	return start + (end - start) * t;
}
XMFLOAT3 LerpedFloat3(const XMFLOAT3& start, const XMFLOAT3& end, float t);
//減速移動(2次関数版)
float OutQuad(float time);
//加速移動
float InQuad(float time);
//加減速移動
float InOutQuad(float time);
//4次関数版
//減速移動
float OutQuart(float time);
//加速移動
float InQuart(float time);
//加減速移動
float InOutQuart(float time);
//行き過ぎて戻ってくる動き
//overshoot:振れ幅　値が大きいと大きく行き過ぎる形になる
//freq:振れ幅の周波数 値が小さい=揺れが速い
float OutClassic(float time, float amp, float freq);
//加速していって到達地点でバウンドする
float OutBounce(float time);
//3Dベクトル線形補間 time=0.0fならstart => time=1.0ならendになるよう直線補間する
//他のバージョンが必要なら自作する
float ClampFloat(float watasaretaatai,float min, float max);
}
