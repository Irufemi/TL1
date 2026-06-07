#pragma once
#include "Engine/Core/Math/Matrix4x4.h"

struct PerView {
    Matrix4x4 viewProjection;
    Matrix4x4 billboardMatrix;
    Vector3 worldPosition;
    float pad;
};