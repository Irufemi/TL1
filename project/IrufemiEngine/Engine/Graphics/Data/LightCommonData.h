#pragma once

#include "DirectionalLight.h"
#include "../../Core/Math/Matrix4x4.h"
#include <cstdint>

/**
 * @struct LightCommonData
 * @brief ライトに関連する共通情報を格納する定数バッファ用構造体
 */
struct LightCommonData {
    DirectionalLight directionalLight; //!< 平行光源 (1体固定)
    Matrix4x4 viewProjection;          //!< ライト視点の投影行列 (ShadowMap用)
    uint32_t pointLightCount;          //!< 有効な点光源の数
    uint32_t spotLightCount;           //!< 有効なスポットライトの数
    uint32_t areaLightCount;           //!< 有効なエリアライトの数
    uint32_t padding;                  //!< パディング (16進アライメント用)
};
