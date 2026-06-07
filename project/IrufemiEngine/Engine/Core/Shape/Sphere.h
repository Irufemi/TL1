#pragma once

#include <cstdint>
#include "../Math/Vector3.h"

struct Sphere {
    //!< 中心点
	Vector3 center = {0.0f, 0.0f, 0.0f};

    //!< 半径
    float radius = 1.0f;
};

