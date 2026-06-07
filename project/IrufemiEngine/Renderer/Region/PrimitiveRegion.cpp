#include "PrimitiveRegion.h"
#include <cassert>
#include "Engine/IrufemiEngine.h"
#include "Resource/Texture/TextureManager.h"
#include "Engine/Manager/DrawManager.h"

void PrimitiveRegion::Initialize(PrimitiveType type, const std::string& textureName) {
    type_ = type;
    isCustomPrimitive_ = false;

    EnsureMaterialResources();
    EnsureSharedTexture(textureName);
}

void PrimitiveRegion::InitializeRing(const RingParams& params, const std::string& textureName) {
    type_ = PrimitiveType::Ring;
    isCustomPrimitive_ = true;

    PrimitiveData ringData = PrimitiveManager::CreateRing(params);
    PrimitiveManager::GetInstance()->CreateGPUResource(ringData, customPrimitiveResource_);

    EnsureMaterialResources();
    EnsureSharedTexture(textureName);
}

void PrimitiveRegion::EnsureMaterialResources() {
    if (auto engine = dx_->GetEngine()) {
        if (materialCbIndex_ == static_cast<uint32_t>(-1)) {
            materialCbIndex_ = engine->GetMaterialBufferManager()->Allocate();
        }
    }
    cpuMaterialData_.color = {1.0f, 1.0f, 1.0f, 1.0f};
    cpuMaterialData_.enableLighting = 1;
    cpuMaterialData_.uvTransform = Math::MakeIdentity4x4();
    cpuMaterialData_.metallic = 0.0f;
    cpuMaterialData_.roughness = 0.5f;
    cpuMaterialData_.environmentCoefficient = 0.0f;
    cpuMaterialData_.hasTexture = 1;
    cpuMaterialData_.lightingMode = 3;

    if (auto engine = dx_->GetEngine()) {
        for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
            engine->GetMaterialBufferManager()->Update(materialCbIndex_, cpuMaterialData_, i);
        }
    }
}

void PrimitiveRegion::EnsureSharedTexture(const std::string& textureName) {
    if (!textureName.empty()) {
        textureHandle_ = textureManager_->GetTextureHandle(textureName);
    } else {
        textureHandle_ = textureManager_->GetWhiteTextureHandle();
    }
    assert(textureHandle_.ptr != 0 && "PrimitiveRegion Texture SRV handle is invalid");
}

float PrimitiveRegion::GetBoundingSphereRadius() const {
    // プリミティブマネージャが生成する基本形状のサイズは基本的に半径/サイズ1.0近辺なので1.0fを返す。
    // 必要なら PrimitiveManager からバウンディングスフィア情報を取得できるようにする
    return 1.0f;
}

void PrimitiveRegion::Draw() {
    if (instances_.empty() && instanceWorlds_.empty()) { return; }

    SyncBeforeDraw();

    const PrimitiveResource* res = nullptr;
    if (isCustomPrimitive_) {
        res = &customPrimitiveResource_;
    } else {
        res = &PrimitiveManager::GetInstance()->GetStandardResource(type_);
    }

    if (!res || !res->vertexResource) return;

    RenderPackets::PrimitiveRegionPacket p{};
    // We need to make sure DrawManager understands PrimitiveRegionPacket
    p.vertexBufferView = res->vertexBufferView;
    p.indexBufferView = res->indexBufferView;
    p.indexCount = res->indexCount;
    
    p.materialAddress = GetMaterialVAddress();
    p.textureHandle = GetTextureHandle();
    p.instancingSrvHandleGPU = GetInstancingSrvHandleGPU();
    p.instanceCount = GetInstanceCount();
    
    p.blendMode = GetBlendMode();
    p.depthWrite = GetDepthWrite();
    p.cullMode = GetCullMode();
    p.castShadows = GetCastShadows();
    p.customPSO = GetCustomPSO();
    p.customCBVAddress = GetCustomCBVAddress();

    drawManager_->SubmitPrimitiveRegion(p);
}
