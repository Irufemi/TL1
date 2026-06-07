#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <queue>
#include <mutex>
#include <cstdint>
#include <limits>

class DescriptorAllocator {
public:
    static constexpr uint32_t kInvalid = 0xFFFFFFFFu; // std::numeric_limits<uint32_t>::max() の代用

    DescriptorAllocator(ID3D12DescriptorHeap* heap, uint32_t descriptorSize, uint32_t baseIndex = 0);

    uint32_t Allocate();
    void Free(uint32_t index);
    void FreeAfterFence(uint32_t index, uint64_t safeFence);
    void GarbageCollect(uint64_t completedFence);

    // 使用中インデックス集合(昇順ユニーク)を渡してフリーリストを再構築
    void RebuildFreeListExcept(const std::vector<uint32_t>& usedSortedUnique);

    // 先頭の予約(ImGui 等)
    void ReservePrefix(uint32_t count);

    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(uint32_t index) const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(uint32_t index) const;

    uint32_t Capacity() const { return capacity_; }
    uint32_t BaseIndex() const { return baseIndex_; }

private:
    struct Pending {
        uint64_t fence;
        uint32_t index;
        bool operator<(const Pending& rhs) const { return fence > rhs.fence; } // フェンス小→大
    };

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap_;
    uint32_t descriptorSize_ = 0;
    uint32_t capacity_ = 0;
    uint32_t baseIndex_ = 0;
    uint32_t nextIndex_ = 0;

    std::vector<uint32_t> freeList_;
    std::priority_queue<Pending> pending_;
    mutable std::mutex mutex_;
};