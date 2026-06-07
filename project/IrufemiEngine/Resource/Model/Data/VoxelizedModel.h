#pragma once

#include "../../../Engine/Core/Type/Voxel.h"
#include "../../../Engine/Core/Math/Vector3.h"
#include "../../../Engine/Core/Math/Vector3Int.h"
#include <vector>

// ボクセル化されたモデル全体を管理する構造体
struct VoxelizedModel {
    std::vector<Voxel> voxels;
    Vector3 aabbMin;
    Vector3 aabbMax;
    Vector3Int resolution;
};