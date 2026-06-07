#include "Object3DResource.h"
#include "Engine/Graphics/DirectX/DirectXCommon.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/IrufemiEngine.h"

Object3DResource::~Object3DResource() {
    Unmap();
    
    if (auto dxCommon = BaseResource::GetDirectXCommon()) {
        if (auto engine = dxCommon->GetEngine()) {
            if (materialCbIndex_ != static_cast<uint32_t>(-1)) {
                engine->GetMaterialBufferManager()->Free(materialCbIndex_);
            }
            if (transformCbIndex_ != static_cast<uint32_t>(-1)) {
                engine->GetTransformBufferManager()->Free(transformCbIndex_);
            }
        }
    }
}

void Object3DResource::CreateResource() {
    if (!s_dxCommon_) return;

    if (!vertexDataList_.empty()) {
        vertexResource_ = s_dxCommon_->CreateBufferResource(sizeof(VertexData) * vertexDataList_.size());
        vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
        vertexBufferView_.SizeInBytes = static_cast<UINT>(sizeof(VertexData) * vertexDataList_.size());
        vertexBufferView_.StrideInBytes = sizeof(VertexData);
    }

    if (!indexDataList_.empty()) {
        indexResource_ = s_dxCommon_->CreateBufferResource(sizeof(uint32_t) * indexDataList_.size());
        indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
        indexBufferView_.SizeInBytes = static_cast<UINT>(sizeof(uint32_t) * indexDataList_.size());
        indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
        indexCount_ = static_cast<uint32_t>(indexDataList_.size());
    }

    if (auto engine = BaseResource::GetDirectXCommon()->GetEngine()) {
        materialCbIndex_ = engine->GetMaterialBufferManager()->Allocate();
        
        cpuMaterialData_.color = {1,1,1,1};
        cpuMaterialData_.enableLighting = true;
        cpuMaterialData_.uvTransform = Math::MakeIdentity4x4();
        cpuMaterialData_.metallic = 0.0f;
        cpuMaterialData_.roughness = 0.5f;
        cpuMaterialData_.environmentCoefficient = 0.0f;
        
        for(uint32_t i=0; i<kMaxFramesInFlight; ++i){
            engine->GetMaterialBufferManager()->Update(materialCbIndex_, cpuMaterialData_, i);
        }

        transformCbIndex_ = engine->GetTransformBufferManager()->Allocate();
    }
}

void Object3DResource::Map() {
    if (vertexResource_) {
        vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
    }
    if (indexResource_) {
        indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData_));
    }
}

void Object3DResource::Unmap() {
    if (vertexResource_ && vertexData_) {
        vertexResource_->Unmap(0, nullptr);
        vertexData_ = nullptr;
    }
    if (indexResource_ && indexData_) {
        indexResource_->Unmap(0, nullptr);
        indexData_ = nullptr;
    }
}

void Object3DResource::UpdateTransform(const Camera& camera) {


    transformationMatrix_.world = Math::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    // CPU側のマテリアルキャッシュにのみ反映させる

    // 法線変換用：平行移動を除いた World を使う
    Matrix4x4 worldForNormal = transformationMatrix_.world;
    worldForNormal.m[3][0] = 0.0f;
    worldForNormal.m[3][1] = 0.0f;
    worldForNormal.m[3][2] = 0.0f;
    worldForNormal.m[3][3] = 1.0f;
    
    // 逆転置行列を計算
    transformationMatrix_.WorldInverseTranspose = Math::Transpose(Math::Inverse(worldForNormal));

    // マテリアルの CPUキャッシュ更新 (描画直前の SyncBeforeDraw で GPUへ送られる)
    cpuMaterialData_.uvTransform = Math::MakeAffineMatrix(uvTransform_.scale, uvTransform_.rotate, uvTransform_.translate);
    
    MarkAsDirty();
}

D3D12_GPU_VIRTUAL_ADDRESS Object3DResource::GetMaterialVAddress() const {
    if (materialCbIndex_ == static_cast<uint32_t>(-1)) return 0;
    return BaseResource::GetDirectXCommon()->GetEngine()->GetMaterialBufferManager()->GetGPUVirtualAddress(materialCbIndex_, BaseResource::GetDirectXCommon()->GetFrameIndex());
}

D3D12_GPU_VIRTUAL_ADDRESS Object3DResource::GetTransformVAddress() const {
    if (externalTransformCbIndex_) {
        return BaseResource::GetDirectXCommon()->GetEngine()->GetTransformBufferManager()->GetGPUVirtualAddress(*externalTransformCbIndex_, BaseResource::GetDirectXCommon()->GetFrameIndex());
    }
    if (transformCbIndex_ == static_cast<uint32_t>(-1)) return 0;
    return BaseResource::GetDirectXCommon()->GetEngine()->GetTransformBufferManager()->GetGPUVirtualAddress(transformCbIndex_, BaseResource::GetDirectXCommon()->GetFrameIndex());
}

void Object3DResource::SyncBeforeDraw() {
    uint32_t frameIndex = BaseResource::GetDirectXCommon()->GetFrameIndex();
    
    if (CheckAndClearDirty(frameIndex)) {
        if (auto engine = BaseResource::GetDirectXCommon()->GetEngine()) {
            // 外部バッファがなければ自身を更新
            if (!externalTransformCbIndex_ && transformCbIndex_ != static_cast<uint32_t>(-1)) {
                engine->GetTransformBufferManager()->Update(transformCbIndex_, transformationMatrix_, frameIndex);
            }
            
            // マテリアルデータを更新
            if (materialCbIndex_ != static_cast<uint32_t>(-1)) {
                engine->GetMaterialBufferManager()->Update(materialCbIndex_, cpuMaterialData_, frameIndex);
            }
        }
        

    }
}

