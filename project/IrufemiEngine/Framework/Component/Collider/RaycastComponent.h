#pragma once
#include "Framework/Component/Component.h"
#include "Engine/Core/Math/Vector3.h"
#include "Engine/Manager/CollisionManager.h"
#include <functional>

class TransformComponent;

/**
 * @class RaycastComponent
 * @brief 自身の位置から指定した方向へレイを飛ばし、オブジェクトを検知するセンサーコンポーネント
 */
class RaycastComponent : public Component {
public:
    RaycastComponent() = default;
    ~RaycastComponent() = default;

    void Initialize() override;
    void Update() override;
    void DrawDebug(); // ComponentにはDrawDebugがないためoverrideを外す
#ifdef EditorMode
    friend class RaycastComponentEditor;
#endif
    nlohmann::json Serialize() override;
    void Deserialize(const nlohmann::json& j) override;

    /// @brief 現在レイが何かに当たっているかを取得する
    bool IsHit() const { return hitInfo_.isHit; }
    
    /// @brief 当たったオブジェクトの情報を取得する
    const RaycastHit& GetHitInfo() const { return hitInfo_; }

    // 設定
    Vector3 localOffset_ = { 0.0f, 0.0f, 0.0f };
    Vector3 localDirection_ = { 0.0f, 0.0f, 1.0f }; // ローカルZ軸方向
    float maxDistance_ = 100.0f;
    uint32_t mask_ = 0xFFFFFFFF; // 全てのレイヤーと判定

    bool showDebugLine_ = true;

    // 当たった時に呼ばれるコールバック
    std::function<void(const RaycastHit&)> onHit_;

private:
    TransformComponent* transform_ = nullptr;
    RaycastHit hitInfo_;
    Ray currentRay_;
};
