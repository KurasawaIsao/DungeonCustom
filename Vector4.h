#pragma once
#include<math.h>
class Vector4
{
public:
	float x, y, z, w;

	Vector4() {}

	Vector4(const Vector4& a) :x(a.x), y(a.y), z(a.z),w(a.w) {}

	Vector4(float nx, float ny, float nz,float nw) :x(nx), y(ny), z(nz),w(nw) {}

	Vector4& operator=(const Vector4& a)
	{
		x = a.x; y = a.y; z = a.z; w = a.w ;
		return *this;
	}

	bool operator ==(const Vector4& a)const
	{
		return x == a.x && y == a.y && z == a.z && w == a.w;
	}


	bool operator !=(const Vector4& a)const
	{
		return x != a.x || y != a.y || z != a.z || w != a.w;
	}

	void zero() { x = y = z = 0.0f; };

	Vector4 operator -()const { return Vector4(-x, -y, -z,-w); };

	Vector4 operator +(const Vector4& a)const { return Vector4(x + a.x, y + a.y, z + a.z,w + a.w); };

	Vector4 operator -(const Vector4& a)const { return Vector4(x - a.x, y - a.y, z - a.z,w - a.w); };

	Vector4 operator *(float a)const { return Vector4(x * a, y * a, z * a,w * a); };

	Vector4 operator /(float a)const//ここについてるconstはメンバ変数を変えないという約束
	{
		float oneOverA = 1.0f / a;
		return Vector4(x * oneOverA, y * oneOverA, z * oneOverA,w * oneOverA);
	};

	Vector4& operator +=(const Vector4& a)
	{
		x += a.x; y += a.y; z += a.z; w += a.w;
		return *this;
	}

	Vector4& operator -=(const Vector4& a)
	{
		x -= a.x; y -= a.y; z -= a.z; w -= a.w;
		return *this;
	}

	Vector4& operator *=(const Vector4& a)
	{
		x *= a.x; y *= a.y; z *= a.z; w *= a.w;
		return *this;
	}

	Vector4& operator /=(float a)
	{
		float oneOverA = 1.0f / a;
		x *= oneOverA; y *= oneOverA; z *= oneOverA; w *= oneOverA;
		return *this;
	}

	void normalize()
	{
		float magSq = x * x + y * y + z * z + w * w;
		if (magSq > 0.0f)
		{
			float oneOverMag = 1.0f / sqrt(magSq);
			x *= oneOverMag;
			y *= oneOverMag;
			z *= oneOverMag;
			w *= oneOverMag;
		}
	}


	/*float operator*(const Vector4& a)const
	{
		return x * a.x + y * a.y + z * a.z;
	}*/

	float Length()const
	{
		return x * x + y * y + z * z + w * w;
	}

	// 長さ（平方根付き）
	float LengthSqrt() const {
		return sqrtf(Length());
	}
	static float dot(const Vector4& a, const Vector4& b) {
		return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
	}
	static Vector4 Lerp(const Vector4& a, const Vector4& b, float t)
	{
		return a * (1.0f - t) + b * t;
	}

};