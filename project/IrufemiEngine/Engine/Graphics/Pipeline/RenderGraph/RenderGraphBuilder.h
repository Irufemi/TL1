#pragma once
#include <d3d12.h>
#include <vector>
#include <string>

// 一時リソースの仮想ハンドル
typedef uint32_t TransientResourceHandle;
constexpr TransientResourceHandle kInvalidHandle = 0xFFFFFFFF;

struct TransientResourceDesc {
    D3D12_RESOURCE_DESC desc;
    D3D12_CLEAR_VALUE clearValue;
    bool hasClearValue;
    std::string name;
};

/**
 * @class RenderGraphBuilder
 * @brief 各描画パスが必要とするリソース（テクスチャやレンダーターゲット等）の状態を登録するためのビルダークラス
 */
class RenderGraphBuilder {
public:
    struct ResourceUsage {
        ID3D12Resource* resource;
        D3D12_RESOURCE_STATES state;
        size_t passIndex; // この要求を出したパスのインデックス
    };

    struct TransientResourceUsage {
        TransientResourceHandle handle;
        D3D12_RESOURCE_STATES state;
        size_t passIndex;
    };

    void SetCurrentPassIndex(size_t index) { currentPassIndex_ = index; }

    /**
     * @brief 特定のリソースがこのパスの実行時に指定したステートであることを要求する
     * @param resource 必須ステートを要求する対象のリソース
     * @param state 要求するステート
     */
    void RequireState(ID3D12Resource* resource, D3D12_RESOURCE_STATES state) {
        if (resource) {
            usages_.push_back({ resource, state, currentPassIndex_ });
        }
    }

    /**
     * @brief RenderGraph内で完結する一時テクスチャ（エイリアシング対象）を作成する
     */
    TransientResourceHandle CreateTransientResource(const std::string& name, const D3D12_RESOURCE_DESC& desc, const D3D12_CLEAR_VALUE* clearValue = nullptr) {
        TransientResourceHandle handle = static_cast<TransientResourceHandle>(transientDescs_.size());
        TransientResourceDesc tDesc{};
        tDesc.desc = desc;
        if (clearValue) {
            tDesc.clearValue = *clearValue;
            tDesc.hasClearValue = true;
        } else {
            tDesc.hasClearValue = false;
        }
        tDesc.name = name;
        transientDescs_.push_back(tDesc);
        return handle;
    }

    /**
     * @brief 一時テクスチャのステート要求
     */
    void RequireTransientState(TransientResourceHandle handle, D3D12_RESOURCE_STATES state) {
        if (handle != kInvalidHandle) {
            transientUsages_.push_back({ handle, state, currentPassIndex_ });
        }
    }

    const std::vector<ResourceUsage>& GetUsages() const { return usages_; }
    const std::vector<TransientResourceDesc>& GetTransientDescs() const { return transientDescs_; }
    const std::vector<TransientResourceUsage>& GetTransientUsages() const { return transientUsages_; }

    void Clear() { 
        usages_.clear(); 
        transientDescs_.clear();
        transientUsages_.clear();
        currentPassIndex_ = 0;
    }

private:
    std::vector<ResourceUsage> usages_;
    std::vector<TransientResourceDesc> transientDescs_;
    std::vector<TransientResourceUsage> transientUsages_;
    size_t currentPassIndex_ = 0;
};
