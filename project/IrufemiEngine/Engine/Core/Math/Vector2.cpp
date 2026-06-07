#include "Vector2.h"
#include <stdexcept>
#include <cassert>

float& Vector2::operator[](int index) {
	switch (index) {
	case 0: return x;
	case 1: return y;
	default: throw std::out_of_range("Vector2 index out of range");
	}
}

float Vector2::operator[](int index) const {
	switch (index) {
	case 0: return x;
	case 1: return y;
	default: throw std::out_of_range("Vector2 index out of range");
	}
}

Vector2& Vector2::operator+=(Vector2 rhs) {
	x += rhs.x;
	y += rhs.y;
	return *this;
}

Vector2& Vector2::operator-=(Vector2 rhs) {
	x -= rhs.x;
	y -= rhs.y;
	return *this;
}

Vector2& Vector2::operator*=(float s) {
	x *= s;
	y *= s;
	return *this;
}

Vector2& Vector2::operator/=(float s) {
	assert(s != 0.0f && "Division by zero");
	const float inv = 1.0f / s;
	x *= inv;
	y *= inv;
	return *this;
}

Vector2 operator+(Vector2 lhs, Vector2 rhs) {
	return { lhs.x + rhs.x, lhs.y + rhs.y };
}

Vector2 operator-(Vector2 lhs, Vector2 rhs) {
	return { lhs.x - rhs.x, lhs.y - rhs.y };
}

Vector2 operator+(Vector2 v) {
	return v;
}

Vector2 operator-(Vector2 v) {
	return { -v.x, -v.y };
}

Vector2 operator*(Vector2 v, float s) {
	return { v.x * s, v.y * s };
}

Vector2 operator*(float s, Vector2 v) {
	return { v.x * s, v.y * s };
}

Vector2 operator/(Vector2 v, float s) {
	assert(s != 0.0f && "Division by zero");
	const float inv = 1.0f / s;
	return { v.x * inv, v.y * inv };
}