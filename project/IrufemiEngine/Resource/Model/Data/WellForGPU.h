#pragma once

#include "Engine/Core/Math/Matrix4x4.h"

struct WellForGPU {
    Matrix4x4 skeletonSpaceMatrix; //位置用
    Matrix4x4 skeletonSpaceInverseTransposeMatrix; // 法線用
};