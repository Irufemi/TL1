#pragma once

#include <vector>
#include "Engine/Core/Math/Matrix4x4.h"
#include <wrl.h>
#include <d3d12.h>
#include <span>
#include <array>
#include "VertexInfluence.h"
#include "WellForGPU.h"
#include "Engine/Graphics/DirectX/DirectXCommon.h"

struct SkinningInformation {
    uint32_t numVertices;
};

struct SkinCluster {
    std::vector<Matrix4x4> inverseBindPoseMatrices;
    Microsoft::WRL::ComPtr<ID3D12Resource> influenceResource;
    D3D12_VERTEX_BUFFER_VIEW influenceBufferView;
    std::span<VertexInfluence> mappedInfluence;
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kMaxFramesInFlight> paletteResource;
    std::array<std::span<WellForGPU>, kMaxFramesInFlight> mappedPalette;
    std::array<std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE>, kMaxFramesInFlight> paletteSrvHandle;

    // コンピュートシェーダー用リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> skinningInformationResource;
    SkinningInformation* mappedSkinningInformation = nullptr;

    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kMaxFramesInFlight> skinnedVertexResource;
    std::array<D3D12_VERTEX_BUFFER_VIEW, kMaxFramesInFlight> skinnedVertexBufferView;
    std::array<std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE>, kMaxFramesInFlight> skinnedVertexSrvHandle;
    std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> influenceSrvHandle;
    std::array<std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE>, kMaxFramesInFlight> skinnedVertexUavHandle;
};