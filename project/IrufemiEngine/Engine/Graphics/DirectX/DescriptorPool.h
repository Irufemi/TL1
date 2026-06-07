#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <queue>
#include <mutex>
#include <cstdint>
#include <limits>

class DescriptorPool {
public:

    static constexpr uint32_t kMaxSRVCount = 16384;
    static constexpr uint32_t kInvalid = 0xFFFFFFFFu; // std::numeric_limits<uint32_t>::max() の代用

    DescriptorPool() = default;
    void Initialize(ID3D12Device* device);

    uint32_t Allocate(uint32_t count = 1);
    void Free(uint32_t index);
    void FreeAfterFence(uint32_t index, uint64_t safeFence);
    void GarbageCollect(uint64_t completedFence);

    // 使用中インデックス集合(昇順ユニーク)を渡してフリーリストを再構築
    void RebuildFreeListExcept(const std::vector<uint32_t>& usedSortedUnique);

    // 先頭の予約(ImGui 等)
    void ReservePrefix(uint32_t count);

    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(uint32_t index) const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(uint32_t index) const;

    // SRV作成
    void CreateSRVForTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT format, UINT mipLevels);
    void CreateSRVForStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);

    ID3D12DescriptorHeap* GetHeap() const { return heap_.Get(); }
    uint32_t Capacity() const { return kMaxSRVCount; }
    uint32_t BaseIndex() const { return baseIndex_; }

private:
    struct Pending {
        uint64_t fence;
        uint32_t index;
        bool operator<(const Pending& rhs) const { return fence > rhs.fence; } // フェンス小→大
    };

    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap_;
    uint32_t descriptorSize_ = 0;
    uint32_t baseIndex_ = 0;
    uint32_t nextIndex_ = 0;

    std::vector<uint32_t> freeList_;
    std::priority_queue<Pending> pending_;
    mutable std::mutex mutex_;
};