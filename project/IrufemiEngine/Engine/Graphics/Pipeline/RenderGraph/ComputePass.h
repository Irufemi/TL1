#pragma once

#include "IRenderPass.h"

/**
 * @class ComputePass
 * @brief RenderGraph の最初のパスとして、コンピュートシェーダを一括実行するパス
 */
class ComputePass : public IRenderPass {
public:
    ~ComputePass() override = default;

    void Setup(class RenderGraphBuilder& builder, class DrawManager* drawManager, class IrufemiEngine* engine) override {}
    void Execute(class DrawManager* drawManager, class IrufemiEngine* engine) override;
};
