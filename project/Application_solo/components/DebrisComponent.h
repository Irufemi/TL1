#pragma once
#include "Framework/Component/Component.h"
#include "Engine/Core/Math/Vector3.h"

class DebrisManagerComponent;

enum class DebrisState {
    Idle,       ///< 漂流中（自然な浮遊）
    Pulled,     ///< プレイヤーに引き寄せられている
    Orbiting,   ///< プレイヤーの周囲を回転浮遊中
    Thrown      ///< 敵へ向かってホーミング中
};

/**
 * @class DebrisComponent
 * @brief ガレキの振る舞い（状態遷移と位置の補間）を制御するコンポーネント
 */
class DebrisComponent : public Component {
public:
    DebrisComponent() = default;
    ~DebrisComponent() override = default;

    void Initialize() override;
    void Update() override;
    void OnRegisterProperties() override;
    std::string GetComponentName() const override { return "DebrisComponent"; }

    // 状態変更用のインターフェース
    void SetState(DebrisState newState) { state_ = newState; }
    DebrisState GetState() const { return state_; }

    void SetTarget(GameObject* target) { targetObject_ = target; }
    void SetOrbitParams(float angle, float radius) { orbitAngle_ = angle; orbitRadius_ = radius; }
    void SetThrowDirection(const Vector3& dir) { throwDirection_ = dir; }

    void SetVirtualId(int id) { virtualId_ = id; }
    void SetManager(DebrisManagerComponent* manager) { manager_ = manager; }

private:
    DebrisState state_ = DebrisState::Idle;
    
    int virtualId_ = -1;
    DebrisManagerComponent* manager_ = nullptr;
    
    // 追従・目標用の対象
    GameObject* targetObject_ = nullptr;
    
    // Orbiting（疑似浮遊）用のパラメータ
    float orbitAngle_ = 0.0f;
    float orbitRadius_ = 2.0f;
    float orbitSpeed_ = 2.0f;
    
    // 各種移動用のパラメータ
    float pullSpeed_ = 10.0f;
    float throwSpeed_ = 50.0f;
    Vector3 throwDirection_ = {0.0f, 0.0f, 1.0f};
    
    // Idle時のフワフワアニメーション用
    float idleTimeY_ = 0.0f;
    float baseIdleY_ = 0.0f;
};
