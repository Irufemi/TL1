#pragma once
#include "Framework/Component/Component.h"
#include "Engine/Core/Math/Vector3.h"

class TransformComponent;

/**
 * @class CameraFollowPlayerComponent
 * @brief プレイヤーを追従するゲーム特化型カメラ制御コンポーネント
 * @details プレイヤーの現在位置・向きをベースに、滑らかな遅延追従（Lerp）および姿勢制御を行います。
 */
class CameraFollowPlayerComponent : public Component {
public:
    CameraFollowPlayerComponent() = default;
    ~CameraFollowPlayerComponent() override = default;

    void Initialize() override;
    void Update() override;

    std::string GetComponentName() const override { return "CameraFollowPlayerComponent"; }

protected:
    void OnRegisterProperties() override;

private:
    TransformComponent* targetTransform_ = nullptr; ///< 追従ターゲット (Player) のTransform

    // インスペクター調整可能プロパティ
    Vector3 offset_ = { 0.0f, 4.0f, -18.0f }; ///< プレイヤーに対するカメラの相対位置オフセット
    float followDelay_ = 0.01f;                ///< 追従の遅れ (Lerp値。1フレームに進む割合。0に近づくほど補間が強くなります)
};
