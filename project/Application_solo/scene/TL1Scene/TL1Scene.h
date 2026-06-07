#pragma once

#include "Framework/BaseScene.h"
#include <memory>

class IrufemiEngine;

/**
 * @class TL1Scene
 * @brief JSONからレベルデータを読み込んで配置するテスト用シーン
 */
class TL1Scene : public BaseScene {
public:
    ~TL1Scene() override;

    void Initialize(IrufemiEngine* engine) override;
    void Update() override;
    void Draw() override;
    void DrawDebugTab() override;

private:
    // 必要に応じてメンバ変数を追加する場合は variableName_ の形式にする
};
