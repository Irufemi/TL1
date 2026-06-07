#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <array>
#include <cstdint>
#include <cassert>
#include "DirectXCommon.h"

// マルチバッファリング対応 汎用定数バッファ管理クラステンプレート
template <typename T>
class ConstantBuffer {
public:
    ConstantBuffer() = default;
    ~ConstantBuffer() {
        Unmap();
        if (dxCommon_) {
            for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
                if (resources_[i]) {
                    dxCommon_->ReleaseAfterFence(resources_[i]);
                }
            }
        }
    }

    // バッファを生成しマッピングする（kMaxFramesInFlight分）
    void Initialize(DirectXCommon* dxCommon) {
        assert(dxCommon != nullptr);
        dxCommon_ = dxCommon;
        // 定数バッファの制約：サイズは256バイトの倍数
        size_t alignedSize = (sizeof(T) + 255) & ~255;
        for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
            resources_[i] = dxCommon_->CreateBufferResource(alignedSize);
            HRESULT hr = resources_[i]->Map(0, nullptr, reinterpret_cast<void**>(&mappedData_[i]));
            assert(SUCCEEDED(hr));
        }
    }

    // frameIndex番目のバッファにデータを書き込む
    void Update(const T& data, uint32_t frameIndex) {
        if (mappedData_[frameIndex]) {
            *mappedData_[frameIndex] = data;
        }
    }

    // すべてのフレームバッファ（kMaxFramesInFlight分）に同じデータを書き込む（初期化用）
    void UpdateAll(const T& data) {
        for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
            if (mappedData_[i]) {
                *mappedData_[i] = data;
            }
        }
    }

    // マッピングされたメモリへの直接アクセス
    T* operator[](uint32_t frameIndex) {
        return mappedData_[frameIndex];
    }
    const T* operator[](uint32_t frameIndex) const {
        return mappedData_[frameIndex];
    }

    // DirectX リソース用ゲッター
    ID3D12Resource* GetResource(uint32_t frameIndex) const {
        return resources_[frameIndex].Get();
    }

    D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress(uint32_t frameIndex) const {
        return resources_[frameIndex]->GetGPUVirtualAddress();
    }

private:
    void Unmap() {
        for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
            if (resources_[i] && mappedData_[i]) {
                resources_[i]->Unmap(0, nullptr);
                mappedData_[i] = nullptr;
            }
        }
    }

private:
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kMaxFramesInFlight> resources_;
    std::array<T*, kMaxFramesInFlight> mappedData_ = {nullptr};
    DirectXCommon* dxCommon_ = nullptr;
};
