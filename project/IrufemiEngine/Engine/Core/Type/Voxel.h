#pragma once

#include "../Math/Vector3.h"
#include "../Math/Vector4.h"
#include "../Math/Vector2.h"

// ボクセル化された個々のキューブの情報
struct Voxel {
    Vector3 position; // ワールド空間での中心位置
    Vector3 normal;   // 元のモデルからサンプリングした法線
    Vector4 color;    // 元のテクスチャからサンプリングした色
    Vector2 uv;       // 元のモデルからサンプリングしたUV
};