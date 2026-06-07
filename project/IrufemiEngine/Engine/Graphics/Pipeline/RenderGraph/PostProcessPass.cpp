#include "PostProcessPass.h"
#include "../../../Manager/DrawManager.h"
#include "../../../IrufemiEngine.h"
#include "../../PostProcess/PostProcessManager.h"
#include "../../DirectX/DirectXCommon.h"
#include "../../DirectX/DirectXUtils.h"
#include "RenderGraph.h"
#include <cstring>
void PostProcessPass::Setup(RenderGraphBuilder& builder, DrawManager* drawManager, IrufemiEngine* engine) {
    auto ppMgr = engine->GetPostProcessManager();
    const auto& activeModes = ppMgr->GetActiveModes();
    auto mainRenderTex = engine->GetMainRenderTexture();

    // 入力となるメインレンダリング結果のステート要求
#ifdef EditorMode
    // EditorModeでは自身に書き戻すため、最終的な出力先として RENDER_TARGET を要求する
    builder.RequireState(mainRenderTex->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET);
#else
    builder.RequireState(mainRenderTex->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
#endif

    workTextureHandles_.clear();
    bloomExtractHandle_ = kInvalidHandle;
    bloomBlurHandle_ = kInvalidHandle;

    D3D12_RESOURCE_DESC desc = mainRenderTex->GetResource()->GetDesc();

#ifdef EditorMode
    // EditorMode 用の Src 退避テクスチャ (エフェクトの有無に関わらず常に必要)
    editorSrcHandle_ = builder.CreateTransientResource("PP_EditorSrc", desc);
    builder.RequireTransientState(editorSrcHandle_, D3D12_RESOURCE_STATE_COPY_DEST);
#endif

    if (!activeModes.empty()) {
        bool hasOutline = false;
        bool hasBloom = false;
        bool hasSeparableBlur = false;
        for (auto mode : activeModes) {
            if (mode == PostProcessMode::Bloom) hasBloom = true;
            if (mode == PostProcessMode::DepthBasedOutline) hasOutline = true;
            if (mode == PostProcessMode::Smoothing || mode == PostProcessMode::GaussianFilter) hasSeparableBlur = true;
        }

        if (hasOutline) {
            builder.RequireState(drawManager->GetDxCommon()->GetDepthStencilResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }
        
        // ピンポンバッファ用の一時テクスチャ (最大2枚)
        // ポストプロセスの中間計算はリニア空間で行うため、UNORM を指定する
        D3D12_RESOURCE_DESC workDesc = desc;
        workDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        workTextureHandles_.push_back(builder.CreateTransientResource("PP_Work0", workDesc));
        workTextureHandles_.push_back(builder.CreateTransientResource("PP_Work1", workDesc));

        if (hasBloom) {
            bloomExtractHandle_ = builder.CreateTransientResource("BloomExtract", workDesc);
        }
        if (hasBloom || hasSeparableBlur) {
            bloomBlurHandle_ = builder.CreateTransientResource("BloomBlur", workDesc);
        }

        // 初期ステートは全て SRV (内部で書き込み前に RTV に遷移させる)
        for (auto handle : workTextureHandles_) {
            builder.RequireTransientState(handle, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }
        if (hasBloom) {
            builder.RequireTransientState(bloomExtractHandle_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }
        if (hasBloom || hasSeparableBlur) {
            builder.RequireTransientState(bloomBlurHandle_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }
    }
}

void PostProcessPass::Execute(DrawManager* drawManager, IrufemiEngine* engine) {
    auto ppMgr = engine->GetPostProcessManager();
    auto* cmdList = drawManager->GetDxCommon()->GetCommandList();
    auto* renderGraph = drawManager->GetRenderGraph();

    PostProcessManager::PostProcessWorkspace workspace;
    
    if (!workTextureHandles_.empty()) {
        workspace.workTextures[0] = renderGraph->GetTransientRenderTexture(workTextureHandles_[0]);
        workspace.workTextures[1] = renderGraph->GetTransientRenderTexture(workTextureHandles_[1]);
    }
    if (bloomExtractHandle_ != kInvalidHandle) {
        workspace.bloomExtract = renderGraph->GetTransientRenderTexture(bloomExtractHandle_);
    }
    if (bloomBlurHandle_ != kInvalidHandle) {
        workspace.bloomBlur = renderGraph->GetTransientRenderTexture(bloomBlurHandle_);
    }

    // Outline のための逆投影行列更新
    const auto& activeModes = ppMgr->GetActiveModes();
    bool hasOutline = false;
    for (auto mode : activeModes) {
        if (mode == PostProcessMode::DepthBasedOutline) hasOutline = true;
    }
    if (hasOutline) {
        if (auto* perFrameData = drawManager->GetPerFrameData()) {
            ppMgr->GetOutlineParams().projectionInverse = Math::Inverse(perFrameData->camera.projection);
        }
    }

#ifdef EditorMode
    auto editorSrcTex = renderGraph->GetTransientRenderTexture(editorSrcHandle_);
    
    // CopyResource (mainRenderTex -> editorSrcTex)
    // RenderGraph によるステート管理のため開始時に RENDER_TARGET から COPY_SOURCE に手動で遷移
    DirectXUtils::TransitionBarrier(cmdList, engine->GetMainRenderTexture()->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);

    cmdList->CopyResource(editorSrcTex->GetResource(), engine->GetMainRenderTexture()->GetResource());

    // バリア遷移
    DirectXUtils::TransitionBarrier(cmdList, editorSrcTex->GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    renderGraph->RegisterResourceState(editorSrcTex->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    
    DirectXUtils::TransitionBarrier(cmdList, engine->GetMainRenderTexture()->GetResource(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    // mainRenderTexture は最終出力先として RENDER_TARGET に戻るので、RenderGraphの認識と一致する

    // EditorMode の場合、最終出力先は mainRenderTexture になる
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = engine->GetMainRenderTexture()->GetRtvHandle();
    ppMgr->Draw(cmdList, editorSrcTex, rtvHandle, workspace);
    
    // (末尾での PIXEL_SHADER_RESOURCE への手動遷移は削除。UIPass が RequireState で処理するため)
#else
    // 最終出力先はバックバッファ
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = drawManager->GetDxCommon()->GetRtvHandles(drawManager->GetDxCommon()->GetCurrentBackBufferIndex());
    
    // 通常モードでは mainRenderTexture は SRV 状態で渡される必要がある (Setup で RequireState 済み)
    ppMgr->Draw(cmdList, engine->GetMainRenderTexture(), rtvHandle, workspace);
#endif

    // 深度バッファを元の DEPTH_WRITE に戻す
    if (hasOutline) {
        DirectXUtils::TransitionBarrier(
            cmdList, drawManager->GetDxCommon()->GetDepthStencilResource(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES
        );
        renderGraph->RegisterResourceState(drawManager->GetDxCommon()->GetDepthStencilResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
    }
}
