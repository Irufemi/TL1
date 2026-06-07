#pragma once
#include "Framework/Component/Component.h"

/**
 * @class RailShooterEnemyComponent
 * @brief レールシューティング用の敵キャラクター制御コンポーネント
 */
class RailShooterEnemyComponent : public Component {
public:
    RailShooterEnemyComponent() = default;
    ~RailShooterEnemyComponent() override = default;

    void Initialize() override;
    void Update() override;
    void OnRegisterProperties() override;
    std::string GetComponentName() const override { return "RailShooterEnemyComponent"; }

    bool IsAlive() const { return hp_ > 0 && isActive_; }
    void TakeDamage(int damage);

private:
    float spawnProgress_ = 0.5f; ///< プレイヤーがどの進行度に達したらアクティブになるか (0.0 ~ 1.0)
    bool isActive_ = false;      ///< 現在活動中かどうか
    float speed_ = 5.0f;         ///< 敵の移動速度
    int hp_ = 100;               ///< 耐久力
};
