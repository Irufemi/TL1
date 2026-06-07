#pragma once

#include "IRenderPass.h"
#include <cstdint>

/**
 * @class SelectionOutlinePass
 * @brief 選択中のオブジェクトのマスクを描画し、シルエットのアウトラインを合成するパス
 */
class SelectionOutlinePass : public IRenderPass {
public:
    void Setup(RenderGraphBuilder& builder, class DrawManager* drawManager, class IrufemiEngine* engine) override;
    void Execute(class DrawManager* drawManager, class IrufemiEngine* engine) override;

private:
    uint32_t maskHandle_ = static_cast<uint32_t>(-1);
};
