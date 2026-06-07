#pragma once

#include "../../Core/Math/Vector4.h"
#include "../../Core/Math/Matrix4x4.h"
#include <cstdint>

/**
 * @struct Material
 * @brief 統一マテリアル構造体
 * HLSL側の Material とメモリレイアウトを完全に一致させる
 */
struct Material {
    Vector4 color;              //!< ベースカラー
    int32_t enableLighting;      //!< ライティング有効フラグ
    int32_t hasTexture;          //!< テクスチャ有効フラグ
    int32_t lightingMode;        //!< 0:None, 1:Lambert, 2:Half-Lambert, 3:PBR
    float environmentCoefficient; //!< 環境マップの映り込み係数
    
    Matrix4x4 uvTransform;       //!< UV座標変換行列
    
    float metallic;              //!< 金属度
    float roughness;             //!< 粗さ
    int32_t useClampSampler;     //!< パーティクル等で使用するサンプラー切替 (0:WRAP, 1:CLAMP)
    float alphaReference;        //!< ディスカード閾値 (0.0f = 全部描画, 1.0f = 全部棄却)
};
