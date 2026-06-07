#pragma once

#include <d3d12.h>
#include <cstdint>
#include <functional>
#include "../../Core/Type/BlendMode.h"
#include "../Pipeline/PSOManager.h"

/**
 * @file RenderPackets.h
 * @brief 描画命令（パケット）の構造体定義群
 */

// 前方宣言
class Object3DResource;
class Object2DResource;
class LineResource;
struct GpuMesh;

namespace RenderPackets {

    struct Standard3DPacket {
        const class Object3DResource* resource;
        const D3D12_VERTEX_BUFFER_VIEW* vertexBufferViewOverride;
        BlendMode blendMode;
        PSOManager::DepthWrite depthWrite;
        PSOManager::CullMode cullMode;
        bool castShadows;
        ID3D12PipelineState* customPSO = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS customCBVAddress = 0;
        ID3D12Resource* vertexBufferResourceOverride = nullptr;
    };

    struct SpritePacket {
        const class Object2DResource* resource;
        BlendMode blendMode;
        PSOManager::DepthWrite depthWrite;
        PSOManager::CullMode cullMode;
        ID3D12PipelineState* customPSO = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS customCBVAddress = 0;
    };



    struct LinePacket {
        const class LineResource* resource;
        D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU;
        UINT instanceCount;
        BlendMode blendMode;
        PSOManager::DepthWrite depthWrite;
        PSOManager::CullMode cullMode;
        ID3D12PipelineState* customPSO = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS customCBVAddress = 0;
    };

    struct GPUParticlePacket {
        D3D12_VERTEX_BUFFER_VIEW vbv;
        D3D12_INDEX_BUFFER_VIEW ibv;
        uint32_t indexCount;
        D3D12_GPU_VIRTUAL_ADDRESS materialAddress;
        D3D12_GPU_VIRTUAL_ADDRESS perViewAddress;
        D3D12_GPU_VIRTUAL_ADDRESS emitterAddress;
        D3D12_GPU_DESCRIPTOR_HANDLE particleSrvHandle;
        D3D12_GPU_DESCRIPTOR_HANDLE sortListSrvHandle;
        D3D12_GPU_DESCRIPTOR_HANDLE textureHandle;
        uint32_t instanceCount;
        ID3D12Resource* particleResource;
        BlendMode blendMode;
        PSOManager::DepthWrite depthWrite;
        PSOManager::CullMode cullMode;
        ID3D12PipelineState* customPSO = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS customCBVAddress = 0;
    };

    struct VoxelParticlePacket {
        uint32_t instanceCount;
        D3D12_VERTEX_BUFFER_VIEW vbv;
        D3D12_INDEX_BUFFER_VIEW ibv;
        uint32_t indexCount;
        D3D12_GPU_VIRTUAL_ADDRESS emitterAddress;
        D3D12_GPU_DESCRIPTOR_HANDLE particleDataHandle;
        ID3D12Resource* particleResource;
        ID3D12PipelineState* drawPSO;
    };

    struct SkyboxPacket {
        D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
        D3D12_INDEX_BUFFER_VIEW indexBufferView;
        D3D12_GPU_VIRTUAL_ADDRESS materialAddress;
        D3D12_GPU_VIRTUAL_ADDRESS transformationAddress;
        D3D12_GPU_DESCRIPTOR_HANDLE textureHandle;
        UINT indexCount;
    };

    struct PrimitiveRegionPacket {
        D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
        D3D12_INDEX_BUFFER_VIEW indexBufferView;
        D3D12_GPU_VIRTUAL_ADDRESS materialAddress;
        D3D12_GPU_DESCRIPTOR_HANDLE textureHandle;
        D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU;
        UINT indexCount;
        UINT instanceCount;
        BlendMode blendMode;
        PSOManager::DepthWrite depthWrite;
        PSOManager::CullMode cullMode;
        bool castShadows;
        ID3D12PipelineState* customPSO = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS customCBVAddress = 0;
    };

    struct ModelRegionPacket {
        const struct GpuMesh* gpuMesh;
        D3D12_GPU_VIRTUAL_ADDRESS materialAddress;
        D3D12_GPU_DESCRIPTOR_HANDLE textureHandle;
        D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU;
        UINT instanceCount;
        BlendMode blendMode;
        PSOManager::DepthWrite depthWrite;
        PSOManager::CullMode cullMode;
        bool castShadows;
        ID3D12PipelineState* customPSO = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS customCBVAddress = 0;
    };

} // namespace RenderPackets
