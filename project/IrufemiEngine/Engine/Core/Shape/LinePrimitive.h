#pragma once

#include "../Math/Vector2.h"
#include "../Math/Vector3.h"

struct Line {
	Vector3 origin; //!< 始点
	Vector3 diff;   //!< 終点への差分ベクトル
};

struct Ray {
	Vector3 origin; //!< 始点
	Vector3 diff;   //!< 終点への差分ベクトル
};

struct Segment2D {
	Vector2 origin; //!< 始点
	Vector2 end;   //!< 終点への差分ベクトル
};

struct Segment {
	Vector3 origin; //!< 始点
	Vector3 diff;   //!< 終点への差分ベクトル
};

