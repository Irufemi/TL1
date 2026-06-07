#pragma once
#include <d3d12.h>

/**
 * @enum RootSlot
 * @brief ルートパラメータのスロットインデックスを定義します。
 * @details HLSL側での register(bX) や register(tX) とは独立した、
 *          ID3D12GraphicsCommandList::SetGraphicsRoot... で使用するインデックスです。
 *          現在は既存の構成を維持するため、マジックナンバーをそのままEnum化しています。
 */
enum class RootSlot : UINT {
    Material = 0,         ///< マテリアル (register b0) - PS
    Transform = 1,        ///< 座標変換行列 (register b0) - VS
    Texture = 2,          ///< メインテクスチャ (register t0) - PS
    LightCommon = 3,      ///< ライト共通データ (register b1) - VS/PS
    Instancing = 4,       ///< インスタンシング用 SRV (register t0) - VS
    Camera = 5,           ///< カメラ (register b2) - VS/PS
    Lights = 6,           ///< ライトSRVテーブル (register t2, t3, t4) - PS
    Special = 7,          ///< 特殊用 (GSなど) (register b6) - ALL
    EnvMap = 8,           ///< 環境マップ/深度 (register t1) - PS
    LineInstancing = 9,   ///< ライン用インスタンシング (register t1) - VS
    ShadowMap = 10,        ///< シャドウマップ (register t5) - PS
};
