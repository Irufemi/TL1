#pragma once

#include "CameraForGPU.h"
#include "DirectionalLight.h"
#include "PointLight.h"
#include "SpotLight.h"
#include "AreaLight.h"
#include <cstdint>

/**
 * @struct PerFrameData
 * @brief フレーム全体で共有されるデータ構造体 (Camera, Timeなど)
 */
struct PerFrameData {
    CameraForGPU camera;        //!< カメラ情報 (view, projection, worldPosition)
    float time;                 //!< フレーム経過時間 (秒)
    float deltaTime;            //!< フレーム差分時間 (秒)
    float padding[2];           //!< パディング
};
