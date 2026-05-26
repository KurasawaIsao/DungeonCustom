#pragma once
#include<math.h>
class Vector3
{
public:
	float x, y, z;

	Vector3() {}

	Vector3(const Vector3& a) :x(a.x), y(a.y), z(a.z) {}

	Vector3(float nx, float ny, float nz) :x(nx), y(ny), z(nz) {}

	Vector3 &operator=(const Vector3& a)
	{
		x = a.x; y = a.y; z = a.z;
		return *this;
	}

	bool operator ==(const Vector3& a)const
	{
		return x == a.x && y == a.y && z == a.z;
	}


	bool operator !=(const Vector3& a)const
	{
		return x != a.x || y != a.y || z != a.z;
	}

	void zero() { x = y = z = 0.0f; };

	Vector3 operator -()const { return Vector3(-x, -y, -z); };

	Vector3 operator +(const Vector3& a)const { return Vector3(x + a.x, y + a.y, z + a.z); };

	Vector3 operator -(const Vector3& a)const { return Vector3(x - a.x, y - a.y, z - a.z); };

	Vector3 operator *(float a)const { return Vector3(x * a, y * a, z * a); };

	Vector3 operator /(float a)const//ここについてるconstはメンバ変数を変えないという約束
	{
		float oneOverA = 1.0f / a;
		return Vector3(x * oneOverA, y * oneOverA, z * oneOverA);
	};

	Vector3& operator +=(const Vector3& a)
	{
		x += a.x; y += a.y; z += a.z;
		return *this;
	}

	Vector3& operator -=(const Vector3& a)
	{
		x -= a.x; y -= a.y; z -= a.z;
		return *this;
	}

	Vector3& operator *=(const Vector3& a)
	{
		x *= a.x; y *= a.y; z *= a.z;
		return *this;
	}

	Vector3& operator /=(float a)
	{
		float oneOverA = 1.0f / a;
		x *= oneOverA; y *= oneOverA; z *= oneOverA;
		return *this;
	}

	void normalize()
	{
		float magSq = x * x + y * y + z * z;
		if (magSq > 0.0f)
		{
			float oneOverMag = 1.0f / sqrt(magSq);
			x *= oneOverMag;
			y *= oneOverMag;
			z *= oneOverMag;
		}
	}


	/*float operator*(const Vector3& a)const
	{
		return x * a.x + y * a.y + z * a.z;
	}*/

	float Length()const
	{
		return x * x + y * y + z * z;
	}

	// 長さ（平方根付き）
	float LengthSqrt() const {
		return sqrtf(x * x + y * y + z * z);
	}
	static float dot(const Vector3& a, const Vector3& b) {
		return a.x * b.x + a.y * b.y + a.z * b.z;
	}
	static Vector3 cross(const Vector3& a, const Vector3& b) {
		Vector3 ret;
		ret.x = a.y * b.z - a.z * b.y;
		ret.y = a.z * b.x - a.x * b.z;
		ret.z = a.x * b.y - a.y * b.x;
		return ret;
	}
	static Vector3 Lerp(const Vector3& a, const Vector3& b, float t)
	{
		return a * (1.0f - t) + b * t;
	}
	static float Lerp(const float& a, const float& b, float t)
	{
		return a * (1.0f - t) + b * t;
	}
	static float Clamp(float t,const float& a, const float& b)
	{
		if (t <= a)
		{
			return a;
		}
		else if (t >= b)
		{
			return b;
		}
		return t;
	}

};