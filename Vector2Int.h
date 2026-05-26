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

	Vector2Int operator /(float a)const//‚±‚±‚É‚Â‚¢‚Ä‚éconst‚Íƒƒ“ƒo•Ï”‚ð•Ï‚¦‚È‚¢‚Æ‚¢‚¤–ñ‘©
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
		float magSq = x * x + y * y;
		if (magSq > 0.0f)
		{
			float oneOverMag = 1.0f / sqrt(magSq);
			x *= oneOverMag;
			y *= oneOverMag;
		}
	}

	static float Distance(const Vector2Int& a, const Vector2Int& b) {
		float dx = static_cast<float>(a.x - b.x);
		float dy = static_cast<float>(a.y - b.y);
		return sqrt(dx * dx + dy * dy);
	}
	/*float operator*(const Vector2Int& a)const
	{
		return x * a.x + y * a.y + z * a.z;
	}*/

	float Length()const
	{
		return x * x + y * y;
	}
	int Manhattan(const Vector2Int& other) const
	{
		return abs(x - other.x) + abs(y - other.y);
	}
};
#endif // VECTOR2INT_H