#pragma once

#include "Engine/Core/Math/QuaternionTransform.h"
#include "Engine/Core/Math/Matrix4x4.h"
#include <string>
#include <cstdint>
#include <vector>
#include <optional>

struct Joint {
    // Transform情報
    QuaternionTransform transform;
    // localMatrix
    Matrix4x4 localMatrix;
    // skeletonSpaceでの変換行列
    Matrix4x4 skeletonSpaceMatrix;
    // 名前
    std::string name;
    // 子JointのIndexのリスト。いなければ空
    std::vector<int32_t> children;
    // 自身のIndex
    int32_t index;
    // 親JointのIndex。いなければnull
    std::optional<int32_t> parent;
};