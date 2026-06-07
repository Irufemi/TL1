#include "MainOpaquePass.h"
#include "../../../Manager/DrawManager.h"
#include "../../../IrufemiEngine.h"
#include "../../DirectX/ShadowMap.h"
#include "RenderGraphBuilder.h"

void MainOpaquePass::Setup(RenderGraphBuilder& builder, DrawManager* drawManager, IrufemiEngine* engine) {
    if (auto shadowMap = drawManager->GetShadowMap()) {
        builder.RequireState(shadowMap->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }
}

void MainOpaquePass::Execute(DrawManager* drawManager, IrufemiEngine* engine) {
    // 1. Skybox
    const auto& skyboxQueue = drawManager->GetSkyboxQueue();
    if (!skyboxQueue.empty()) {
        engine->ApplyPSO("Skybox");
        for (const auto& p : skyboxQueue) {
            drawManager->DrawSkybox(p);
        }
    }
    
    // Helper lambda to apply PSO efficiently
    auto DrawWithPSO = [&](const auto& queue, auto applyPSOFunc, auto drawFunc) {
        if (queue.empty()) return;
        
        BlendMode currentBlend = BlendMode::kBlendModeNormal;
        PSOManager::DepthWrite currentDepth = PSOManager::DepthWrite::Enable;
        PSOManager::CullMode currentCull = PSOManager::CullMode::Back;
        ID3D12PipelineState* currentCustomPSO = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS currentCustomCBV = 0;
        bool psoApplied = false;
        bool first = true;
        
        for (const auto& p : queue) {
            bool stateChanged = first || p.blendMode != currentBlend || p.depthWrite != currentDepth || p.cullMode != currentCull;
            bool psoChanged = (p.customPSO != currentCustomPSO);
            
            if (stateChanged || psoChanged || !psoApplied) {
                engine->SetBlend(p.blendMode);
                engine->SetDepthWrite(p.depthWrite);
                engine->SetCull(p.cullMode);
                
                if (p.customPSO) {
                    drawManager->BindPSO(p.customPSO);
                } else {
                    applyPSOFunc();
                }
                
                currentBlend = p.blendMode;
                currentDepth = p.depthWrite;
                currentCull = p.cullMode;
                currentCustomPSO = p.customPSO;
                currentCustomCBV = 0; // Force re-bind on state/PSO change
                psoApplied = true;
                first = false;
            }

            if (p.customCBVAddress != 0 && p.customCBVAddress != currentCustomCBV) {
                engine->BindLightningParams(p.customCBVAddress);
                currentCustomCBV = p.customCBVAddress;
            }

            drawFunc(p);
        }
    };
    
    // 2. Standard 3D (Opaque and Alpha blend)
    DrawWithPSO(drawManager->GetStandard3DQueue(), [&]() { engine->ApplyPSO("Object3D"); }, [&](const auto& p) { drawManager->DrawStandard3D(p); });
    
    // PrimitiveRegion
    DrawWithPSO(drawManager->GetPrimitiveRegionQueue(), [&]() { engine->ApplyPSO("Region"); }, [&](const auto& p) { drawManager->DrawPrimitiveRegion(p); });

    // ModelRegion
    DrawWithPSO(drawManager->GetModelRegionQueue(), [&]() { engine->ApplyPSO("Region"); }, [&](const auto& p) { drawManager->DrawModelRegion(p); });
}
