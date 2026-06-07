#include "BaseRegion.h"
#include <cassert>
#include <cstring>
#include "Engine/IrufemiEngine.h"
#include "Engine/Core/Math/Geometry/Collision.h"
DirectXCommon* BaseRegion::dx_ = nullptr;
TextureManager* BaseRegion::textureManager_ = nullptr;
DrawManager* BaseRegion::drawManager_ = nullptr;
DescriptorPool* BaseRegion::srvPool_ = nullptr;

BaseRegion::~BaseRegion() {
    if (srvPool_ && dx_) {
        for (auto& idx : instancingSrvIndex_) {
            if (idx != UINT32_MAX) {
                srvPool_->FreeAfterFence(idx, dx_->GetFenceValue());
                idx = UINT32_MAX;
            }
        }
    }
    if (auto engine = dx_->GetEngine()) {
        if (materialCbIndex_ != static_cast<uint32_t>(-1)) {
            engine->GetMaterialBufferManager()->Free(materialCbIndex_);
        }
    }
}

void BaseRegion::SetColor(const Vector4& color) {
    cpuMaterialData_.color = color;
    if (auto engine = dx_->GetEngine()) {
        for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
            engine->GetMaterialBufferManager()->Update(materialCbIndex_, cpuMaterialData_, i);
        }
    }
}

void BaseRegion::SetEnvironmentCoefficient(float coefficient) {
    cpuMaterialData_.environmentCoefficient = coefficient;
    if (auto engine = dx_->GetEngine()) {
        for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
            engine->GetMaterialBufferManager()->Update(materialCbIndex_, cpuMaterialData_, i);
        }
    }
}

void BaseRegion::SetInstanceColor(uint32_t index, const Vector4& color) {
    if (index < instanceColors_.size()) {
        instanceColors_[index] = color;
        instanceDirty_ = true;
    } else {
        assert(false && "BaseRegion::SetInstanceColor: index out of range");
    }
}

void BaseRegion::SetAllInstanceColor(const Vector4& color) {
    for (auto& c : instanceColors_) {
        c = color;
    }
    for (auto& c : instanceWorldColors_) {
        c = color;
    }
    instanceDirty_ = true;
}

void BaseRegion::AddInstance(const Transform& t) {
    instances_.push_back(t);
    instanceColors_.push_back({1.0f, 1.0f, 1.0f, 1.0f});
    instanceDirty_ = true;
}

void BaseRegion::AddInstance(const Transform& t, const Vector4& color) {
    instances_.push_back(t);
    instanceColors_.push_back(color);
    instanceDirty_ = true;
}

void BaseRegion::AddInstance(const Vector3& center, float scale, const Vector3& rotate) {
    Transform t;
    t.translate = center;
    t.scale = {scale, scale, scale};
    t.rotate = rotate;
    instances_.push_back(t);
    instanceColors_.push_back({1.0f, 1.0f, 1.0f, 1.0f});
    instanceDirty_ = true;
}

void BaseRegion::AddInstance(const Vector3& center, float scale, const Vector3& rotate, const Vector4& color) {
    Transform t;
    t.translate = center;
    t.scale = {scale, scale, scale};
    t.rotate = rotate;
    instances_.push_back(t);
    instanceColors_.push_back(color);
    instanceDirty_ = true;
}

void BaseRegion::AddInstanceWorld(const Matrix4x4& world, const Vector4& color) {
    instanceWorlds_.push_back(world);
    instanceWorldColors_.push_back(color);
    instanceDirty_ = true;
}

void BaseRegion::UpdateInstance(uint32_t index, const Transform& t) {
    if (index < instances_.size()) {
        instances_[index] = t;
        instanceDirty_ = true;
    } else {
        assert(false && "BaseRegion::UpdateInstance: index out of range");
    }
}

void BaseRegion::ClearInstances() {
    instances_.clear();
    instanceColors_.clear();
    instanceWorlds_.clear();
    instanceWorldColors_.clear();
    instanceDirty_ = true;
}

void BaseRegion::CreateOrResizeInstanceBuffer(uint32_t instanceCount) {
    const UINT stride = sizeof(InstanceData);
    const UINT sizeInBytes = std::max<UINT>(stride * instanceCount, stride);
    uint32_t frameIndex = dx_->GetFrameIndex();

    if (!instanceBuffer_[frameIndex] || instanceBuffer_[frameIndex]->GetDesc().Width < sizeInBytes) {
        instanceBuffer_[frameIndex] = dx_->CreateBufferResource(sizeInBytes);

        if (instancingSrvIndex_[frameIndex] == UINT32_MAX) {
            assert(srvPool_);
            uint32_t idx = srvPool_->Allocate();
            if (idx == DescriptorPool::kInvalid) { OutputDebugStringA("BaseRegion SRV allocate failed\n"); return; }
            instancingSrvIndex_[frameIndex] = idx;
            instancingSrvCPU_[frameIndex] = srvPool_->GetCPUHandle(idx);
            instancingSrvGPU_[frameIndex] = srvPool_->GetGPUHandle(idx);
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
        srv.Format = DXGI_FORMAT_UNKNOWN;
        srv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv.Buffer.FirstElement = 0;
        srv.Buffer.NumElements = instanceCount;
        srv.Buffer.StructureByteStride = stride;
        srv.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

        dx_->GetDevice()->CreateShaderResourceView(instanceBuffer_[frameIndex].Get(), &srv, instancingSrvCPU_[frameIndex]);
    }
}

void BaseRegion::BuildInstanceBuffer(bool force) {
    if (instances_.empty() && instanceWorlds_.empty()) { 
        visibleInstanceCount_ = 0;
        return; 
    }
    if (!force && !instanceDirty_) { return; }

    const UINT totalCount = static_cast<UINT>(instances_.size() + instanceWorlds_.size());
    std::vector<InstanceData> temp;
    temp.reserve(totalCount);

    const Matrix4x4* view = nullptr;
    const Matrix4x4* proj = nullptr;
    const Frustum* frustum = nullptr;
    
    Camera* activeCamera = nullptr;
    if (dx_ && dx_->GetEngine() && dx_->GetEngine()->GetCameraManager()) {
        activeCamera = dx_->GetEngine()->GetCameraManager()->GetActiveCamera();
    }

    if (activeCamera) {
        // we can add frustum culling if camera is set
        frustum = &activeCamera->GetFrustum();
    }

    float modelRadius = GetBoundingSphereRadius();

    // Transform based instances
    for (size_t i = 0; i < instances_.size(); ++i) {
        const auto& inst = instances_[i];
        
        if (isCullingEnabled_ && activeCamera && frustum) {
            float maxScale = (std::max)({ inst.scale.x, inst.scale.y, inst.scale.z });
            Sphere boundingSphere;
            boundingSphere.center = inst.translate;
            boundingSphere.radius = modelRadius * maxScale * 1.1f;

            if (!Collision::IsCollision(*frustum, boundingSphere)) {
                continue;
            }
        }

        InstanceData data;
        Matrix4x4 world = Math::MakeAffineMatrix(inst.scale, inst.rotate, inst.translate);
        data.WVP = Math::MakeIdentity4x4(); // Used to be calculated here, now unused in shader

        Matrix4x4 worldForNormal = world;
        worldForNormal.m[3][0] = 0.0f;
        worldForNormal.m[3][1] = 0.0f;
        worldForNormal.m[3][2] = 0.0f;
        worldForNormal.m[3][3] = 1.0f;

        data.World = world;
        data.WorldInverseTranspose = Math::Transpose(Math::Inverse(worldForNormal));
        data.color = instanceColors_[i];
        
        temp.push_back(data);
    }

    // World matrix based instances
    for (size_t i = 0; i < instanceWorlds_.size(); ++i) {
        InstanceData data;
        data.WVP = Math::MakeIdentity4x4();
        data.World = instanceWorlds_[i];

        Matrix4x4 worldForNormal = instanceWorlds_[i];
        worldForNormal.m[3][0] = 0.0f;
        worldForNormal.m[3][1] = 0.0f;
        worldForNormal.m[3][2] = 0.0f;
        worldForNormal.m[3][3] = 1.0f;
        
        data.WorldInverseTranspose = Math::Transpose(Math::Inverse(worldForNormal));
        data.color = instanceWorldColors_[i];

        // For simplicity, world-based instances bypass culling
        temp.push_back(data);
    }

    visibleInstanceCount_ = static_cast<uint32_t>(temp.size());
    if (visibleInstanceCount_ == 0) {
        instanceDirty_ = false;
        return;
    }

    CreateOrResizeInstanceBuffer(totalCount);

    uint32_t frameIndex = dx_->GetFrameIndex();
    lastUpdateFrameIndex_ = frameIndex;
    uint8_t* dst = nullptr;
    HRESULT hr = instanceBuffer_[frameIndex]->Map(0, nullptr, reinterpret_cast<void**>(&dst));
    assert(SUCCEEDED(hr));
    std::memcpy(dst, temp.data(), sizeof(InstanceData) * visibleInstanceCount_);
    instanceBuffer_[frameIndex]->Unmap(0, nullptr);

    instanceDirty_ = false;
}

void BaseRegion::SyncBeforeDraw() {
    if (instanceDirty_) {
        BuildInstanceBuffer(false);
    }
}

D3D12_GPU_VIRTUAL_ADDRESS BaseRegion::GetMaterialVAddress() const {
    if (materialCbIndex_ == static_cast<uint32_t>(-1)) return 0;
    return dx_->GetEngine()->GetMaterialBufferManager()->GetGPUVirtualAddress(materialCbIndex_, dx_->GetFrameIndex());
}
