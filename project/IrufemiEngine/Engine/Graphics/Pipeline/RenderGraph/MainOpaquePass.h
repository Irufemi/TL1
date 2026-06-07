#pragma once
#include "IRenderPass.h"

class MainOpaquePass : public IRenderPass {
public:
    void Setup(RenderGraphBuilder& builder, class DrawManager* drawManager, class IrufemiEngine* engine) override;
    void Execute(class DrawManager* drawManager, class IrufemiEngine* engine) override;
};
