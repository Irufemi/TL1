#include "ShadowPass.h"
#include "../../../Manager/DrawManager.h"
#include "../../../IrufemiEngine.h"
#include "../../DirectX/ShadowMap.h"
#include "RenderGraphBuilder.h"

void ShadowPass::Setup(RenderGraphBuilder& builder, DrawManager* drawManager, IrufemiEngine* engine) {
    if (auto shadowMap = drawManager->GetShadowMap()) {
        builder.RequireState(shadowMap->GetResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
    }
}

void ShadowPass::Execute(DrawManager* drawManager, IrufemiEngine* engine) {
    drawManager->BeginShadowPass();

    auto DrawShadowsWithPSO = [&](const auto& queue, auto drawFunc) {
        if (queue.empty()) return;
        
        PSOManager::CullMode currentCull = PSOManager::CullMode::Back;
        bool first = true;
        
        for (const auto& p : queue) {
            if (!p.castShadows) continue;
            
            if (first || p.cullMode != currentCull) {
                engine->SetCull(p.cullMode);
                engine->ApplyPSO("Object3D"); // BeginShadowPass中なので自動的にShadowPSOが適用される
                currentCull = p.cullMode;
                first = false;
            }
            drawFunc(p);
        }
    };

    DrawShadowsWithPSO(drawManager->GetStandard3DQueue(), [&](const auto& p) { drawManager->DrawStandard3D(p); });
    DrawShadowsWithPSO(drawManager->GetPrimitiveRegionQueue(), [&](const auto& p) { drawManager->DrawPrimitiveRegion(p); });
    DrawShadowsWithPSO(drawManager->GetModelRegionQueue(), [&](const auto& p) { drawManager->DrawModelRegion(p); });

    drawManager->EndShadowPass();
}
