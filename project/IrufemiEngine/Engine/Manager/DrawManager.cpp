#include "DrawManager.h"
using namespace RenderPackets;

#include<Windows.h>
#include <cassert>

#include <dxgidebug.h>
#include "Renderer/Object2D/Sprite/Sprite.h"
#include "Renderer/Object3D/StaticModelObject/StaticModelObject.h"
#include "Renderer/Region/ModelRegion.h"
#include "Renderer/Region/PrimitiveRegion.h"

#include "Renderer/LineInstanced/LineClass.h"
#include "Renderer/Skybox//Skybox.h"
#include "Renderer/Object3D/Object3DResource.h"
#include "Renderer/Object2D/Object2DResource.h"

#include "Renderer/LineInstanced/LineResource.h"
#include "../Graphics/DirectX/DirectXCommon.h"
#include "../Graphics/DirectX/DirectXUtils.h"
#include "../Graphics/Pipeline/RenderGraph/RenderGraph.h"
#include "../Graphics/Pipeline/RenderGraph/ComputePass.h"
#include "../Graphics/Pipeline/RenderGraph/ShadowPass.h"
#include "../Graphics/Pipeline/RenderGraph/MainOpaquePass.h"
#include "../Graphics/Pipeline/RenderGraph/MainTransparentPass.h"
#include "../Graphics/Pipeline/RenderGraph/UIPass.h"
#include "../Graphics/Pipeline/RenderGraph/PostProcessPass.h"
#include "../Graphics/Pipeline/RenderGraph/SelectionOutlinePass.h"
#include "../../Resource/Model/ModelManager.h"
#include "../../engine/IrufemiEngine.h"
#include "../Graphics/Data/CameraForGPU.h"
#include "../Graphics/Data/DirectionalLight.h"
#include "../Graphics/Data/PointLight.h"
#include "../Graphics/Data/SpotLight.h"
#include "../Graphics/Data/AreaLight.h"
#include "../Core/Math/Math.h"
#include "../../Resource/Model/Data/SkinCluster.h"
#include "../Graphics/DirectX/ShadowMap.h"
#include "../../Resource/Texture/TextureManager.h"


namespace {
    // cpp内限定のヌルCBV
    Microsoft::WRL::ComPtr<ID3D12Resource> gNullPointLight;
    Microsoft::WRL::ComPtr<ID3D12Resource> gNullSpotLight;
    D3D12_GPU_VIRTUAL_ADDRESS gNullPointLightVA = 0;
    D3D12_GPU_VIRTUAL_ADDRESS gNullSpotLightVA = 0;

    void EnsureNullPointLight(DirectXCommon* dx) {
        if (gNullPointLight) return;
        gNullPointLight = dx->CreateBufferResource(sizeof(PointLight));
        PointLight* p = nullptr;
        gNullPointLight->Map(0, nullptr, reinterpret_cast<void**>(&p));
        p->color = { 0,0,0,0 }; p->position = { 0,0,0 }; p->intensity = 0.0f;
        gNullPointLightVA = gNullPointLight->GetGPUVirtualAddress();
    }

    void EnsureNullSpotLight(DirectXCommon* dx) {
        if (gNullSpotLight) return;
        gNullSpotLight = dx->CreateBufferResource(sizeof(SpotLight));
        SpotLight* s = nullptr;
        gNullSpotLight->Map(0, nullptr, reinterpret_cast<void**>(&s));
        s->color = { 0,0,0,0 };
        s->position = { 0,0,0 };
        s->intensity = 0.0f;
        s->direction = { 0, -1, 0 };
        s->distance = 0.0f;
        s->decay = 1.0f;
        s->cosAngle = 1.0f;
        gNullSpotLightVA = gNullSpotLight->GetGPUVirtualAddress();
    }
} // anonymous

DrawManager::DrawManager() {}
DrawManager::~DrawManager() {}

void DrawManager::Initialize(DirectXCommon* dx) {
    dxCommon_ = dx;
    commandList_ = dx->GetCommandList();

    // 定数バッファのサイズ (256バイトアラインメント)
    const size_t perFrameSize = (sizeof(PerFrameData) + D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1) & ~(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1);
    const size_t lightCommonSize = (sizeof(LightCommonData) + D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1) & ~(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1);
    const uint32_t kMaxLights = 1024;

    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        auto& fr = frameResources_[i];

        // フレーム定数バッファ (PerFrameData + LightCommonData)
        fr.frameResource = dxCommon_->CreateBufferResource(perFrameSize + lightCommonSize);
        uint8_t* mapped = nullptr;
        fr.frameResource->Map(0, nullptr, reinterpret_cast<void**>(&mapped));

        fr.perFrameData = reinterpret_cast<PerFrameData*>(mapped);
        fr.lightCommonData = reinterpret_cast<LightCommonData*>(mapped + perFrameSize);

        fr.frameData.camera = fr.frameResource->GetGPUVirtualAddress();
        fr.frameData.lightCommon = fr.frameData.camera + perFrameSize;

        // StructuredBuffer の初期化 (ひとまず1024個分を確保)
        fr.pointLightResource = dxCommon_->CreateBufferResource(sizeof(PointLight) * kMaxLights);
        fr.spotLightResource = dxCommon_->CreateBufferResource(sizeof(SpotLight) * kMaxLights);
        fr.areaLightResource = dxCommon_->CreateBufferResource(sizeof(AreaLight) * kMaxLights);

        // ライト SRV デスクリプタの一括確保 (Point, Spot, Area 用に 3 つ)
        auto pool = dxCommon_->GetSrvPool();
        fr.lightSrvBaseIndex = pool->Allocate(3);
        fr.lightSrvHandle = pool->GetGPUHandle(fr.lightSrvBaseIndex);

        // StructuredBuffer SRV の作成
        pool->CreateSRVForStructuredBuffer(fr.lightSrvBaseIndex + 0, fr.pointLightResource.Get(), kMaxLights, sizeof(PointLight));
        pool->CreateSRVForStructuredBuffer(fr.lightSrvBaseIndex + 1, fr.spotLightResource.Get(), kMaxLights, sizeof(SpotLight));
        pool->CreateSRVForStructuredBuffer(fr.lightSrvBaseIndex + 2, fr.areaLightResource.Get(), kMaxLights, sizeof(AreaLight));
    }

    // レンダーグラフの構築
    renderGraph_ = std::make_unique<RenderGraph>();
    renderGraph_->InitializeTransientResourceManager(dxCommon_);
    
    renderGraph_->AddPass(std::make_unique<ComputePass>());
    renderGraph_->AddPass(std::make_unique<ShadowPass>());
    renderGraph_->AddPass(std::make_unique<MainOpaquePass>());
    renderGraph_->AddPass(std::make_unique<MainTransparentPass>());
    renderGraph_->AddPass(std::make_unique<UIPass>());
    renderGraph_->AddPass(std::make_unique<PostProcessPass>());
    renderGraph_->AddPass(std::make_unique<SelectionOutlinePass>());

    // シャドウマップの初期化 (2048x2048) - 全フレーム分
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        shadowMaps_[i] = std::make_unique<ShadowMap>();
        shadowMaps_[i]->Initialize(dxCommon_, 2048, 2048);
        
        // RenderGraph にリソースの初期ステートを登録
        renderGraph_->RegisterResourceState(shadowMaps_[i]->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }
}

void DrawManager::ExecuteComputePasses() {
    for (auto* task : computeTasks_) {
        task->DispatchCompute();
    }
    
    // パイプラインのボトルネック解消のため、各モデルごとではなく
    // 全てのコンピュートタスクのディスパッチ完了後に一括してグローバルUAVバリアを発行する
    if (!computeTasks_.empty()) {
        ExecuteUAVBarrier(nullptr);
    }
    
    computeTasks_.clear();
}

void DrawManager::Finalize() {
    auto* srvPool = dxCommon_->GetSrvPool();
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        auto& fr = frameResources_[i];
        if (fr.frameResource && fr.perFrameData) {
            fr.frameResource->Unmap(0, nullptr);
            fr.frameResource.Reset();
        }
        fr.pointLightResource.Reset();
        fr.spotLightResource.Reset();
        fr.areaLightResource.Reset();

        // SRVの解放
        if (srvPool && fr.lightSrvBaseIndex != 0xFFFFFFFFu) {
            for (uint32_t j = 0; j < 3; ++j) {
                srvPool->FreeAfterFence(fr.lightSrvBaseIndex + j, dxCommon_->GetFenceValue(i));
            }
            fr.lightSrvBaseIndex = 0xFFFFFFFFu;
        }
    }

    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        shadowMaps_[i].reset();
    }

    shadowMaps_[0].reset();
    shadowMaps_[1].reset();
}

void DrawManager::OnResize(int32_t width, int32_t height) {
    if (renderGraph_) {
        renderGraph_->OnResize();

        // 永続リソースであるシャドウマップのステートも再登録する
        for (int i = 0; i < kMaxFramesInFlight; ++i) {
            if (shadowMaps_[i]) {
                // フレーム完了時点ではSRV状態になっているため、その状態を登録
                renderGraph_->RegisterResourceState(shadowMaps_[i]->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            }
        }
    }
}

void DrawManager::RegisterResourceState(ID3D12Resource* resource, D3D12_RESOURCE_STATES state) {
    if (renderGraph_) {
        renderGraph_->RegisterResourceState(resource, state);
    }
}

void DrawManager::BindPSO(ID3D12PipelineState* pso) {
    if (pso) {
        commandList_->SetPipelineState(pso);
    }
}

void DrawManager::PreDraw(std::array<float, 4> clearColor, float clearDepth, uint8_t clearStencil) {

    // 1. GPU同期 (これから使うスロットが前回の使用（通し番号）を終えるまで待つ)
    ID3D12Fence* fence = dxCommon_->GetFence();
    uint64_t waitValue = dxCommon_->GetFenceValue(); // このスロットが最後に使われた時の通し番号
    if (fence->GetCompletedValue() < waitValue) {
        fence->SetEventOnCompletion(waitValue, dxCommon_->GetFenceEvent());
        WaitForSingleObject(dxCommon_->GetFenceEvent(), INFINITE);
    }

    // 2. コマンドリストとアロケータのリセット (現在のフレーム用)
    ID3D12CommandAllocator* allocator = dxCommon_->GetCommandAllocator();
    HRESULT hr = allocator->Reset();
    assert(SUCCEEDED(hr));
    hr = commandList_->Reset(allocator, nullptr);
    assert(SUCCEEDED(hr));

    // フレーム開始時に、ポーズ中でSetFrameDataが呼ばれなくてもバッファが常に同期待ちにならないようキャッシュを現在のバッファへコピーする
    SyncCachedFrameData();

    // バックバッファとRTV/DSVの取得 (これはスワップチェーン依存なのでそのままでよい)

    // バックバッファとRTV/DSVの取得
    const UINT backIdx = dxCommon_->GetSwapChain()->GetCurrentBackBufferIndex();
    ID3D12Resource* backBuffer = dxCommon_->GetSwapChainResources(backIdx);
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = dxCommon_->GetRtvHandles(backIdx);
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dxCommon_->GetDSVCPUDescriptorHandle(0);

    /*完璧な画面クリアを目指して*/

    ///TransitionBarrierを張るコード

    // TransitionBarrierの設定（Present -> RenderTarget）
    DirectXUtils::TransitionBarrier(commandList_, backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

    // レンダーターゲット追跡の更新 (バックバッファ)
    currentRenderTexture_ = nullptr;

    /*画面の色を変えよう*/

    ///コマンドを積み込んで確定させる

    //描画先のRTVを設定する
    commandList_->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);
    //指定した色で画面全体をクリアする
    commandList_->ClearRenderTargetView(rtvHandle, clearColor.data(), 0, nullptr);

    /*前後関係を正しくしよう*/

    //指定した深度で画面全体をクリアする
    commandList_->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, clearDepth, clearStencil, 0, nullptr);

    // フレーム共通のビューポート/シザーを一度だけ設定
    commandList_->RSSetViewports(1, &dxCommon_->GetViewport());
    commandList_->RSSetScissorRects(1, &dxCommon_->GetScissorRect());

    // フレームで利用するSRVヒープを設定(全描画共通)
    ID3D12DescriptorHeap* descriptorHeaps[] = { dxCommon_->GetSrvDescriptorHeap() };
    commandList_->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    // --- フレーム共通CBV/SRVをここで一度だけバインド ---
    BindCommonParameters();

    // 環境マップをバインド
    if (environmentMapHandle_.ptr != 0) {
        commandList_->SetGraphicsRootDescriptorTable((UINT)RootSlot::EnvMap, environmentMapHandle_);
    } else if (textureManager_) {
        commandList_->SetGraphicsRootDescriptorTable((UINT)RootSlot::EnvMap, textureManager_->GetWhiteCubeMapHandle());
    }
}

void DrawManager::PostDraw() {

    const UINT backIdx = dxCommon_->GetSwapChain()->GetCurrentBackBufferIndex();
    ID3D12Resource* backBuffer = dxCommon_->GetSwapChainResources(backIdx);

    /*完璧な画面クリアを目指して*/

    //画面に描く処理はすべて終わり、画面に映すので、状態を遷移（RenderTarget -> Present）
    DirectXUtils::TransitionBarrier(commandList_, backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

    /*画面の色を変えよう*/

    ///コマンドを積み込んで確定させる

    //コマンドリストの内容を確定させる。すべてのコマンドを積んでからCloseすること
    HRESULT hr = commandList_->Close();
    assert(SUCCEEDED(hr));

    ///コマンドをキックする

    //GPUにコマンドリストの実行を行わせる
    ID3D12CommandList* commandLists[] = { commandList_ };
    dxCommon_->GetCommandQueue()->ExecuteCommandLists(_countof(commandLists), commandLists);
    //GPUとOSに画面の交換を行うよう通知する
    hr = dxCommon_->GetSwapChain()->Present(1, 0);
    // デバイスが削除されたかどうかのチェック
    if (FAILED(hr)) {
        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
            HRESULT removedReason = dxCommon_->GetDevice()->GetDeviceRemovedReason();
            char str[256];
            sprintf_s(str, "Device Removed or Reset, reason code: 0x%08X", removedReason);
            OutputDebugStringA(str);
            OutputDebugStringA("\n");
            throw std::runtime_error(str);
        } else {
            throw std::runtime_error("Present failed with an unknown error.");
        }
    }


    // 1. フェンスをシグナル (通し番号をインクリメントして記録)
    uint64_t nextValue = dxCommon_->IncrementGlobalFence();
    dxCommon_->GetFenceValue() = nextValue; // このスロットの完了番号として保存
    dxCommon_->GetCommandQueue()->Signal(dxCommon_->GetFence(), nextValue);
 
    // 2. 次のフレームへインデックスを進める
    dxCommon_->AdvanceFrameIndex();

    dxCommon_->UpdateFixFPS();
}

void DrawManager::SetFrameData(const CameraForGPU& camera, float time, float deltaTime, const DirectionalLight& light, const std::vector<PointLight*>& pointLights, const std::vector<SpotLight*>& spotLights, const std::vector<AreaLight*>& areaLights) {
    cachedPerFrame_.camera = camera;
    cachedPerFrame_.time = time;
    cachedPerFrame_.deltaTime = deltaTime;
    cachedDirectionalLight_ = light;
    
    cachedPointLights_.clear();
    for (auto* pl : pointLights) cachedPointLights_.push_back(*pl);
    
    cachedSpotLights_.clear();
    for (auto* sl : spotLights) cachedSpotLights_.push_back(*sl);
    
    cachedAreaLights_.clear();
    for (auto* al : areaLights) cachedAreaLights_.push_back(*al);
    
    SyncCachedFrameData();
}

void DrawManager::SyncCachedFrameData() {
    auto& fr = frameResources_[dxCommon_->GetFrameIndex()];

    if (fr.perFrameData) { *fr.perFrameData = cachedPerFrame_; }
    if (fr.lightCommonData) {
        // ライト共通データの更新（b1）
        fr.lightCommonData->directionalLight = cachedDirectionalLight_;
        fr.lightCommonData->pointLightCount = static_cast<int32_t>(cachedPointLights_.size());
        fr.lightCommonData->spotLightCount = static_cast<int32_t>(cachedSpotLights_.size());
        fr.lightCommonData->areaLightCount = static_cast<int32_t>(cachedAreaLights_.size());

        // シャドウマップの行列更新
        ShadowMap* shadowMap = shadowMaps_[dxCommon_->GetFrameIndex()].get();
        if (shadowMap) {
            Vector3 targetPos = useCustomShadowParams_ ? shadowTargetPos_ : cachedPerFrame_.camera.worldPosition;
            float orthoSize = useCustomShadowParams_ ? shadowOrthoSize_ : 128.0f;
            shadowMap->UpdateMatrix(cachedDirectionalLight_.direction, targetPos, orthoSize);
            fr.lightCommonData->viewProjection = shadowMap->GetViewProjection();
        }
    }

    // 各 StructuredBuffer へ書き込み
    auto copyLights = [](ID3D12Resource* res, const auto& lightVec) {
        if (!res || lightVec.empty()) return;
        using LightType = std::remove_pointer_t<typename std::decay_t<decltype(lightVec)>::value_type>;
        LightType* mapped = nullptr;
        if (SUCCEEDED(res->Map(0, nullptr, reinterpret_cast<void**>(&mapped))) && mapped) {
            for (size_t i = 0; i < lightVec.size(); ++i) {
                mapped[i] = lightVec[i];
            }
            res->Unmap(0, nullptr);
        }
    };

    copyLights(fr.pointLightResource.Get(), cachedPointLights_);
    copyLights(fr.spotLightResource.Get(), cachedSpotLights_);
    copyLights(fr.areaLightResource.Get(), cachedAreaLights_);
}

void DrawManager::SetEnvironmentMap(D3D12_GPU_DESCRIPTOR_HANDLE envMapHandle) {
    environmentMapHandle_ = envMapHandle;
}



void DrawManager::SubmitSprite(const Object2DResource* resource) {
    if (!resource) return;
    SpritePacket p{};
    p.resource = resource;
    p.blendMode = dxCommon_->GetEngine()->currentBlend_;
    p.depthWrite = dxCommon_->GetEngine()->currentDepth_;
    p.cullMode = dxCommon_->GetEngine()->currentCull_;
    spriteQueue_.push_back(p);
}

void DrawManager::SubmitTopMostSprite(const Object2DResource* resource) {
    if (!resource) return;
    SpritePacket p{};
    p.resource = resource;
    p.blendMode = dxCommon_->GetEngine()->currentBlend_;
    p.depthWrite = dxCommon_->GetEngine()->currentDepth_;
    p.cullMode = dxCommon_->GetEngine()->currentCull_;
    topMostSpriteQueue_.push_back(p);
}

void DrawManager::DrawSprite(const RenderPackets::SpritePacket& packet) {
    const Object2DResource* resource = packet.resource;
    if (!resource || !commandList_) return;

    // トポロジ設定
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 頂点バッファとインデックスバッファの設定
    commandList_->IASetVertexBuffers(0, 1, &resource->vertexBufferView_);
    commandList_->IASetIndexBuffer(&resource->indexBufferView_);

    commandList_->SetGraphicsRootConstantBufferView((UINT)RootSlot::Material, resource->GetMaterialVAddress());
    commandList_->SetGraphicsRootConstantBufferView((UINT)RootSlot::Transform, resource->GetTransformVAddress());
    commandList_->SetGraphicsRootDescriptorTable((UINT)RootSlot::Texture, resource->textureHandle_);

    commandList_->DrawIndexedInstanced(resource->indexCount_, 1, 0, 0, 0);
}

void DrawManager::SubmitText(const Object2DResource* resource) {
    if (!resource) return;
    SpritePacket p{};
    p.resource = resource;
    p.blendMode = dxCommon_->GetEngine()->currentBlend_;
    p.depthWrite = dxCommon_->GetEngine()->currentDepth_;
    p.cullMode = dxCommon_->GetEngine()->currentCull_;
    textQueue_.push_back(p);
}

void DrawManager::SubmitTopMostText(const Object2DResource* resource) {
    if (!resource) return;
    SpritePacket p{};
    p.resource = resource;
    p.blendMode = dxCommon_->GetEngine()->currentBlend_;
    p.depthWrite = dxCommon_->GetEngine()->currentDepth_;
    p.cullMode = dxCommon_->GetEngine()->currentCull_;
    topMostTextQueue_.push_back(p);
}

void DrawManager::DrawText(const RenderPackets::SpritePacket& packet) {
    const Object2DResource* resource = packet.resource;
    if (!resource || !commandList_) return;

    if (packet.customPSO) {
        commandList_->SetPipelineState(packet.customPSO);
    }

    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList_->IASetVertexBuffers(0, 1, &resource->vertexBufferView_);
    commandList_->IASetIndexBuffer(&resource->indexBufferView_);
    commandList_->SetGraphicsRootConstantBufferView((UINT)RootSlot::Material, resource->GetMaterialVAddress());
    commandList_->SetGraphicsRootConstantBufferView((UINT)RootSlot::Transform, resource->GetTransformVAddress());
    commandList_->SetGraphicsRootDescriptorTable((UINT)RootSlot::Texture, resource->textureHandle_);
    commandList_->DrawIndexedInstanced(resource->indexCount_, 1, 0, 0, 0);
}


void DrawManager::SubmitModelRegion(const ModelRegionPacket& packet) {
    modelRegionQueue_.push_back(packet);
}

void DrawManager::DrawModelRegion(const RenderPackets::ModelRegionPacket& packet) {
    const GpuMesh* gpuMesh = packet.gpuMesh;
    if (!gpuMesh || gpuMesh->vertexCount == 0 || packet.instanceCount == 0) { return; }

    // IA設定 (共有リソースから)
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList_->IASetVertexBuffers(0, 1, &gpuMesh->vertexBufferView);
    if (gpuMesh->indexCount > 0) {
        commandList_->IASetIndexBuffer(&gpuMesh->indexBufferView);
    }

    // Material (CBV)
    commandList_->SetGraphicsRootConstantBufferView((UINT)RootSlot::Material, packet.materialAddress);

    // Texture (SRV)
    commandList_->SetGraphicsRootDescriptorTable((UINT)RootSlot::Texture, packet.textureHandle);

    // Instances (SRV)
    commandList_->SetGraphicsRootDescriptorTable((UINT)RootSlot::Instancing, packet.instancingSrvHandleGPU);

    // Draw
    if (gpuMesh->indexCount > 0) {
        commandList_->DrawIndexedInstanced(gpuMesh->indexCount, packet.instanceCount, 0, 0, 0);
    } else {
        commandList_->DrawInstanced(gpuMesh->vertexCount, packet.instanceCount, 0, 0);
    }
}

void DrawManager::SubmitPrimitiveRegion(const RenderPackets::PrimitiveRegionPacket& packet) {
    primitiveRegionQueue_.push_back(packet);
}

void DrawManager::DrawPrimitiveRegion(const RenderPackets::PrimitiveRegionPacket& packet) {
    if (packet.indexCount == 0 || packet.instanceCount == 0) { return; }

    // IA
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList_->IASetVertexBuffers(0, 1, &packet.vertexBufferView);
    commandList_->IASetIndexBuffer(&packet.indexBufferView);

    // CBV (PS)
    commandList_->SetGraphicsRootConstantBufferView((UINT)RootSlot::Material, packet.materialAddress);          // PS b0

    // SRV (PS t0 / VS t0)
    commandList_->SetGraphicsRootDescriptorTable((UINT)RootSlot::Texture, packet.textureHandle);            // PS t0
    commandList_->SetGraphicsRootDescriptorTable((UINT)RootSlot::Instancing, packet.instancingSrvHandleGPU);   // VS t0

    // Draw
    commandList_->DrawIndexedInstanced(packet.indexCount, packet.instanceCount, 0, 0, 0);
}

void DrawManager::SubmitLineInstanced(const LineResource* resource, const D3D12_GPU_DESCRIPTOR_HANDLE& instancingSrvHandleGPU, const UINT& instanceCount) {
    if (!resource || instanceCount == 0) return;
    LinePacket p{};
    p.resource = resource;
    p.instancingSrvHandleGPU = instancingSrvHandleGPU;
    p.instanceCount = instanceCount;
    p.blendMode = dxCommon_->GetEngine()->currentBlend_;
    p.depthWrite = dxCommon_->GetEngine()->currentDepth_;
    p.cullMode = dxCommon_->GetEngine()->currentCull_;
    lineQueue_.push_back(p);
}

void DrawManager::DrawLineInstanced(const RenderPackets::LinePacket& packet) {
    const LineResource* resource = packet.resource;
    if (!resource || packet.instanceCount == 0) return;

    // IA
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
    commandList_->IASetVertexBuffers(0, 1, &resource->vertexBufferView_);
    commandList_->IASetIndexBuffer(&resource->indexBufferView_);

    // SRV (VS t1)
    commandList_->SetGraphicsRootDescriptorTable((UINT)RootSlot::LineInstancing, packet.instancingSrvHandleGPU);

    // Draw
    commandList_->DrawIndexedInstanced(2, packet.instanceCount, 0, 0, 0);
}

void DrawManager::DispatchSkinning(const SkinCluster& skinCluster, const ManagedModel* model, uint32_t numVertices) {
    if (!model || !model->gpuMeshes[0] || !dxCommon_) return;

    // --- コンピュートシェーダーによるスキニング実行 ---
    // PSOをコンピュート用に切り替え
    commandList_->SetPipelineState(dxCommon_->GetPSOManager()->GetComputePSO("Skinning"));

    // RootSignatureはSkipして共通のComputeRootSignatureを使用する想定
    // (PSO設定時にセットされているはずだが、念のため管理が必要な場合はここでセット)

    // Parameterの設定
    uint32_t frameIndex = dxCommon_->GetFrameIndex();
    
    // 0: Palette (t0)
    commandList_->SetComputeRootDescriptorTable(0, skinCluster.paletteSrvHandle[frameIndex].second);
    // 1: Input Vertices (t1) (最初のメッシュの頂点を使用)
    commandList_->SetComputeRootDescriptorTable(1, model->gpuMeshes[0]->vertexSrvHandle);
    // 2: Influences (t2)
    commandList_->SetComputeRootDescriptorTable(2, skinCluster.influenceSrvHandle.second);
    // 3: Output Vertices (u0)
    commandList_->SetComputeRootDescriptorTable(3, skinCluster.skinnedVertexUavHandle[frameIndex].second);
    // 4: Skinning Information (b0)
    commandList_->SetComputeRootConstantBufferView(4, skinCluster.skinningInformationResource->GetGPUVirtualAddress());

    // Dispatch (numthreads = 256)
    commandList_->Dispatch((numVertices + 255) / 256, 1, 1);
}

void DrawManager::ExecuteUAVBarrier(ID3D12Resource* resource) {
    DirectXUtils::UAVBarrier(commandList_, resource);
}

void DrawManager::SubmitSkybox(const D3D12_VERTEX_BUFFER_VIEW& vertexBufferView, const D3D12_INDEX_BUFFER_VIEW& indexBufferView, D3D12_GPU_VIRTUAL_ADDRESS materialAddress, D3D12_GPU_VIRTUAL_ADDRESS transformationAddress, D3D12_GPU_DESCRIPTOR_HANDLE textureHandle, const UINT& indexCount) {
    SkyboxPacket p{};
    p.vertexBufferView = vertexBufferView;
    p.indexBufferView = indexBufferView;
    p.materialAddress = materialAddress;
    p.transformationAddress = transformationAddress;
    p.textureHandle = textureHandle;
    p.indexCount = indexCount;
    skyboxQueue_.push_back(p);
}

void DrawManager::DrawSkybox(const RenderPackets::SkyboxPacket& packet) {

    commandList_->IASetVertexBuffers(0, 1, &packet.vertexBufferView); // VBVを設定
    //IBVを設定
    commandList_->IASetIndexBuffer(&packet.indexBufferView);
    //形状を設定。PSOに設定しているものとはまた別。同じものを設定すると考えておけば良い
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ///CBVを設定する

    //マテリアルCBufferの場所を設定(ここでの第一引数の0はRootParameter配列の0番目であり、registerの0ではない)
    commandList_->SetGraphicsRootConstantBufferView((UINT)RootSlot::Material, packet.materialAddress);

    //wvp用のCBufferの場所を設定(今回はRootParameter[1]に対してCBVの設定を行っている)
    commandList_->SetGraphicsRootConstantBufferView((UINT)RootSlot::Transform, packet.transformationAddress);

    ///DescriptorTableを設定する

    //SRVのDescriptorTableの先頭を設定。2はRootParameter[2]である。
    commandList_->SetGraphicsRootDescriptorTable((UINT)RootSlot::Texture, packet.textureHandle);

    //描画！（DrawCall/ドローコール）。3頂点で1つのインスタンス。
    commandList_->DrawIndexedInstanced(packet.indexCount, 1, 0, 0, 0);
}

void DrawManager::SubmitStandard3D(const Object3DResource* resource, const D3D12_VERTEX_BUFFER_VIEW* vertexBufferViewOverride, bool castShadows, ID3D12Resource* vertexBufferResourceOverride) {
    if (!resource) return;
    Standard3DPacket p{};
    p.resource = resource;
    p.vertexBufferViewOverride = vertexBufferViewOverride;
    p.blendMode = dxCommon_->GetEngine()->currentBlend_;
    p.depthWrite = dxCommon_->GetEngine()->currentDepth_;
    p.cullMode = dxCommon_->GetEngine()->currentCull_;
    p.castShadows = castShadows;
    p.customPSO = resource->GetCustomPSO();
    p.customCBVAddress = resource->GetCustomCBVAddress();
    p.vertexBufferResourceOverride = vertexBufferResourceOverride;
    standard3DQueue_.push_back(p);
}

void DrawManager::SubmitUI3D(const Object3DResource* resource, const D3D12_VERTEX_BUFFER_VIEW* vertexBufferViewOverride) {
    if (!resource) return;
    Standard3DPacket p{};
    p.resource = resource;
    p.vertexBufferViewOverride = vertexBufferViewOverride;
    p.blendMode = dxCommon_->GetEngine()->currentBlend_;
    p.depthWrite = dxCommon_->GetEngine()->currentDepth_;
    p.cullMode = dxCommon_->GetEngine()->currentCull_;
    p.customPSO = resource->GetCustomPSO();
    p.customCBVAddress = resource->GetCustomCBVAddress();
    ui3DQueue_.push_back(p);
}

void DrawManager::SubmitOutlineMask(const Object3DResource* resource, const D3D12_VERTEX_BUFFER_VIEW* vertexBufferViewOverride) {
    if (!resource) return;
    Standard3DPacket p{};
    p.resource = resource;
    p.vertexBufferViewOverride = vertexBufferViewOverride;
    p.blendMode = dxCommon_->GetEngine()->currentBlend_;
    p.depthWrite = dxCommon_->GetEngine()->currentDepth_;
    p.cullMode = dxCommon_->GetEngine()->currentCull_;
    p.customPSO = resource->GetCustomPSO();
    p.customCBVAddress = resource->GetCustomCBVAddress();
    selectionMaskQueue_.push_back(p);
}

void DrawManager::SubmitTextOutlineMask(const Object2DResource* resource) {
    if (!resource) return;
    SpritePacket p{};
    p.resource = resource;
    p.blendMode = dxCommon_->GetEngine()->currentBlend_;
    p.depthWrite = dxCommon_->GetEngine()->currentDepth_;
    p.cullMode = dxCommon_->GetEngine()->currentCull_;
    p.customPSO = resource->GetCustomPSO();
    p.customCBVAddress = resource->GetCustomCBVAddress();
    selectionMaskQueue2D_.push_back(p);
}

void DrawManager::DrawStandard3D(const RenderPackets::Standard3DPacket& packet) {
    const Object3DResource* resource = packet.resource;
    if (!resource || !commandList_) return;
    
    // --- 描画前: UAV -> VBV ---
    if (packet.vertexBufferResourceOverride) {
        DirectXUtils::TransitionBarrier(commandList_, packet.vertexBufferResourceOverride, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    }

    // トポロジ設定
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 頂点バッファの設定 (オーバーライドがあれば優先)
    if (packet.vertexBufferViewOverride) {
        commandList_->IASetVertexBuffers(0, 1, packet.vertexBufferViewOverride);
    } else {
        commandList_->IASetVertexBuffers(0, 1, &resource->vertexBufferView_);
    }
    commandList_->IASetIndexBuffer(&resource->indexBufferView_);

    // 各種リソースのバインド
    commandList_->SetGraphicsRootConstantBufferView((UINT)RootSlot::Material, resource->GetMaterialVAddress());
    commandList_->SetGraphicsRootConstantBufferView((UINT)RootSlot::Transform, resource->GetTransformVAddress());
    commandList_->SetGraphicsRootDescriptorTable((UINT)RootSlot::Texture, resource->textureHandle_);

    // customCBVAddress が設定されていれば Special (b6) にバインドする
    if (packet.customCBVAddress != 0) {
        commandList_->SetGraphicsRootConstantBufferView((UINT)RootSlot::Special, packet.customCBVAddress);
    }

    // 描画
    commandList_->DrawIndexedInstanced(resource->indexCount_, 1, 0, 0, 0);

    // --- 描画後: VBV -> UAV に戻す (次フレームのCompute用) ---
    if (packet.vertexBufferResourceOverride) {
        DirectXUtils::TransitionBarrier(commandList_, packet.vertexBufferResourceOverride, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    }
}


void DrawManager::SubmitGPUParticle(const RenderPackets::GPUParticlePacket& packet) {
    if (packet.instanceCount == 0) return;
    gpuParticleQueue_.push_back(packet);
}

void DrawManager::DrawGPUParticle(const RenderPackets::GPUParticlePacket& packet) {
    if (!commandList_) return;

    // リソースバリヤー: UAV -> ShaderResource (読み取り)
    if (packet.particleResource) {
        DirectXUtils::TransitionBarrier(commandList_, packet.particleResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    }

    // IA 設定: VB/Topology
    commandList_->IASetVertexBuffers(0, 1, &packet.vbv);
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // --- CBV のバインド ---
    // (rootParameters[(UINT)RootSlot::Material] に対応、PixelShader 側の b0 想定)
    commandList_->SetGraphicsRootConstantBufferView((UINT)RootSlot::Material, packet.materialAddress);
    // (rootParameters[(UINT)RootSlot::Transform] に対応、VertexShader の b0 配置)
    commandList_->SetGraphicsRootConstantBufferView((UINT)RootSlot::Transform, packet.perViewAddress);
    // エミッター設定 (RootSlot::Special -> register b6)
    if (packet.emitterAddress != 0) {
        commandList_->SetGraphicsRootConstantBufferView((UINT)RootSlot::Special, packet.emitterAddress);
    }

    // --- SRVのバインド ---
    // テクスチャ (PS t0)
    commandList_->SetGraphicsRootDescriptorTable((UINT)RootSlot::Texture, packet.textureHandle);
    // パーティクルデータ (VS t0 -> Slot 5: Instancing)
    commandList_->SetGraphicsRootDescriptorTable((UINT)RootSlot::Instancing, packet.particleSrvHandle);
    
    // ソートデータ (VS t1 -> Slot 9: LineInstancing)
    commandList_->SetGraphicsRootDescriptorTable((UINT)RootSlot::LineInstancing, packet.sortListSrvHandle);

    if (packet.indexCount > 0) {
        commandList_->IASetIndexBuffer(&packet.ibv);
        commandList_->DrawIndexedInstanced(packet.indexCount, packet.instanceCount, 0, 0, 0);
    } else {
        // 従来のビルボード互換
        commandList_->DrawInstanced(6, packet.instanceCount, 0, 0);
    }

    // リソースバリヤー: ShaderResource -> UAV (次のフレームの計算用に戻す)
    if (packet.particleResource) {
        DirectXUtils::TransitionBarrier(commandList_, packet.particleResource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    }
}

void DrawManager::SubmitVoxelParticle(
    uint32_t instanceCount,
    const D3D12_VERTEX_BUFFER_VIEW& vbv,
    const D3D12_INDEX_BUFFER_VIEW& ibv,
    uint32_t indexCount,
    D3D12_GPU_VIRTUAL_ADDRESS emitterAddress,
    D3D12_GPU_DESCRIPTOR_HANDLE particleDataHandle,
    ID3D12Resource* particleResource,
    ID3D12PipelineState* drawPSO
) {
    if (instanceCount == 0) return;
    VoxelParticlePacket p{};
    p.instanceCount = instanceCount;
    p.vbv = vbv;
    p.ibv = ibv;
    p.indexCount = indexCount;
    p.emitterAddress = emitterAddress;
    p.particleDataHandle = particleDataHandle;
    p.particleResource = particleResource;
    p.drawPSO = drawPSO;
    voxelParticleQueue_.push_back(p);
}

void DrawManager::DrawVoxelParticle(const RenderPackets::VoxelParticlePacket& packet) {
    if (!commandList_) return;

    // リソースバリヤー: UAV -> ShaderResource (読み取り)
    if (packet.particleResource) {
        DirectXUtils::TransitionBarrier(commandList_, packet.particleResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    }

    // VoxelParticle 専用PSOをバインド
    if (packet.drawPSO) {
        commandList_->SetPipelineState(packet.drawPSO);
    }

    // トポロジ設定
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 頂点バッファとインデックスバッファの設定
    commandList_->IASetVertexBuffers(0, 1, &packet.vbv);
    commandList_->IASetIndexBuffer(&packet.ibv);

    // VoxelParticle 特有のバインド
    // Slot 1: Transform (b0) <- Emitter
    commandList_->SetGraphicsRootConstantBufferView((UINT)RootSlot::Transform, packet.emitterAddress);
    // Slot 9: LineInstancing (t1) <- ParticleData
    commandList_->SetGraphicsRootDescriptorTable((UINT)RootSlot::LineInstancing, packet.particleDataHandle);

    commandList_->DrawIndexedInstanced(packet.indexCount, packet.instanceCount, 0, 0, 0);

    // リソースバリヤー: ShaderResource -> UAV (次のフレームの計算用に戻す)
    if (packet.particleResource) {
        DirectXUtils::TransitionBarrier(commandList_, packet.particleResource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    }
}

void DrawManager::BeginRenderTexture(RenderTexture* rt, const Vector4& clearColor) {
    // 1. Transition Barrier (SRV -> RenderTarget)
    DirectXUtils::TransitionBarrier(commandList_, rt->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

    // レンダーターゲットを追跡
    currentRenderTexture_ = rt;

    // 2. Set Render Target
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rt->GetRtvHandle();
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dxCommon_->GetDSVCPUDescriptorHandle(0);
    commandList_->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);

    // 3. Clear
    commandList_->ClearRenderTargetView(rtvHandle, &clearColor.x, 0, nullptr);
    commandList_->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // 4. Set Viewport/Scissor
    D3D12_VIEWPORT viewport{ 0.0f, 0.0f, static_cast<float>(rt->GetWidth()), static_cast<float>(rt->GetHeight()), 0.0f, 1.0f };
    D3D12_RECT scissor{ 0, 0, static_cast<long>(rt->GetWidth()), static_cast<long>(rt->GetHeight()) };
    commandList_->RSSetViewports(1, &viewport);
    commandList_->RSSetScissorRects(1, &scissor);

    // 5. Descriptor Heaps (念のため再設定)
    ID3D12DescriptorHeap* descriptorHeaps[] = { dxCommon_->GetSrvDescriptorHeap() };
    commandList_->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
}

void DrawManager::EndRenderTexture(RenderTexture* rt) {
    // 1. Transition Barrier (RenderTarget -> SRV)
    DirectXUtils::TransitionBarrier(commandList_, rt->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void DrawManager::SetRenderTargetToBackBuffer(bool useDepth) {
    const UINT backIdx = dxCommon_->GetSwapChain()->GetCurrentBackBufferIndex();
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = dxCommon_->GetRtvHandles(backIdx);
    if (useDepth) {
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dxCommon_->GetDSVCPUDescriptorHandle(0);
        commandList_->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);
    } else {
        commandList_->OMSetRenderTargets(1, &rtvHandle, false, nullptr);
    }

    // ビューポートとシザーを元に戻す
    commandList_->RSSetViewports(1, &dxCommon_->GetViewport());
    commandList_->RSSetScissorRects(1, &dxCommon_->GetScissorRect());

    // レンダーターゲット追跡のリセット
    currentRenderTexture_ = nullptr;
}

void DrawManager::DrawRenderTexture(RenderTexture* renderTexture, ID3D12PipelineState* pso, D3D12_GPU_VIRTUAL_ADDRESS cbvAddress, D3D12_GPU_DESCRIPTOR_HANDLE depthSrvHandle) {
    if (!renderTexture) return;

    // 1. PSOの設定 (引数が渡された場合はそれを使用、そうでなければデフォルトのCopyImage)
    if (pso) {
        commandList_->SetPipelineState(pso);
    } else {
        ID3D12PipelineState* defaultPso = dxCommon_->GetPSOManager()->GetCopyImage();
        if (!defaultPso) return;
        commandList_->SetPipelineState(defaultPso);
    }

    // 2. ルートシグネチャの設定
    commandList_->SetGraphicsRootSignature(dxCommon_->GetRootSignature());

    // 3. 形状の設定 (三角形リスト)
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 4. テクスチャの設定 (RootParameter[(UINT)RootSlot::Texture])
    commandList_->SetGraphicsRootDescriptorTable((UINT)RootSlot::Texture, renderTexture->GetSrvHandleGPU());

    // 深度テクスチャの設定 (RootParameter[(UINT)RootSlot::EnvMap])
    if (depthSrvHandle.ptr != 0) {
        commandList_->SetGraphicsRootDescriptorTable((UINT)RootSlot::EnvMap, depthSrvHandle);
    }

    // 追加: ConstantBuffer の設定 (引数があれば RootParameter[(UINT)RootSlot::Material] にセット)
    if (cbvAddress != 0) {
        commandList_->SetGraphicsRootConstantBufferView((UINT)RootSlot::Material, cbvAddress);
    }

    // 5. 描画 (3頂点のインデックスなし描画: SV_VertexIDを使用するためVBいらず)
    commandList_->DrawInstanced(3, 1, 0, 0);
}
void DrawManager::BindCommonParameters() {
    if (!commandList_ || !dxCommon_) return;

    commandList_->SetGraphicsRootSignature(dxCommon_->GetRootSignature());
    commandList_->SetComputeRootSignature(dxCommon_->GetComputeRootSignature());

    auto& fr = frameResources_[dxCommon_->GetFrameIndex()];
    commandList_->SetGraphicsRootConstantBufferView((UINT)RootSlot::Camera, fr.frameData.camera);
    commandList_->SetGraphicsRootConstantBufferView((UINT)RootSlot::LightCommon, fr.frameData.lightCommon);

    // 点光源、スポットライト、面光源を１つのテーブル（Slot 6）で一括設定
    commandList_->SetGraphicsRootDescriptorTable((UINT)RootSlot::Lights, fr.lightSrvHandle);

    // シャドウマップをバインド (Slot 10 / register t5) - シャドウパス中はバインドしない
    ShadowMap* shadowMap = GetShadowMap();
    if (shadowMap && !isShadowPass_) {
        commandList_->SetGraphicsRootDescriptorTable((UINT)RootSlot::ShadowMap, shadowMap->GetSrvHandle());
    }
}

void DrawManager::BeginShadowPass() {
    ShadowMap* shadowMap = GetShadowMap();
    if (!shadowMap) return;
    isShadowPass_ = true;

    // 1. シャドウマップの準備 (バリア遷移、クリア、DSVセット)
    shadowMap->Begin(commandList_);

    // 2. ライト行列を定数バッファに反映
    auto& fr = frameResources_[dxCommon_->GetFrameIndex()];
    if (fr.lightCommonData) {
        // すでに SetFrameData で計算済みだが、念のため最新の状態を反映
        fr.lightCommonData->viewProjection = shadowMap->GetViewProjection();
    }

    // 3. DescriptorHeap再設定
    ID3D12DescriptorHeap* descriptorHeaps[] = { dxCommon_->GetSrvDescriptorHeap() };
    commandList_->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    // 4. バインド (ライト行列を定数バッファに反映させるため)
    BindCommonParameters();
}

void DrawManager::EndShadowPass() {
    ShadowMap* shadowMap = GetShadowMap();
    if (!shadowMap) return;
    isShadowPass_ = false;

    // 1. バリア遷移を元に戻す (DepthWrite -> SRV)
    shadowMap->End(commandList_);

    // 2. レンダーターゲットを復帰させる
    if (currentRenderTexture_) {
        // 元の RenderTexture があればそれを再設定 (クリアはしない)
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = currentRenderTexture_->GetRtvHandle();
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dxCommon_->GetDSVCPUDescriptorHandle(0);
        commandList_->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);

        // ビューポート等も復帰
        D3D12_VIEWPORT viewport{ 0.0f, 0.0f, static_cast<float>(currentRenderTexture_->GetWidth()), static_cast<float>(currentRenderTexture_->GetHeight()), 0.0f, 1.0f };
        D3D12_RECT scissor{ 0, 0, static_cast<long>(currentRenderTexture_->GetWidth()), static_cast<long>(currentRenderTexture_->GetHeight()) };
        commandList_->RSSetViewports(1, &viewport);
        commandList_->RSSetScissorRects(1, &scissor);
    }
    else {
        // なければバックバッファに戻す
        SetRenderTargetToBackBuffer(true);
    }
}

void DrawManager::ExecuteRenderQueues(IrufemiEngine* engine) {
    if (renderGraph_) {
        // メインレンダリングテクスチャの初期状態を登録 (RenderGraph内で遷移するため)
        renderGraph_->RegisterResourceState(engine->GetMainRenderTexture()->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET);
        
        // 深度バッファの初期状態も登録 (DepthBasedOutline 等で参照するため)
        renderGraph_->RegisterResourceState(dxCommon_->GetDepthStencilResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE);

        renderGraph_->Execute(this, engine);
        
#ifdef EditorMode
        // RenderGraph 終了後、メインテクスチャを ImGui 等で読み取れるように SRV ステートに戻す
        DirectXUtils::TransitionBarrier(dxCommon_->GetCommandList(), engine->GetMainRenderTexture()->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
#endif
    }

    // RenderGraph 終了後はバックバッファを描画対象とする (TopMost UI など用)
    SetRenderTargetToBackBuffer(false);

    ClearRenderQueues();
}

void DrawManager::ExecuteTopMostQueues(IrufemiEngine* engine) {
    if (topMostSpriteQueue_.empty()) return;

    BlendMode currentBlend = BlendMode::kBlendModeNormal;
    PSOManager::DepthWrite currentDepth = PSOManager::DepthWrite::Enable;
    PSOManager::CullMode currentCull = PSOManager::CullMode::Back;
    bool first = true;
    
    for (const auto& p : topMostSpriteQueue_) {
        if (first || p.blendMode != currentBlend || p.depthWrite != currentDepth || p.cullMode != currentCull) {
            engine->SetBlend(p.blendMode);
            engine->SetDepthWrite(p.depthWrite);
            engine->SetCull(p.cullMode);
            engine->ApplyPSO("SpriteForBackBuffer");
            
            currentBlend = p.blendMode;
            currentDepth = p.depthWrite;
            currentCull = p.cullMode;
            first = false;
        }
        DrawSprite(p);
    }

    first = true;
    for (const auto& p : topMostTextQueue_) {
        if (first || p.blendMode != currentBlend || p.depthWrite != currentDepth || p.cullMode != currentCull) {
            engine->SetBlend(p.blendMode);
            engine->SetDepthWrite(p.depthWrite);
            engine->SetCull(p.cullMode);
            // 本来は TextForBackBuffer のようなPSOが望ましいが、最前面用の Text PSO がなければ通常の Text PSO を使用する
            engine->ApplyPSO("Text");
            
            currentBlend = p.blendMode;
            currentDepth = p.depthWrite;
            currentCull = p.cullMode;
            first = false;
        }
        DrawText(p);
    }
}

void DrawManager::ClearRenderQueues() {
    standard3DQueue_.clear();
    ui3DQueue_.clear();
    selectionMaskQueue_.clear();
    selectionMaskQueue2D_.clear();
    spriteQueue_.clear();

    lineQueue_.clear();
    gpuParticleQueue_.clear();
    voxelParticleQueue_.clear();
    skyboxQueue_.clear();
    primitiveRegionQueue_.clear();
    modelRegionQueue_.clear();
    postRenderQueue_.clear();
    topMostSpriteQueue_.clear();
    textQueue_.clear();
    topMostTextQueue_.clear();
}
