#include "Ease.h"

#include "Engine/Core/Math/Math.h"
#include <algorithm>
#include <cmath>
#include <numbers>

using namespace Math;

float Lerp(float pos1, float pos2, float t	) {
	float result;
	result = (1.0f - t) * pos1 + t * pos2;
	return result;
}

// 線形補間
Vector2 Lerp(const Vector2& v1, const Vector2& v2, float t){
	return Add(Multiply(1.0f - t, v1), Multiply(t, v2));
}

// 線形補間
Vector3 Lerp(const Vector3& v1, const Vector3& v2, float t) { 
	return Add(Multiply(1.0f - t, v1), Multiply(t, v2));
}

// 線形補間
Vector4 Lerp(const Vector4& v1, const Vector4& v2, float t) {
	return Multiply(1.0f - t, v1) + Multiply(t, v2);
}

// Quaternion 線形補間(最短経路を選び正規化して返す)
Quaternion Lerp(const Quaternion& q0, const Quaternion& q1, float t) {
	// 最短経路のため内積を計算し、負なら q1 を反転
	float dot = q0.x * q1.x + q0.y * q1.y + q0.z * q1.z + q0.w * q1.w;
	Quaternion q1s = q1;
	if (dot < 0.0f) {
		q1s.x = -q1s.x;
		q1s.y = -q1s.y;
		q1s.z = -q1s.z;
		q1s.w = -q1s.w;
	}

	Quaternion res;
	res.x = q0.x + t * (q1s.x - q0.x);
	res.y = q0.y + t * (q1s.y - q0.y);
	res.z = q0.z + t * (q1s.z - q0.z);
	res.w = q0.w + t * (q1s.w - q0.w);

	// 正規化して返す(NLERP)
	return Normalize(res);
}

// 線形補間(0~1制限あり)
float LerpClamped(float a, float b, float t) {
	t = std::clamp(t, 0.0f, 1.0f);
	return Lerp(a, b, t);
}

// 線形補間(0~1制限あり)
Vector2 LerpClamped(const Vector2& v1, const Vector2& v2, float t) {
	t = std::clamp(t, 0.0f, 1.0f);
	return Lerp(v1, v2, t);
}

// 線形補間(0~1制限あり)
Vector3 LerpClamped(const Vector3& v1, const Vector3& v2, float t) {
	t = std::clamp(t, 0.0f, 1.0f);
	return Lerp(v1, v2, t);
}

// 球面線形補間
Vector3 Slerp(const Vector3& v1, const Vector3& v2, float t) {
	// --- ユーティリティ ---
	auto len = [](const Vector3& v) -> float { return std::sqrt(Dot(v, v)); };
	auto safeNormalize = [](const Vector3& v) -> Vector3 {
		float l = std::sqrt(Dot(v, v));
		if (l <= 1e-8f)
			return Vector3{0, 0, 0};
		return Multiply(1.0f / l, v);
	};

	// 長さ
	float l1 = len(v1);
	float l2 = len(v2);

	// どちらもゼロ長
	if (l1 <= 1e-8f && l2 <= 1e-8f)
		return Vector3{0, 0, 0};
	// 一方がゼロ長：もう片方の方向へ長さだけLerp
	if (l1 <= 1e-8f)
		return Multiply(std::clamp(t, 0.0f, 1.0f) * l2, safeNormalize(v2));
	if (l2 <= 1e-8f)
		return Multiply((1.0f - std::clamp(t, 0.0f, 1.0f)) * l1, safeNormalize(v1));

	// 単位方向
	Vector3 u1 = Multiply(1.0f / l1, v1);
	Vector3 u2 = Multiply(1.0f / l2, v2);

	// 角度
	float d = std::clamp(Dot(u1, u2), -1.0f, 1.0f);
	float theta = std::acos(d);
	float length = Lerp(l1, l2, t); // 長さは線形補間

	// 角度が極小なら nlerp 的に処理(sinθ ≒ 0 回避)
	if (theta < 1e-5f) {
		Vector3 dir = safeNormalize(Add(Multiply(1.0f - t, u1), Multiply(t, u2)));
		return Multiply(length, dir);
	}

	// 180°付近(無数の経路がある)対策：任意の直交方向を選んで回す
	if (std::abs(std::numbers::pi_v<float> - theta) < 1e-4f) {
		// u1 とほぼ平行でない基準軸を選ぶ
		Vector3 axis = (std::abs(u1.x) < 0.9f) ? Vector3{1, 0, 0} : Vector3{0, 1, 0};
		Vector3 ortho = safeNormalize(Cross(u1, axis));
		Vector3 vperp = safeNormalize(Cross(ortho, u1)); // u1 に直交する単位ベクトル
		Vector3 dir = Add(Multiply(std::cos(std::numbers::pi_v<float> * t), u1), Multiply(std::sin(std::numbers::pi_v<float> * t), vperp));
		return Multiply(length, dir);
	}

	// 通常のSlerp
	float sinTheta = std::sin(theta);
	float a = std::sin((1.0f - t) * theta) / sinTheta;
	float b = std::sin(t * theta) / sinTheta;
	Vector3 dir = Add(Multiply(a, u1), Multiply(b, u2));
	return Multiply(length, dir);
}

float EaseInSine(float num) { return 1.0f - std::cosf((num * std::numbers::pi_v<float> / 2.0f)); }

float EaseOutSine(float num) { return std::sinf(num * std::numbers::pi_v<float> / 2.0f); }

float EaseInOutSine(float num) { return -(std::cosf(std::numbers::pi_v<float> * num) - 1.0f) / 2.0f; }

float EaseInQuad(float num) { return num * num; }

float EaseOutQuad(float num) { return 1.0f - (1.0f - num) * (1.0f - num); }

float EaseInOutQuad(float num) { return num < 0.5f ? 2.0f * num * num : 1.0f - std::powf(-2.0f * num + 2.0f, 2.0f) / 2.0f; }

float EaseInCubic(float num) { return num * num * num; }

float EaseOutCubic(float num) { return 1.0f - std::powf(1.0f - num, 3.0f); }

float EaseInOutCubic(float num) { return num < 0.5f ? 4.0f * num * num * num : 1.0f - std::powf(-2.0f * num + 2.0f, 3.0f) / 2.0f; }

float EaseInQuart(float num) { return num * num * num * num; }

float EaseOutQuart(float num) { return 1.0f - std::powf(1.0f - num, 4.0f); }

float EaseInOutQuart(float num) { return num < 0.5f ? 8.0f * num * num * num * num : 1.0f - std::powf(-2.0f * num + 2.0f, 4.0f) / 2.0f; }

float EaseInQuint(float num) { return num * num * num * num * num; }

float EaseOutQuint(float num) { return 1.0f - std::powf(1.0f - num, 5.0f); }

float EaseInOutQuint(float num) { return num < 0.5f ? 16.0f * num * num * num * num * num : 1.0f - std::powf(-2.0f * num + 2.0f, 5.0f) / 2; }

float EvaluateEase(EaseType type, float t) {
    switch (type) {
        case EaseType::Linear:        return t;
        case EaseType::EaseInSine:    return EaseInSine(t);
        case EaseType::EaseOutSine:   return EaseOutSine(t);
        case EaseType::EaseInOutSine: return EaseInOutSine(t);
        case EaseType::EaseInQuad:    return EaseInQuad(t);
        case EaseType::EaseOutQuad:   return EaseOutQuad(t);
        case EaseType::EaseInOutQuad: return EaseInOutQuad(t);
        case EaseType::EaseInCubic:   return EaseInCubic(t);
        case EaseType::EaseOutCubic:  return EaseOutCubic(t);
        case EaseType::EaseInOutCubic:return EaseInOutCubic(t);
        case EaseType::EaseInQuart:   return EaseInQuart(t);
        case EaseType::EaseOutQuart:  return EaseOutQuart(t);
        case EaseType::EaseInOutQuart:return EaseInOutQuart(t);
        case EaseType::EaseInQuint:   return EaseInQuint(t);
        case EaseType::EaseOutQuint:  return EaseOutQuint(t);
        case EaseType::EaseInOutQuint:return EaseInOutQuint(t);
        default:                      return t;
    }
}
