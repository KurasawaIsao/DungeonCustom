#ifndef VECTOR2INT_H
#define VECTOR2INT_H
#include<math.h>
class Vector2Int
{
public:
	int x, y;

	Vector2Int() : x(0), y(0) {}

	Vector2Int(const Vector2Int& a) :x(a.x), y(a.y){}

	Vector2Int(int nx, int ny) :x(nx), y(ny) {}

	Vector2Int& operator=(const Vector2Int& a)
	{
		x = a.x; y = a.y;
		return *this;
	}

	bool operator ==(const Vector2Int& a)const
	{
		return x == a.x && y == a.y;
	}


	bool operator !=(const Vector2Int& a)const
	{
		return x != a.x || y != a.y;
	}

	void zero() { x = y =  0.0f; };

	Vector2Int operator -()const { return Vector2Int(-x, -y); };

	Vector2Int operator +(const Vector2Int& a)const { return Vector2Int(x + a.x, y + a.y); };

	Vector2Int operator -(const Vector2Int& a)const { return Vector2Int(x - a.x, y - a.y); };

	Vector2Int operator *(float a)const { return Vector2Int(x * a, y * a); };

	Vector2Int operator /(float a)const//ここについてるconstはメンバ変数を変えないという約束
	{
		float oneOverA = 1.0f / a;
		return Vector2Int(x * oneOverA, y * oneOverA);
	};

	Vector2Int& operator +=(const Vector2Int& a)
	{
		x += a.x; y += a.y;
		return *this;
	}

	Vector2Int& operator -=(const Vector2Int& a)
	{
		x -= a.x; y -= a.y;
		return *this;
	}

	Vector2Int& operator *=(const Vector2Int& a)
	{
		x *= a.x; y *= a.y;
		return *this;
	}

	Vector2Int& operator /=(float a)
	{
		float oneOverA = 1.0f / a;
		x *= oneOverA; y *= oneOverA;
		return *this;
	}

	void normalize()
	{
		// グリッド上の方向として扱えるように、各成分を -1、0、1 のいずれかに揃える。
		x = (x > 0) - (x < 0);
		y = (y > 0) - (y < 0);
	}

	Vector2Int normalized() const
	{
		// 元の値を変更せず、グリッド上で正規化した方向を返す。
		return Vector2Int(
			(x > 0) - (x < 0),
			(y > 0) - (y < 0)
		);
	}

	float Length()const
	{
		return static_cast<float>(x * x + y * y);
	}

	// 長さ（平方根付き）を返す。グリッド差分の実距離を見たい時に使う。
	float LengthSqrt() const
	{
		return sqrtf(Length());
	}

	// 2点間のユークリッド距離を返す。
	static float Distance(const Vector2Int& a, const Vector2Int& b) {
		return (a - b).LengthSqrt();
	}

	// 上下左右だけで数える距離を返す。経路探索の優先度付けに使う。
	int Manhattan(const Vector2Int& other) const
	{
		return abs(x - other.x) + abs(y - other.y);
	}

	// 上下左右だけで数える距離を、static関数としても呼べるようにする。
	static int ManhattanDistance(const Vector2Int& a, const Vector2Int& b)
	{
		return a.Manhattan(b);
	}

	// 斜め移動を1歩として数える距離を返す。隣接判定や視界範囲に使う。
	int Chebyshev(const Vector2Int& other) const
	{
		int dx = abs(x - other.x);
		int dy = abs(y - other.y);
		return dx > dy ? dx : dy;
	}

	// 斜め移動を1歩として数える距離を、static関数としても呼べるようにする。
	static int ChebyshevDistance(const Vector2Int& a, const Vector2Int& b)
	{
		return a.Chebyshev(b);
	}
};
#endif