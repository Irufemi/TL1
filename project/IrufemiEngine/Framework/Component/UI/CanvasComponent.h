#pragma once
#include "../Component.h"
#include <vector>

/**
 * @class CanvasComponent
 * @brief UI要素をグループ化し、一括で透明度を操作するキャンバスコンポーネント
 */
class CanvasComponent : public Component {
public:
    CanvasComponent() = default;
    ~CanvasComponent() override = default;

    void Initialize() override;
    void Update() override;
    
    std::string GetComponentName() const override { return "CanvasComponent"; }
    void OnRegisterProperties() override;

private:
    float groupAlpha_ = 1.0f; // グループ全体のアルファ値
};
