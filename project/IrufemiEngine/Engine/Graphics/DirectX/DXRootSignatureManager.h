#pragma once

#include <d3d12.h>
#include <wrl.h>
#include "RootSignatureConfig.h"

class Log;

/**
 * @class DXRootSignatureManager
 * @brief DirectX12のルートシグネチャ生成と管理を専門に行うクラス
 */
class DXRootSignatureManager {
public:
    DXRootSignatureManager() = default;
    ~DXRootSignatureManager() = default;

    /**
     * @brief 初期化処理。各種ルートシグネチャを生成します。
     * @param[in] device D3D12デバイス
     * @param[in] log ログ出力用インスタンス
     */
    void Initialize(ID3D12Device* device, Log* log);

    /**
     * @brief 解放処理
     */
    void Finalize();

    /**
     * @brief 描画用のルートシグネチャを取得
     */
    ID3D12RootSignature* GetGraphicsRootSignature() const { return graphicsRootSignature_.Get(); }

    /**
     * @brief コンピュートシェーダ用のルートシグネチャを取得
     */
    ID3D12RootSignature* GetComputeRootSignature() const { return computeRootSignature_.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D12RootSignature> graphicsRootSignature_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> computeRootSignature_ = nullptr;
};
