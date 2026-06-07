#pragma once
#include "IRenderPass.h"

class MainTransparentPass : public IRenderPass {
public:
    void Setup(RenderGraphBuilder& builder, class DrawManager* drawManager, class IrufemiEngine* engine) override;
    void Execute(class DrawManager* drawManager, class IrufemiEngine* engine) override;
};
