#include "Vector4.h"

Vector4& Vector4::operator+=(Vector4 other)
{
	x += other.x;
	y += other.y;
	z += other.z;
	w += other.w;
	return *this;
}

Vector4& Vector4::operator-=(Vector4 other)
{
	x -= other.x;
	y -= other.y;
	z -= other.z;
	w -= other.w;
	return *this;
}

Vector4& Vector4::operator*=(float s)
{
	x *= s;
	y *= s;
	z *= s;
	w *= s;
	return *this;
}

Vector4& Vector4::operator/=(float s)
{
	x /= s;
	y /= s;
	z /= s;
	w /= s;
	return *this;
}

Vector4 Vector4::operator+() const
{
	return *this;
}

Vector4 Vector4::operator-() const
{
	return {-x, -y, -z, -w};
}

Vector4 operator+(Vector4 v1, Vector4 v2)
{
	return { v1.x + v2.x, v1.y + v2.y, v1.z + v2.z, v1.w + v2.w };
}

Vector4 operator-(Vector4 v1, Vector4 v2)
{
	return { v1.x - v2.x, v1.y - v2.y, v1.z - v2.z, v1.w - v2.w };
}

Vector4 operator*(Vector4 v, float s)
{
	return { v.x * s, v.y * s, v.z * s, v.w * s };
}

Vector4 operator*(float s, Vector4 v)
{
	return v * s;
}

Vector4 operator/(Vector4 v, float s)
{
	return { v.x / s, v.y / s, v.z / s, v.w / s };
}


