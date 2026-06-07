#pragma once
#include "../Core/BaseResource.h"
#include <vector>
#include "../../Engine/Graphics/DirectX/DirectXCommon.h"
#include <wrl.h>
#include <d3d12.h>
#include "../../Engine/Graphics/Data/VertexData.h"
#include "../../Engine/Graphics/Data/Material.h"
#include "../../Engine/Graphics/Data/TransformationMatrix.h"
#include "../../Engine/Core/Math/Transform.h"
#include "../../Engine/Graphics/DirectX/DynamicConstantBuffer.h"

class Camera;

class Object3DResource : public BaseResource {
public:
    virtual ~Object3DResource();

    void CreateResource() override;
    void Map() override;
    void Unmap() override;

    void UpdateTransform(const Camera& camera);

    void SetCustomPSO(ID3D12PipelineState* pso) { customPSO_ = pso; }
    ID3D12PipelineState* GetCustomPSO() const { return customPSO_; }

    void SetCustomCBVAddress(D3D12_GPU_VIRTUAL_ADDRESS addr) { customCBVAddress_ = addr; }
    D3D12_GPU_VIRTUAL_ADDRESS GetCustomCBVAddress() const { return customCBVAddress_; }

public:
    // --- 頂点バッファ ---
    std::vector<VertexData> vertexDataList_{};
    VertexData* vertexData_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_ = nullptr;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

    // --- インデックスバッファ ---
    std::vector<uint32_t> indexDataList_{};
    uint32_t* indexData_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_ = nullptr;
    D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
    uint32_t indexCount_ = 0;

    // --- マテリアル ---
    Transform uvTransform_{ {1.0f,1.0f,1.0f}, {0.0f,0.0f,0.0f}, {0.0f,0.0f,0.0f} };
    Material cpuMaterialData_{};
    Material* GetMaterialData() { return &cpuMaterialData_; }
    
    uint32_t materialCbIndex_ = static_cast<uint32_t>(-1);

    // --- トランスフォーム ---
    Transform transform_{ {1.0f,1.0f,1.0f}, {0.0f,0.0f,0.0f}, {0.0f,0.0f,0.0f} };
    TransformationMatrix transformationMatrix_{};
    
    uint32_t transformCbIndex_ = static_cast<uint32_t>(-1);
    uint32_t* externalTransformCbIndex_ = nullptr;
    
    const TransformationMatrix& GetTransformationMatrix() const { return transformationMatrix_; }

    // --- getters ---
    D3D12_GPU_VIRTUAL_ADDRESS GetMaterialVAddress() const;
    D3D12_GPU_VIRTUAL_ADDRESS GetTransformVAddress() const;
    
    void SyncBeforeDraw();
    
    // --- 外部リソースの借用 (StaticModelObject/AnimationModel等で共有するため) ---
    void SetExternalTransformCbIndex(uint32_t* externalCbIndex) {
        externalTransformCbIndex_ = externalCbIndex;
    }

    // --- テクスチャ ---
    D3D12_GPU_DESCRIPTOR_HANDLE textureHandle_ = {};

    bool isFirstUpdate_ = true;

    ID3D12PipelineState* customPSO_ = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS customCBVAddress_ = 0;
};
