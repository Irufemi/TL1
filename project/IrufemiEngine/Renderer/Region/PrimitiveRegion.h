#pragma once

#include "BaseRegion.h"
#include "Engine/Manager/PrimitiveManager.h"

/**
 * @class PrimitiveRegion
 * @brief PrimitiveManager を利用した基本形状を描画する領域クラス
 */
class PrimitiveRegion : public BaseRegion {
public:
    PrimitiveRegion() = default;
    ~PrimitiveRegion() override = default;

    /**
     * @brief プリミティブ形状の領域を初期化する
     * @param type 生成する形状の種類
     * @param textureName 適用するテクスチャパス
     */
    void Initialize(PrimitiveType type, const std::string& textureName = "resources/uvChecker.png");

    /**
     * @brief リング形状専用の初期化
     */
    void InitializeRing(const RingParams& params, const std::string& textureName = "resources/uvChecker.png");

    void Draw() override;

protected:
    float GetBoundingSphereRadius() const override;

private:
    void EnsureMaterialResources();
    void EnsureSharedTexture(const std::string& textureName);
    
private:
    PrimitiveType type_ = PrimitiveType::Sphere;
    bool isCustomPrimitive_ = false; // リングなどの個別パラメータを使用するか
    PrimitiveResource customPrimitiveResource_; // カスタム用のリソース
};
