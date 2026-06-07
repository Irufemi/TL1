#include "DXSwapChainManager.h"
#include <cassert>
#include <algorithm>

void DXSwapChainManager::Initialize(ID3D12Device* device, IDXGIFactory7* dxgiFactory, ID3D12CommandQueue* commandQueue, HWND hwnd, int32_t width, int32_t height) {
    descriptorSizeRTV_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    descriptorSizeDSV_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

    CreateSwapChain(dxgiFactory, commandQueue, hwnd, width, height);
    CreateDescriptorHeaps(device);
    InitializeRenderTargets(device);
    CreateDepthStencil(device, width, height);
}

void DXSwapChainManager::Finalize() {
    depthStencilResource_.Reset();
    rtvDescriptorHeap_.Reset();
    dsvDescriptorHeap_.Reset();
    swapChainResources_[0].Reset();
    swapChainResources_[1].Reset();
    swapChain_.Reset();
}

void DXSwapChainManager::CreateSwapChain(IDXGIFactory7* dxgiFactory, ID3D12CommandQueue* commandQueue, HWND hwnd, int32_t width, int32_t height) {
    swapChainDesc_.Width = width;
    swapChainDesc_.Height = height;
    swapChainDesc_.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc_.SampleDesc.Count = 1;
    swapChainDesc_.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc_.BufferCount = 2;
    swapChainDesc_.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    HRESULT hr = dxgiFactory->CreateSwapChainForHwnd(
        commandQueue, 
        hwnd, 
        &swapChainDesc_, 
        nullptr, 
        nullptr, 
        reinterpret_cast<IDXGISwapChain1**>(swapChain_.GetAddressOf())
    );
    assert(SUCCEEDED(hr));

    for (uint32_t i = 0; i < 2; ++i) {
        hr = swapChain_->GetBuffer(i, IID_PPV_ARGS(swapChainResources_[i].GetAddressOf()));
        assert(SUCCEEDED(hr));
    }
}

void DXSwapChainManager::CreateDescriptorHeaps(ID3D12Device* device) {
    // RTV用ヒープ作成
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = 128;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    HRESULT hr = device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(rtvDescriptorHeap_.GetAddressOf()));
    assert(SUCCEEDED(hr));

    // DSV用ヒープ作成
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.NumDescriptors = 16;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    hr = device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(dsvDescriptorHeap_.GetAddressOf()));
    assert(SUCCEEDED(hr));

    nextRtvIndex_ = 4; // 0, 1 は SwapChain 用、2, 3 は ImGui 用に予約
    nextDsvIndex_ = 1; // 0 はメインの深度バッファ
}

void DXSwapChainManager::InitializeRenderTargets(ID3D12Device* device) {
    rtvDesc_.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    rtvDesc_.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

    D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    for (uint32_t i = 0; i < 2; ++i) {
        rtvHandles_[i].ptr = rtvStartHandle.ptr + (i * descriptorSizeRTV_);
        device->CreateRenderTargetView(swapChainResources_[i].Get(), &rtvDesc_, rtvHandles_[i]);
    }

    // ImGui用 RTV
    D3D12_RENDER_TARGET_VIEW_DESC imGuiRtvDesc = rtvDesc_;
    imGuiRtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    for (uint32_t i = 0; i < 2; ++i) {
        rtvHandles_[i + 2].ptr = rtvHandles_[1].ptr + ((i + 1) * descriptorSizeRTV_);
        device->CreateRenderTargetView(swapChainResources_[i].Get(), &imGuiRtvDesc, rtvHandles_[i + 2]);
    }
}

Microsoft::WRL::ComPtr<ID3D12Resource> DXSwapChainManager::CreateDepthStencilTextureResource(ID3D12Device* device, int32_t width, int32_t height) {
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = width;
    resourceDesc.Height = height;
    resourceDesc.MipLevels = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_CLEAR_VALUE depthClearValue{};
    depthClearValue.DepthStencil.Depth = 1.0f;
    depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

    Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
    HRESULT hr = device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &depthClearValue,
        IID_PPV_ARGS(resource.GetAddressOf())
    );
    assert(SUCCEEDED(hr));

    return resource;
}

void DXSwapChainManager::CreateDepthStencil(ID3D12Device* device, int32_t width, int32_t height) {
    depthStencilResource_ = CreateDepthStencilTextureResource(device, width, height);

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    device->CreateDepthStencilView(depthStencilResource_.Get(), &dsvDesc, dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart());
}

void DXSwapChainManager::ReleaseSwapChainResources() {
    for (auto& res : swapChainResources_) {
        res.Reset();
    }
    depthStencilResource_.Reset();
}

void DXSwapChainManager::ResizeSwapChain(ID3D12Device* device, int32_t width, int32_t height) {
    if (width <= 0 || height <= 0) return;

    ReleaseSwapChainResources();

    DXGI_SWAP_CHAIN_DESC1 desc{};
    swapChain_->GetDesc1(&desc);
    HRESULT hr = swapChain_->ResizeBuffers(desc.BufferCount, width, height, desc.Format, desc.Flags);
    assert(SUCCEEDED(hr));

    for (uint32_t i = 0; i < desc.BufferCount; ++i) {
        hr = swapChain_->GetBuffer(i, IID_PPV_ARGS(swapChainResources_[i].GetAddressOf()));
        assert(SUCCEEDED(hr));
        
        device->CreateRenderTargetView(swapChainResources_[i].Get(), &rtvDesc_, rtvHandles_[i]);

        D3D12_RENDER_TARGET_VIEW_DESC imGuiRtvDesc = rtvDesc_;
        imGuiRtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        rtvHandles_[i + 2].ptr = rtvHandles_[1].ptr + ((i + 1) * descriptorSizeRTV_);
        device->CreateRenderTargetView(swapChainResources_[i].Get(), &imGuiRtvDesc, rtvHandles_[i + 2]);
    }

    depthStencilResource_ = CreateDepthStencilTextureResource(device, width, height);
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    device->CreateDepthStencilView(depthStencilResource_.Get(), &dsvDesc, dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart());
}

D3D12_CPU_DESCRIPTOR_HANDLE DXSwapChainManager::GetRTVCPUDescriptorHandle(uint32_t index) const {
    D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += (descriptorSizeRTV_ * index);
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE DXSwapChainManager::GetRTVGPUDescriptorHandle(uint32_t index) const {
    D3D12_GPU_DESCRIPTOR_HANDLE handle = rtvDescriptorHeap_->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += (descriptorSizeRTV_ * index);
    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE DXSwapChainManager::GetDSVCPUDescriptorHandle(uint32_t index) const {
    D3D12_CPU_DESCRIPTOR_HANDLE handle = dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += (descriptorSizeDSV_ * index);
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE DXSwapChainManager::GetDSVGPUDescriptorHandle(uint32_t index) const {
    D3D12_GPU_DESCRIPTOR_HANDLE handle = dsvDescriptorHeap_->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += (descriptorSizeDSV_ * index);
    return handle;
}

uint32_t DXSwapChainManager::AllocateRTVIndex() {
    std::lock_guard<std::mutex> lock(descriptorMutex_);
    if (!freeRtvIndices_.empty()) {
        uint32_t index = freeRtvIndices_.back();
        freeRtvIndices_.pop_back();
        return index;
    }
    assert(nextRtvIndex_ < 128);
    return nextRtvIndex_++;
}

void DXSwapChainManager::FreeRTVIndex(uint32_t index, uint64_t currentFenceValue) {
    if (index == 0xFFFFFFFF) return;
    std::lock_guard<std::mutex> lock(descriptorMutex_);
    pendingFreeRtvs_.push_back({ currentFenceValue, index });
}

uint32_t DXSwapChainManager::AllocateDSVIndex() {
    std::lock_guard<std::mutex> lock(descriptorMutex_);
    if (!freeDsvIndices_.empty()) {
        uint32_t index = freeDsvIndices_.back();
        freeDsvIndices_.pop_back();
        return index;
    }
    assert(nextDsvIndex_ < 16);
    return nextDsvIndex_++;
}

void DXSwapChainManager::FreeDSVIndex(uint32_t index, uint64_t currentFenceValue) {
    if (index == 0xFFFFFFFF) return;
    std::lock_guard<std::mutex> lock(descriptorMutex_);
    pendingFreeDsvs_.push_back({ currentFenceValue, index });
}

void DXSwapChainManager::FlushPendingDescriptors(uint64_t completedFenceValue) {
    std::lock_guard<std::mutex> lock(descriptorMutex_);

    for (const auto& d : pendingFreeRtvs_) {
        if (d.fenceValue <= completedFenceValue) {
            freeRtvIndices_.push_back(d.index);
        }
    }
    auto rtvIt = std::remove_if(pendingFreeRtvs_.begin(), pendingFreeRtvs_.end(), [completedFenceValue](const PendingDescriptor& d) {
        return d.fenceValue <= completedFenceValue;
    });
    pendingFreeRtvs_.erase(rtvIt, pendingFreeRtvs_.end());

    for (const auto& d : pendingFreeDsvs_) {
        if (d.fenceValue <= completedFenceValue) {
            freeDsvIndices_.push_back(d.index);
        }
    }
    auto dsvIt = std::remove_if(pendingFreeDsvs_.begin(), pendingFreeDsvs_.end(), [completedFenceValue](const PendingDescriptor& d) {
        return d.fenceValue <= completedFenceValue;
    });
    pendingFreeDsvs_.erase(dsvIt, pendingFreeDsvs_.end());
}
