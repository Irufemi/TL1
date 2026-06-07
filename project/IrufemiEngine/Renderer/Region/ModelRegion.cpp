#include "ModelRegion.h"
#include <cassert>
#include "Engine/IrufemiEngine.h"
#include "Resource/Model/ModelManager.h"
#include "Resource/Texture/TextureManager.h"
#include "Engine/Manager/DrawManager.h"

ModelManager* ModelRegion::modelManager_ = nullptr;

void ModelRegion::Initialize(const std::string& objFilename) {
    assert(modelManager_ && "ModelRegion::Initialize: ModelManager is not set.");
    managedModel_ = modelManager_->GetModelAsync(objFilename);
    isResourcesInitialized_ = false;

    auto status = managedModel_->status.load();
    if (status == ManagedModel::LoadingStatus::Loaded && managedModel_->cpuModel) {
        InitializeResources();
    }
}

void ModelRegion::InitializeResources() {
    if (!managedModel_ || !managedModel_->cpuModel || managedModel_->cpuModel->meshes.empty()) {
        return;
    }
    const auto& mesh = managedModel_->cpuModel->meshes.front();

    CreateMaterialResources(mesh);
    EnsureSharedTexture(mesh);
    
    isResourcesInitialized_ = true;
}

const GpuMesh* ModelRegion::GetGpuMesh() const {
    if (managedModel_ && !managedModel_->gpuMeshes.empty()) {
        return managedModel_->gpuMeshes.front().get();
    }
    return nullptr;
}

void ModelRegion::CreateMaterialResources(const ObjMesh& mesh) {
    if (auto engine = dx_->GetEngine()) {
        if (materialCbIndex_ == static_cast<uint32_t>(-1)) {
            materialCbIndex_ = engine->GetMaterialBufferManager()->Allocate();
        }
    }
    cpuMaterialData_.color = mesh.material.color;
    cpuMaterialData_.enableLighting = mesh.material.enableLighting;
    cpuMaterialData_.uvTransform = mesh.material.uvTransform;
    cpuMaterialData_.metallic = mesh.material.metallic;
    cpuMaterialData_.roughness = mesh.material.roughness;
    cpuMaterialData_.environmentCoefficient = 0.0f;
    cpuMaterialData_.hasTexture = !mesh.material.textureFilePath.empty();
    cpuMaterialData_.lightingMode = mesh.material.enableLighting ? 3 : 0;
    if (cpuMaterialData_.color.w <= 0.0f) { cpuMaterialData_.color.w = 1.0f; }

    if (auto engine = dx_->GetEngine()) {
        for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
            engine->GetMaterialBufferManager()->Update(materialCbIndex_, cpuMaterialData_, i);
        }
    }
}

void ModelRegion::EnsureSharedTexture(const ObjMesh& mesh) {
    if (!mesh.material.textureFilePath.empty()) {
        textureHandle_ = textureManager_->GetTextureHandle(mesh.material.textureFilePath);
    } else {
        textureHandle_ = textureManager_->GetWhiteTextureHandle();
    }
    assert(textureHandle_.ptr != 0 && "Texture SRV handle is invalid");
}

float ModelRegion::GetBoundingSphereRadius() const {
    return managedModel_ && managedModel_->cpuModel ? managedModel_->cpuModel->boundingSphere.radius : 0.0f;
}

void ModelRegion::Draw() {
    // If not initialized, attempt to init
    if (!isResourcesInitialized_) {
        if (managedModel_ && managedModel_->status.load() == ManagedModel::LoadingStatus::Loaded && managedModel_->cpuModel) {
            InitializeResources();
        } else {
            return; // Not loaded yet
        }
    }

    if (!GetGpuMesh() || GetGpuMesh()->vertexCount == 0 || (instances_.empty() && instanceWorlds_.empty())) { return; }

    SyncBeforeDraw();

    RenderPackets::ModelRegionPacket p{};
    p.gpuMesh = GetGpuMesh();
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

    drawManager_->SubmitModelRegion(p);
}

void ModelRegion::Draw(bool /*isUI*/) {
    // Overloaded draw for UI, if necessary
    Draw();
}
