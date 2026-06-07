#pragma once

#include "../../Core/Math/Matrix4x4.h"
#include "../../Core/Math/Vector3.h"

struct CameraForGPU {
  Matrix4x4 view = {};
  Matrix4x4 projection = {};
  Vector3 worldPosition = {};
};
