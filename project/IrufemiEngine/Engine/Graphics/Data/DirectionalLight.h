#pragma once

#include "../../Core/Math/Vector3.h"
#include "../../Core/Math/Vector4.h"

/*LambertianReflectance*/

struct DirectionalLight {
    //!< ライトの色
    Vector4 color;
    //!< ライトの向き
    Vector3 direction;
    //!< 輝度
    float intensity;
};