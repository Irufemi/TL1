#include "MainTransparentPass.h"
#include "../../../Manager/DrawManager.h"
#include "../../../IrufemiEngine.h"
#include "../../DirectX/ShadowMap.h"
#include "RenderGraphBuilder.h"

void MainTransparentPass::Setup(RenderGraphBuilder& builder, DrawManager* drawManager, IrufemiEngine* engine) {
    if (auto shadowMap = drawManager->GetShadowMap()) {
        builder.RequireState(shadowMap->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }
}

void MainTransparentPass::Execute(DrawManager* drawManager, IrufemiEngine* engine) {
    auto DrawWithPSO = [&](const auto& queue, auto drawFunc, bool isParticle = false, bool isLine = false) {
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
                    if (isParticle) engine->ApplyPSO("Particle");
                    else if (isLine) engine->ApplyPSO("LineInstanced");
                }
                
                currentBlend = p.blendMode;
                currentDepth = p.depthWrite;
                currentCull = p.cullMode;
                currentCustomPSO = p.customPSO;
                currentCustomCBV = 0; // Force re-bind
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

    // 4. Line
    DrawWithPSO(drawManager->GetLineQueue(), [&](const auto& p) { drawManager->DrawLineInstanced(p); }, false, true);



    // 6. GPU Particles
    const auto& gpuParticleQueue = drawManager->GetGPUParticleQueue();
    if (!gpuParticleQueue.empty()) {
        BlendMode currentBlend = BlendMode::kBlendModeNormal;
        PSOManager::DepthWrite currentDepth = PSOManager::DepthWrite::Enable;
        PSOManager::CullMode currentCull = PSOManager::CullMode::Back;
        ID3D12PipelineState* currentCustomPSO = nullptr;
        bool psoApplied = false;
        bool first = true;
        for (const auto& p : gpuParticleQueue) {
            bool stateChanged = first || p.blendMode != currentBlend || p.depthWrite != currentDepth || p.cullMode != currentCull;
            bool psoChanged = (p.customPSO != currentCustomPSO);

            if (stateChanged || psoChanged || !psoApplied) {
                engine->SetBlend(p.blendMode);
                engine->SetDepthWrite(p.depthWrite);
                engine->SetCull(p.cullMode);
                
                if (p.customPSO) {
                    drawManager->BindPSO(p.customPSO);
                } else {
                    engine->ApplyPSO("GpuParticle");
                }
                
                currentBlend = p.blendMode; 
                currentDepth = p.depthWrite; 
                currentCull = p.cullMode;
                currentCustomPSO = p.customPSO;
                psoApplied = true;
                first = false;
            }
            drawManager->DrawGPUParticle(p);
        }
    }
    // 7. Voxel Particles
    const auto& voxelParticleQueue = drawManager->GetVoxelParticleQueue();
    if (!voxelParticleQueue.empty()) {
        for (const auto& p : voxelParticleQueue) {
            drawManager->DrawVoxelParticle(p);
        }
    }
}
