#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>  
#include <d3d12.h>
#include <wrl.h>
#include "Texture.h"
#include "../../../externals/DirectXTex/DirectXTex.h"
#include "../../Engine/Core/System/ThreadPool.h"
#include "../../Engine/Core/System/TaskGroup.h"
#include <atomic>
#include <future>
#include <type_traits>
#include <functional>

// 前方宣言
namespace DirectX {
    class ScratchImage;
}

class DirectXCommon;

/**
 * @class TextureManager
 * @brief テクスチャのロードと管理を一括して行うマネージャクラス
 * @details テクスチャの重複ロードを防ぐためのキャッシュ機構を持ち、IDによる指定でSRVハンドルを提供します。
 */
class TextureManager {
public:
    /**
     * @brief コンストラクタ
     */
    TextureManager() = default;

    /**
     * @brief デストラクタ
     */
    ~TextureManager() = default;

    /**
     * @brief 初期化
     * @param[in] dxCommon DirectX 12 基礎クラスのポインタ
     */
    void Initialize(DirectXCommon* dxCommon);

    /**
     * @brief 指定フォルダ内のすべての画像をロードする
     * @param[in] folderPath ロード対象のフォルダパス
     */
    void LoadAllFromFolder(const std::string& folderPath);

    /**
     * @brief テクスチャ名からGPU側のSRVハンドルを取得
     * @details 未ロードの場合はロードを試みます。
     * @param[in] name ファイルパスまたは識別名
     * @return GPU側のSRVハンドル
     */
    D3D12_GPU_DESCRIPTOR_HANDLE GetTextureHandle(const std::string& name) const;

    /**
     * @brief テクスチャ名からCPU側の画像データを取得
     * @param[in] name ファイルパスまたは識別名
     * @return ScratchImageへのポインタ
     */
    const DirectX::ScratchImage* GetScratchImage(const std::string& name) const;

    /**
     * @brief ロード済みのテクスチャ名一覧を取得
     */
    std::vector<std::string> GetTextureNames() const;

    /**
     * @brief デバッグ表示用にロード済みのテクスチャ名一覧を取得（キューブマップ除外、ソート済み）
     */
    std::vector<std::string> GetTextureNamesForDebug() const;

    /**
     * @brief デバッグ表示用にロード済みのキューブマップ名一覧を取得（キューブマップのみ、ソート済み）
     */
    std::vector<std::string> GetCubeMapNamesForDebug() const;

    /**
     * @brief フォールバック用のダミー白テクスチャを生成する
     */
    void CreateWhiteDummyTexture();

    /**
     * @brief フォールバック用のダミー白CubeMapを生成する
     */
    void CreateWhiteCubeMap();

    /**
     * @brief テクスチャのピクセルサイズを取得
     * @param[in] name 識別名
     * @param[out] outWidth 幅の出力先
     * @param[out] outHeight 高さの出力先
     * @return 取得成功なら true
     */
    bool GetTextureSize(const std::string& name, uint32_t& outWidth, uint32_t& outHeight) const;

    /**
     * @brief 白テクスチャのGPUハンドルを取得
     */
    D3D12_GPU_DESCRIPTOR_HANDLE GetWhiteTextureHandle() const { return whiteTextureHandle_; }

    /**
     * @brief 白CubeMapテクスチャのGPUハンドルを取得
     */
    D3D12_GPU_DESCRIPTOR_HANDLE GetWhiteCubeMapHandle() const { return whiteCubeMapHandle_; }

    /**
     * @brief テクスチャのロード状態を取得する
     * @param[in] name ファイルパスまたは識別名
     * @return ロード状態。存在しない場合は Failed を返す
     */
    Texture::LoadingStatus GetTextureStatus(const std::string& name) const;

    /**
     * @brief キューブマップかどうかを取得
     */
    bool IsCubeMap(const std::string& name) const;

    /**
     * @brief すべてのロードタスク（背景タスクを含む）が完了しているかを取得
     */
    bool IsAllLoaded() const { return taskGroup_->IsAllDone(); }

    /**
     * @brief 非同期タスクの実行（シーンの状態による自動判定）
     */
    template <class F, class... Args>
    auto EnqueueTask(F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result_t<F, Args...>> {
        bool isCritical = IsCurrentSceneInitializing();
        return EnqueueTask(isCritical, std::forward<F>(f), std::forward<Args>(args)...);
    }

    /**
     * @brief 優先度を指定して非同期タスクを実行
     */
    template <class F, class... Args>
    auto EnqueueTask(bool isCritical, F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result_t<F, Args...>> {
        auto &group = isCritical ? taskGroup_ : backgroundTaskGroup_;
        return threadPool_->Enqueue(group, std::forward<F>(f), std::forward<Args>(args)...);
    }

    /**
     * @brief 白テクスチャのリソースを取得
     */
    ID3D12Resource* GetWhiteTextureResource() const { return whiteTextureResource_.Get(); }

private:
    /**
     * @brief 現在のシーンが初期化中かどうかを判定する
     */
    bool IsCurrentSceneInitializing() const;
    DirectXCommon* dxCommon_ = nullptr;

    // key: ファイルパス(または識別名)、value: Texture オブジェクト
    mutable std::unordered_map<std::string, std::shared_ptr<Texture>> textures_;
    mutable std::mutex mutex_;

    std::unique_ptr<ThreadPool> threadPool_;
    std::shared_ptr<TaskGroup> taskGroup_;           ///< 重要タスク用
    std::shared_ptr<TaskGroup> backgroundTaskGroup_; ///< バックグラウンド用

    // フォールバック白テクスチャ
    Microsoft::WRL::ComPtr<ID3D12Resource> whiteTextureResource_;
    D3D12_GPU_DESCRIPTOR_HANDLE whiteTextureHandle_{ 0 };

    // フォールバック白CubeMap
    Microsoft::WRL::ComPtr<ID3D12Resource> whiteCubeMapResource_;
    D3D12_GPU_DESCRIPTOR_HANDLE whiteCubeMapHandle_{ 0 };

};