#include "BaseModel.h"
#include "Engine/IrufemiEngine.h"
#include "Engine/Graphics/DirectX/DirectXCommon.h"
#include "Renderer/Core/BaseResource.h"
#include "Resource/Model/ModelManager.h"

IrufemiEngine* BaseModel::engine_ = nullptr;

BaseModel::~BaseModel() {
    if (transformCbIndex_ != static_cast<uint32_t>(-1)) {
        if (engine_) {
            engine_->GetTransformBufferManager()->Free(transformCbIndex_);
        }
    }
}

D3D12_GPU_VIRTUAL_ADDRESS BaseModel::GetTransformationGpuAddress() const {
    if (transformCbIndex_ == static_cast<uint32_t>(-1) || !engine_) return 0;
    return engine_->GetTransformBufferManager()->GetGPUVirtualAddress(transformCbIndex_, BaseResource::GetDirectXCommon()->GetFrameIndex());
}

std::shared_ptr<ObjModel> BaseModel::GetCpuModel() const {
    if (managedModel_) return managedModel_->cpuModel;
    return nullptr;
}

size_t BaseModel::GetMeshCount() const {
    if (managedModel_ && managedModel_->cpuModel) {
        return managedModel_->cpuModel->meshes.size();
    }
    return 0;
}

const ObjMaterial* BaseModel::GetMaterial(size_t meshIndex) const {
    if (managedModel_ && managedModel_->cpuModel && meshIndex < managedModel_->cpuModel->meshes.size()) {
        return &managedModel_->cpuModel->meshes[meshIndex].material;
    }
    return nullptr;
}

ObjMaterial* BaseModel::GetMaterial(size_t meshIndex) {
    if (managedModel_ && managedModel_->cpuModel && meshIndex < managedModel_->cpuModel->meshes.size()) {
        return &managedModel_->cpuModel->meshes[meshIndex].material;
    }
    return nullptr;
}

void BaseModel::UpdateMaterials() {
    if (!managedModel_ || !managedModel_->cpuModel || meshResources_.empty()) {
        return;
    }

    // 全メッシュのマテリアルを更新
    for (size_t i = 0; i < managedModel_->cpuModel->meshes.size(); ++i) {
        if (i >= meshResources_.size()) break;

        auto& res = meshResources_[i];
        if (!res->GetMaterialData()) continue;

        const ObjMaterial& cpuMat = managedModel_->cpuModel->meshes[i].material;
        Material* mappedData = res->GetMaterialData();

        // インスタンスカラーとマテリアルカラーを乗算
        mappedData->color.x = cpuMat.color.x * color_.x;
        mappedData->color.y = cpuMat.color.y * color_.y;
        mappedData->color.z = cpuMat.color.z * color_.z;
        mappedData->color.w = cpuMat.color.w * color_.w;
        if (mappedData->color.w <= 0.0f) { mappedData->color.w = 1.0f; }

        // ライティングの有効状態 (個別上書き優先)
        int32_t finalEnableLighting = (enableLightingOverride_ != -1) ? (enableLightingOverride_ == 1) : (cpuMat.enableLighting ? 1 : 0);
        mappedData->enableLighting = finalEnableLighting;

        mappedData->uvTransform = cpuMat.uvTransform;
        mappedData->metallic = cpuMat.metallic;
        mappedData->roughness = cpuMat.roughness;
        mappedData->hasTexture = !cpuMat.textureFilePath.empty();

        // 映り込み係数 (モデル値 * インスタンス係数)
        mappedData->environmentCoefficient = cpuMat.environmentCoefficient * environmentCoefficient_;

        // ライティングモード (個別上書き優先、指定なしならモデル値、ライティング無効なら0)
        if (lightingModeOverride_ != -1) {
            mappedData->lightingMode = lightingModeOverride_;
        } else {
            mappedData->lightingMode = finalEnableLighting ? cpuMat.lightingMode : 0;
        }

        // サンプラー設定 (個別上書き優先)
        mappedData->useClampSampler = (useClampSamplerOverride_ != -1) ? useClampSamplerOverride_ : cpuMat.useClampSampler;
        
        // アルファテスト用閾値
        mappedData->alphaReference = cpuMat.alphaReference;
        
        // (マテリアルバッファへの転送は SyncBeforeDraw() で行われるため、ここでは SyncMaterialData は呼ばない)
    }
}
