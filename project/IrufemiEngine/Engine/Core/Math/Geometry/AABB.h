#pragma once

#include <cstdint>
#include "../Vector3.h"

//AABB(Axis Sligned Bounding Box)
struct AABB {
    //!< 最小点
	Vector3 min{-1.0f, -1.0f, -1.0f};
    //!< 最大点
	Vector3 max{1.0f, 1.0f, 1.0f};
};