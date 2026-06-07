#pragma once

#include "../Engine/Core/Math/Vector2.h"
#include "../Engine/Core/Math/Vector3.h"
#include "../Engine/Core/Math/Vector4.h"

struct VertexData {
    Vector4 position;
    Vector2 texcoord;
    Vector3 normal;
    Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f }; ///< [追加] 頂点カラー
};