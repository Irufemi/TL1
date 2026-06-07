#include "SelectionOutlinePass.h"
#include "RenderGraph.h"
#include "../../../Manager/DrawManager.h"
#include "../../../IrufemiEngine.h"
#include "../../DirectX/DirectXCommon.h"
#include "../../DirectX/DirectXUtils.h"
#include "../../DirectX/RenderTexture.h"

void SelectionOutlinePass::Setup(RenderGraphBuilder& builder, DrawManager* drawManager, IrufemiEngine* engine) {
#ifdef EditorMode
    if (drawManager->GetSelectionMaskQueue().empty() && drawManager->GetSelectionMaskQueue2D().empty()) return;
    
    // MainRenderTarget を更新する (合成用)
    builder.RequireState(engine->GetMainRenderTexture()->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET);
    
    // Mask用 Transient Resource 作成
    D3D12_RESOURCE_DESC desc = engine->GetMainRenderTexture()->GetResource()->GetDesc();
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    maskHandle_ = builder.CreateTransientResource("SelectionMask", desc);
    builder.RequireTransientState(maskHandle_, D3D12_RESOURCE_STATE_RENDER_TARGET);
#endif
}

void SelectionOutlinePass::Execute(DrawManager* drawManager, IrufemiEngine* engine) {
#ifdef EditorMode
    const auto& queue3D = drawManager->GetSelectionMaskQueue();
    const auto& queue2D = drawManager->GetSelectionMaskQueue2D();
    if (queue3D.empty() && queue2D.empty()) return;

    auto* cmdList = drawManager->GetDxCommon()->GetCommandList();
    auto* psoManager = drawManager->GetDxCommon()->GetPSOManager();
    auto* renderGraph = drawManager->GetRenderGraph();
    auto maskTex = renderGraph->GetTransientRenderTexture(maskHandle_);

    // 1. マスクの描画
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = maskTex->GetRtvHandle();
    cmdList->OMSetRenderTargets(1, &rtvHandle, false, nullptr);
    float clearColor[] = {0.0f, 0.0f, 0.0f, 0.0f};
    cmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    // PSOをバインド (SelectionMask)
    if (!queue3D.empty()) {
        auto pso = psoManager->GetPSO("SelectionMask", BlendMode::kBlendModeNone, PSOManager::DepthWrite::Off, PSOManager::CullMode::None);
        if (pso) {
            cmdList->SetPipelineState(pso);
            for (const auto& p : queue3D) {
                drawManager->DrawStandard3D(p);
            }
        }
    }
    
    // 2D Text用マスク描画
    if (!queue2D.empty()) {
        auto psoTextMask = psoManager->GetPSO("SelectionMaskText", BlendMode::kBlendModeNone, PSOManager::DepthWrite::Off, PSOManager::CullMode::None);
        if (psoTextMask) {
            cmdList->SetPipelineState(psoTextMask);
            for (const auto& p : queue2D) {
                // customPSOを使わずに、強制的に psoTextMask で描画するため DrawText 内部ではなくここで設定しているが、
                // DrawText内部で packet.customPSO が優先されるのを防ぐため packet.customPSO を一時的に psoTextMask にして渡すか、
                // DrawSprite を使う
                RenderPackets::SpritePacket pOverride = p;
                pOverride.customPSO = psoTextMask;
                drawManager->DrawText(pOverride);
            }
        }
    }

    // 2. アウトラインの合成描画
    DirectXUtils::TransitionBarrier(cmdList, maskTex->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    // RenderGraphに最新のステートを通知し、次フレームで正しく遷移バリアが張られるようにする
    renderGraph->RegisterResourceState(maskTex->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    D3D12_CPU_DESCRIPTOR_HANDLE mainRtv = engine->GetMainRenderTexture()->GetRtvHandle();
    cmdList->OMSetRenderTargets(1, &mainRtv, false, nullptr);

    auto compPso = psoManager->GetPSO("OutlineComposite", BlendMode::kBlendModeAdd, PSOManager::DepthWrite::Off, PSOManager::CullMode::None);
    if (compPso) {
        cmdList->SetPipelineState(compPso);
        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmdList->SetGraphicsRootDescriptorTable((UINT)RootSlot::Texture, maskTex->GetSrvHandleGPU());
        // 3頂点でフルスクリーンを描画
        cmdList->DrawInstanced(3, 1, 0, 0);
    }
#endif
}
