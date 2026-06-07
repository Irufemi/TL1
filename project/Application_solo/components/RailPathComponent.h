#pragma once
#include "Framework/Component/Component.h"
#include "Engine/Core/Math/Vector3.h"
#include <vector>

/**
 * @class RailPathComponent
 * @brief レールシューティング用の軌道（ウェイポイント）を管理するコンポーネント
 */
class RailPathComponent : public Component {
public:
    RailPathComponent() = default;
    ~RailPathComponent() override = default;

    void OnRegisterProperties() override;
    std::string GetComponentName() const override { return "RailPathComponent"; }

    /**
     * @brief 軌道上の進行度 t (0.0 ~ 1.0) に対する座標を取得する (Catmull-Rom スプライン補間)
     * @param t 進行度 (0.0 ~ 1.0)
     * @return 補間された座標
     */
    Vector3 GetPointAt(float t) const;

    /**
     * @brief 軌道上の進行度 t に対する進行方向（接線ベクトル）を取得する
     * @param t 進行度 (0.0 ~ 1.0)
     * @return 正規化された進行方向ベクトル
     */
    Vector3 GetTangentAt(float t) const;

    const std::vector<Vector3>& GetWaypoints() const { return waypoints_; }

private:
    std::vector<Vector3> waypoints_;
};
