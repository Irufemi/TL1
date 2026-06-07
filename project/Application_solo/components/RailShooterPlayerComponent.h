#pragma once
#include "Framework/Component/Component.h"
#include "Engine/Core/Math/Vector3.h"

class RailPathComponent;

/**
 * @class RailShooterPlayerComponent
 * @brief レールシューティング用のプレイヤー制御コンポーネント
 */
class RailShooterPlayerComponent : public Component {
public:
    RailShooterPlayerComponent() = default;
    ~RailShooterPlayerComponent() override = default;

    void Initialize() override;
    void Update() override;
    void OnRegisterProperties() override;
    std::string GetComponentName() const override { return "RailShooterPlayerComponent"; }

private:
    float progress_ = 0.0f;           ///< ルート（軌道）上の進み具合 (0.0〜1.0)
    float speed_ = 0.1f;              ///< 自動前進するスピード (1秒間に進む割合)
    float xySpeed_ = 10.0f;           ///< 上下左右に避ける（回避運動）スピード
    Vector3 currentOffset_ = {0,0,0}; ///< レールの中心からどのくらいずれているか（上下左右のズレ幅）

    // 画面内を動き回れる範囲（限界値）
    Vector3 moveLimitMin_ = {-10.0f, -10.0f, 0.0f}; ///< 移動できる限界の左下座標
    Vector3 moveLimitMax_ = { 10.0f,  10.0f, 0.0f}; ///< 移動できる限界の右上座標

    RailPathComponent* cachedPath_ = nullptr; ///< シーン内に置かれているルート情報の仮置き場

    // 軌道ポイントが引かれていないとき用の直進用データ
    Vector3 dummyBasePos_ = {0.0f, 0.0f, 0.0f};   ///< パスが無いときにまっすぐ進むための基準位置
    bool isDummyBasePosInitialized_ = false;       ///< まっすぐ進むための初期位置が決まったかどうかの判定フラグ
};
