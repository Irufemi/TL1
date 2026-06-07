#pragma once
#include "Framework/Component/Component.h"
#include <vector>
#include <memory>

class GameObject;

/**
 * @class GravityPlayerComponent
 * @brief ガレキを引き寄せて投げる「重力スロー」アクションを制御するコンポーネント
 */
class GravityPlayerComponent : public Component {
public:
    GravityPlayerComponent() = default;
    ~GravityPlayerComponent() override = default;

    void Initialize() override;
    void Update() override;
    void OnRegisterProperties() override;
    std::string GetComponentName() const override { return "GravityPlayerComponent"; }

private:
    void HandlePullInput();
    void HandleThrowInput();

private:
    std::vector<std::shared_ptr<GameObject>> orbitingDebris_; ///< 現在プレイヤーの周囲を回転しているガレキのリスト
    int maxOrbitCount_ = 5; ///< 最大保持数
    float pullRadius_ = 100.0f; ///< 引き寄せ検知半径

    std::shared_ptr<GameObject> lockedTarget_ = nullptr;
    float lockonRadius2D_ = 200.0f; ///< スクリーン上のロックオン半径（ピクセル）
    
    void UpdateAim();
};
