#include "Easing.h"

namespace Easing
{

//減速移動(2次関数版)
float OutQuad(float time)
{
	return (1.0f * time * (time - 2.0f));
}
//加速移動
float InQuad(float time)
{
	return (time * time);
}
//加減速移動
float InOutQuad(float time)
{
	time *= 2.0f;
	if (time < 1.0f)
	{
		return (time * time * 0.5f);
	}
	time -= 1.0f;
	return (time * (time - 2.0f) - 1.0f) * -0.5f;
}
//4次関数版
//減速移動
float OutQuart(float time)
{
	time -= 1.0f;
	return -1.0f * (time * time * time * time - 1.0f);
}
//加速移動
float InQuart(float time)
{
	return time * time * time * time;
}
//加減速移動
float InOutQuart(float time)
{
	time *= 2.0f;
	if (time < 1.0f)
	{
		(0.5f * time * time * time * time);
	}
	time -= 2.0f;
	return (-0.5f * (time * time * time * time -2.0f));
}
//行き過ぎて戻ってくる動き
//overshoot:振れ幅　値が大きいと大きく行き過ぎる形になる
//freq:振れ幅の周波数 値が小さい=揺れが速い
float OutClassic(float time, float amp, float freq)
{
	freq /= 4.0f;
	float sfreq = freq / 4.0f;

	if ((amp > 1.0f) && (time < 0.4f))
	{
		amp = 1.0f + (time / 0.4f * (amp - 1.0f));
	}

	return (1.0f + powf(2.0f, -10.0f * time) * sinf((time - sfreq) * XM_2PI / freq) * amp);
}
//加速していって到達地点でバウンドする
float OutBounce(float time)
{
	float tmp1 = 2.75f;
	float tmp2 = 7.5625f;

	if (time < (1.0f / tmp1))
	{
		return tmp2 * time * time;
	}
	else if (time < (2.0f / tmp1))
	{
		time -= (1.5f / tmp1);
		return tmp2 * time * time + 0.75f;
	}
	else if (time < (2.5f / tmp1))
	{
		time -= (2.25f / tmp1);
	}
	else
	{
		time -= (2.625f / tmp1);
		return tmp2 * time * time + 0.984375f;
	}
}
float ClampFloat(float watasaretaatai, float min, float max)
{
	if (watasaretaatai < min)
	{
		return min;
	}
	else if (watasaretaatai > max)
	{
		return max;
	}
	else
	{
		return watasaretaatai;
	}

}
XMFLOAT3 LerpedFloat3(const XMFLOAT3& a, const XMFLOAT3& b, float t) {
	// XMFLOAT3 を XMVECTOR に変換
	XMVECTOR v0 = XMLoadFloat3(&a);
	XMVECTOR v1 = XMLoadFloat3(&b);

	// 線形補間（Slerp ではなく Lerp）
	XMVECTOR result = XMVectorLerp(v0, v1, t);

	// XMFLOAT3 に変換して返す
	XMFLOAT3 output;
	XMStoreFloat3(&output, result);
	return output;
}

}

//3Dベクトル線形補間 time=0.0fならstart => time=1.0ならendになるよう直線補間する
//他のバージョンが必要なら自作する
//XMFLOAT3 Vec3Lerp(XMFLOAT3 startpos, XMFLOAT3 endpos, float time)
//{
//	XMFLOAT3 retv;//startからendをtで保管してretvを作る
//	//補完部分(未完)
//	return retv;
//}