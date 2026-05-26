#pragma once
#include<math.h>
class Vector2
{
public:
	float x, y;

	Vector2() {}

	Vector2(const Vector2& a) :x(a.x), y(a.y){}

	Vector2(float nx, float ny) :x(nx), y(ny){}

	Vector2& operator=(const Vector2& a)
	{
		x = a.x; y = a.y;
		return *this;
	}

	bool operator ==(const Vector2& a)const
	{
		return x == a.x && y == a.y;
	}


	bool operator !=(const Vector2& a)const
	{
		return x != a.x || y != a.y;
	}

	void zero() { x = y = 0.0f; };

	Vector2 operator -()const { return Vector2(-x, -y); };

	Vector2 operator +(const Vector2& a)const { return Vector2(x + a.x, y + a.y); };

	Vector2 operator -(const Vector2& a)const { return Vector2(x - a.x, y - a.y); };

	Vector2 operator *(float a)const { return Vector2(x * a, y * a); };

	Vector2 operator /(float a)const//ここについてるconstはメンバ変数を変えないという約束
	{
		float oneOverA = 1.0f / a;
		return Vector2(x * oneOverA, y * oneOverA);
	};

	Vector2& operator +=(const Vector2& a)
	{
		x += a.x; y += a.y; 
		return *this;
	}

	Vector2& operator -=(const Vector2& a)
	{
		x -= a.x; y -= a.y;
		return *this;
	}

	Vector2& operator *=(const Vector2& a)
	{
		x *= a.x; y *= a.y;
		return *this;
	}

	Vector2& operator /=(float a)
	{
		float oneOverA = 1.0f / a;
		x *= oneOverA; y *= oneOverA;
		return *this;
	}

	void normalize()
	{
		float magSq = x * x + y * y;
		if (magSq > 0.0f)
		{
			float oneOverMag = 1.0f / sqrt(magSq);
			x *= oneOverMag;
			y *= oneOverMag;
		}
	}


	/*float operator*(const Vector2& a)const
	{
		return x * a.x + y * a.y + z * a.z;
	}*/

	float Length()const
	{
		return x * x + y * y;
	}

	// 長さ（平方根付き）
	float LengthSqrt() const {
		return sqrtf(x * x + y * y);
	}
	static float dot(const Vector2& a, const Vector2& b) {
		return a.x * b.x + a.y * b.y;
	}

	static Vector2 Lerp(const Vector2& a, const Vector2& b, float t)
	{
		return a * (1.0f - t) + b * t;
	}

};