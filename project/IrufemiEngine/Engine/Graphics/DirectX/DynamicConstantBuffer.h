#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <array>
#include <vector>
#include <cstdint>
#include <cassert>
#include "DirectXCommon.h"

/**
 * @class DynamicConstantBuffer
 * @brief エンジン全体で共有される巨大な定数バッファを管理するクラス
 * @details 数万オブジェクトの定数データを1つの ID3D12Resource で管理し、
 * 各オブジェクトにはインデックスのみを割り当てます。
 */
template <typename T>
class DynamicConstantBuffer {
public:
    DynamicConstantBuffer() = default;
    ~DynamicConstantBuffer() {
        Unmap();
        if (dxCommon_) {
            for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
                if (resources_[i]) {
                    dxCommon_->ReleaseAfterFence(resources_[i]);
                }
            }
        }
    }

    /**
     * @brief バッファの初期化
     * @param dxCommon DirectX基盤
     * @param capacity 確保する最大要素数（例: 65536）
     */
    void Initialize(DirectXCommon* dxCommon, uint32_t capacity) {
        assert(dxCommon != nullptr);
        dxCommon_ = dxCommon;
        capacity_ = capacity;
        // 定数バッファは256バイトアライメント必須
        elementAlignedSize_ = (sizeof(T) + 255) & ~255;
        
        for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
            resources_[i] = dxCommon_->CreateBufferResource(elementAlignedSize_ * capacity_);
            HRESULT hr = resources_[i]->Map(0, nullptr, reinterpret_cast<void**>(&mappedRawData_[i]));
            assert(SUCCEEDED(hr));
        }
    }

    /**
     * @brief 定数バッファの1ブロックを割り当てる
     * @return 割り当てられたインデックス
     */
    uint32_t Allocate() {
        if (!freeIndices_.empty()) {
            uint32_t index = freeIndices_.back();
            freeIndices_.pop_back();
            return index;
        }
        assert(nextIndex_ < capacity_ && "DynamicConstantBuffer capacity exceeded!");
        return nextIndex_++;
    }

    /**
     * @brief 使用終わったインデックスを返却する
     * @param index 返却するインデックス
     */
    void Free(uint32_t index) {
        // 解放されたインデックスを再利用できるようにリストに追加
        freeIndices_.push_back(index);
    }

    /**
     * @brief データを更新する
     * @param index 対象のインデックス
     * @param data 書き込むデータ
     * @param frameIndex 対象のフレームバッファ
     */
    void Update(uint32_t index, const T& data, uint32_t frameIndex) {
        T* ptr = reinterpret_cast<T*>(mappedRawData_[frameIndex] + index * elementAlignedSize_);
        *ptr = data;
    }

    /**
     * @brief GPU仮想アドレスを取得する
     * @param index 対象のインデックス
     * @param frameIndex 対象のフレームバッファ
     * @return D3D12_GPU_VIRTUAL_ADDRESS
     */
    D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress(uint32_t index, uint32_t frameIndex) const {
        return resources_[frameIndex]->GetGPUVirtualAddress() + index * elementAlignedSize_;
    }

private:
    void Unmap() {
        for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
            if (resources_[i] && mappedRawData_[i]) {
                resources_[i]->Unmap(0, nullptr);
                mappedRawData_[i] = nullptr;
            }
        }
    }

private:
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kMaxFramesInFlight> resources_;
    std::array<uint8_t*, kMaxFramesInFlight> mappedRawData_ = {nullptr};
    std::vector<uint32_t> freeIndices_;
    uint32_t nextIndex_ = 0;
    uint32_t capacity_ = 0;
    size_t elementAlignedSize_ = 0;
    DirectXCommon* dxCommon_ = nullptr;
};
