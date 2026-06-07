#pragma once
#include "IRenderPass.h"
#include "RenderGraphBuilder.h"
#include <vector>

class PostProcessPass : public IRenderPass {
public:
    PostProcessPass() = default;
    ~PostProcessPass() override = default;

    void Setup(class RenderGraphBuilder& builder, class DrawManager* drawManager, class IrufemiEngine* engine) override;
    void Execute(class DrawManager* drawManager, class IrufemiEngine* engine) override;

private:
    std::vector<TransientResourceHandle> workTextureHandles_;
    TransientResourceHandle bloomExtractHandle_ = kInvalidHandle;
    TransientResourceHandle bloomBlurHandle_ = kInvalidHandle;

#ifdef EditorMode
    TransientResourceHandle editorSrcHandle_ = kInvalidHandle;
#endif
};
