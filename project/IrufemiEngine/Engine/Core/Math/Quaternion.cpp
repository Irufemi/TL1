#include "Quaternion.h"
#include <stdexcept>
#include <cassert>

float& Quaternion::operator[](int index) {
	switch (index) {
	case 0: return x;
	case 1: return y;
	case 2: return z;
	case 3: return w;
	default: throw std::out_of_range("Quaternion index out of range");
	}
}

float Quaternion::operator[](int index) const {
	switch (index) {
	case 0: return x;
	case 1: return y;
	case 2: return z;
	case 3: return w;
	default: throw std::out_of_range("Quaternion index out of range");
	}
}

Quaternion& Quaternion::operator+=(Quaternion rhs) {
	x += rhs.x;
	y += rhs.y;
	z += rhs.z;
	w += rhs.w;
	return *this;
}

Quaternion& Quaternion::operator-=(Quaternion rhs) {
	x -= rhs.x;
	y -= rhs.y;
	z -= rhs.z;
	w -= rhs.w;
	return *this;
}

Quaternion& Quaternion::operator*=(float s) {
	x *= s;
	y *= s;
	z *= s;
	w *= s;
	return *this;
}

Quaternion& Quaternion::operator/=(float s) {
	assert(s != 0.0f && "Division by zero");
	const float inv = 1.0f / s;
	x *= inv;
	y *= inv;
	z *= inv;
	w *= inv;
	return *this;
}

Quaternion operator+(Quaternion lhs, Quaternion rhs) {
	return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w };
}

Quaternion operator-(Quaternion lhs, Quaternion rhs) {
	return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w };
}

Quaternion operator+(Quaternion q) {
	return q;
}

Quaternion operator-(Quaternion q) {
	return { -q.x, -q.y, -q.z, -q.w };
}

Quaternion operator*(Quaternion q, float s) {
	return { q.x * s, q.y * s, q.z * s, q.w * s };
}

Quaternion operator*(float s, Quaternion q) {
	return q * s;
}

Quaternion operator/(Quaternion q, float s) {
	assert(s != 0.0f && "Division by zero");
	const float inv = 1.0f / s;
	return { q.x * inv, q.y * inv, q.z * inv, q.w * inv };
}

Quaternion operator*(const Quaternion& lhs, const Quaternion& rhs) {
    return {
        lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x,
        lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w,
        lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z
    };
}
