#pragma once
#include "../Core/BaseResource.h"
#include <vector>
#include <wrl.h>
#include <d3d12.h>
#include "../../Engine/Graphics/DirectX/DynamicConstantBuffer.h"
#include "../../Engine/Graphics/Data/VertexData.h"
#include "../../Engine/Graphics/Data/Material.h"
#include "../../Engine/Graphics/Data/TransformationMatrix.h"
#include "../../Engine/Core/Math/Transform.h"

class Camera;

class LineResource : public BaseResource {
public:
    virtual ~LineResource();

    void CreateResource() override;
    void Map() override;
    void Unmap() override;

    void UpdateTransform(const Camera& camera);

public:
    // --- 頂点バッファ ---
    VertexData* vertexData_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_ = nullptr;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

    // --- インデックスバッファ ---
    uint32_t* indexData_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_ = nullptr;
    D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
    uint32_t indexCount_ = 0;

    // --- マテリアル ---
    Material cpuMaterialData_{};
    uint32_t materialCbIndex_ = static_cast<uint32_t>(-1);

    // --- トランスフォーム ---
    Transform transform_{ {1.0f,1.0f,1.0f}, {0.0f,0.0f,0.0f}, {0.0f,0.0f,0.0f} };
    TransformationMatrix transformationMatrix_{};
    uint32_t transformCbIndex_ = static_cast<uint32_t>(-1);

    D3D12_GPU_VIRTUAL_ADDRESS GetMaterialVAddress() const;
    D3D12_GPU_VIRTUAL_ADDRESS GetTransformVAddress() const;



    void SyncBeforeDraw();
};
