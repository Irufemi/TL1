#include <filesystem>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <thread>
#include <format>

#include "TextureManager.h"
#include "../../Engine/Graphics/DirectX/DescriptorPool.h"
#include "../../Engine/Graphics/DirectX/DirectXCommon.h"
#include "../../../externals/DirectXTex/DirectXTex.h"
#include "../../../externals/DirectXTex/d3dx12.h"
#include "../../Engine/IrufemiEngine.h"
#include "../../Framework/SceneManager.h"

static bool IsImageExtImpl(const std::string& extLower) {
    static const char* exts[] = { ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".dds" };
    for (auto* e : exts) {
        if (extLower == e) { return true; }
    }
    return false;
}

// Initialize: DirectXCommon を保存し、Texture にも渡す
void TextureManager::Initialize(DirectXCommon* dxCommon) {
    dxCommon_ = dxCommon;
    Texture::SetDirectXCommon(dxCommon_);

    // ThreadPoolの生成 (論理コア数分)
    if (!threadPool_) {
        threadPool_ = std::make_unique<ThreadPool>(std::thread::hardware_concurrency());
    }
    if (!taskGroup_) {
        taskGroup_ = std::make_shared<TaskGroup>();
    }
    if (!backgroundTaskGroup_) {
        backgroundTaskGroup_ = std::make_shared<TaskGroup>();
    }

    // フォールバック用の白テクスチャ生成と登録
    CreateWhiteDummyTexture();
    CreateWhiteCubeMap();
    if (whiteTextureResource_.Get()) {
        Texture::SetWhiteTextureResource(whiteTextureResource_.Get());
    }
}

// 指定フォルダ配下を走査してロード(キーはフルパス文字列)
void TextureManager::LoadAllFromFolder(const std::string& folderPath) {
    if (!std::filesystem::exists(folderPath)) { return; }

    for (auto& entry : std::filesystem::recursive_directory_iterator(folderPath)) {
        if (!entry.is_regular_file()) { continue; }
        auto p = entry.path();
        auto ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (!IsImageExtImpl(ext)) { continue; }

        const std::string key = p.generic_string();
        
        // 非同期ロード開始
        GetTextureHandle(key);
    }
}

// 取得(未ロードなら非同期ロード開始)
D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetTextureHandle(const std::string& name) const {
    // 既存キー検索
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = textures_.find(name);
        if (it != textures_.end()) {
            return it->second->GetTextureSrvHandleGPU();
        }
    }

    // 新規ロード
    auto tex = std::make_shared<Texture>();
    
    // メタデータ（サイズ）のみ同期的に取得して設定
    DirectX::TexMetadata metadata = dxCommon_->GetTextureMetadata(name);
    if (metadata.width > 0 && metadata.height > 0) {
        tex->SetSize(static_cast<uint32_t>(metadata.width), static_cast<uint32_t>(metadata.height));
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        // 二重登録防止
        auto it = textures_.find(name);
        if (it != textures_.end()) {
            return it->second->GetTextureSrvHandleGPU();
        }
        textures_.emplace(name, tex);
    }

    // 非同期タスクとして投入
    // constメソッド内で非同期タスクを開始するため、メンバへの直接アクセスに注意

    const_cast<TextureManager*>(this)->EnqueueTask([tex, name]() {
        tex->Initialize(name);
    });

    // Textureのコンストラクタで既に白テクスチャが割り当てられたハンドルが返る
    return tex->GetTextureSrvHandleGPU();
}

Texture::LoadingStatus TextureManager::GetTextureStatus(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = textures_.find(name);
    if (it == textures_.end()) {
        return Texture::LoadingStatus::Failed;
    }
    return it->second->GetStatus();
}

const DirectX::ScratchImage* TextureManager::GetScratchImage(const std::string& name) const
{
    // 既存キー検索
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = textures_.find(name);
        if (it != textures_.end()) {
            return it->second->GetScratchImage();
        }
    }

    // キャッシュになければロード (このメソッドは現在同期的に動くが、セーフティのためにハンドル経由の使用が推奨される)
    auto tex = std::make_shared<Texture>();
    tex->Initialize(name);
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = textures_.find(name);
        if (it != textures_.end()) {
            return it->second->GetScratchImage();
        }
        textures_.emplace(name, tex);
    }
    return tex->GetScratchImage();
}

std::vector<std::string> TextureManager::GetTextureNames() const {
    std::vector<std::string> keys;
    std::lock_guard<std::mutex> lock(mutex_);
    keys.reserve(textures_.size());
    for (auto& kv : textures_) keys.push_back(kv.first);
    return keys;
}

std::vector<std::string> TextureManager::GetTextureNamesForDebug() const {
    std::vector<std::string> keys;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        keys.reserve(textures_.size());
        for (auto& kv : textures_) {
            // キューブマップを除外
            if (!kv.second->IsCubemap() && kv.first != "whiteCubeMap") {
                keys.push_back(kv.first);
            }
        }
    }
    // アルファベット順にソート
    std::sort(keys.begin(), keys.end());
    return keys;
}

std::vector<std::string> TextureManager::GetCubeMapNamesForDebug() const {
    std::vector<std::string> keys;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        keys.reserve(textures_.size());
        for (auto& kv : textures_) {
            // キューブマップのみを抽出
            if (kv.second->IsCubemap() || kv.first == "whiteCubeMap") {
                keys.push_back(kv.first);
            }
        }
    }
    // アルファベット順にソート
    std::sort(keys.begin(), keys.end());
    return keys;
}

void TextureManager::CreateWhiteDummyTexture() {
    if (whiteTextureHandle_.ptr != 0) return;
    if (!dxCommon_) { return; }

    // 2x2 白テクスチャ
    uint32_t whitePixels[4] = { 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu };

    auto tex = std::make_shared<Texture>();
    tex->InitializeFromMemory("white", whitePixels, 2, 2);
    
    // バックアップ用リソースとしても保持
    whiteTextureResource_ = dxCommon_->CreateTextureResource(tex->GetScratchImage()->GetMetadata());
    dxCommon_->UploadTextureData(whiteTextureResource_, *tex->GetScratchImage());
    dxCommon_->ReleaseAfterFence(whiteTextureResource_);

    whiteTextureHandle_ = tex->GetTextureSrvHandleGPU();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        textures_.emplace("white", tex);
    }
}

void TextureManager::CreateWhiteCubeMap() {
    if (whiteCubeMapHandle_.ptr != 0) return;
    if (!dxCommon_) { return; }

    // 1x1 6面 白テクスチャ
    uint32_t whitePixels[6] = { 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu };

    auto tex = std::make_shared<Texture>();
    tex->InitializeCubeFromMemory("whiteCubeMap", whitePixels, 1, 1);

    // バックアップ用リソースとして保持
    whiteCubeMapResource_ = dxCommon_->CreateTextureResource(tex->GetScratchImage()->GetMetadata());
    dxCommon_->UploadTextureData(whiteCubeMapResource_, *tex->GetScratchImage());
    dxCommon_->ReleaseAfterFence(whiteCubeMapResource_);

    whiteCubeMapHandle_ = tex->GetTextureSrvHandleGPU();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        textures_.emplace("whiteCubeMap", tex);
    }
}

bool TextureManager::GetTextureSize(const std::string& name, uint32_t& outWidth, uint32_t& outHeight) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = textures_.find(name);
    if (it == textures_.end()) { return false; }
    outWidth = it->second->GetWidth();
    outHeight = it->second->GetHeight();
    return true;
}

bool TextureManager::IsCurrentSceneInitializing() const {
    if (!dxCommon_) return false;
    auto engine = dxCommon_->GetEngine();
    if (!engine) return false;
    auto sceneManager = engine->GetSceneManager();
    if (!sceneManager) return false;
    return sceneManager->IsInitializing();
}

bool TextureManager::IsCubeMap(const std::string& name) const {
    if (name == "whiteCubeMap") return true;
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = textures_.find(name);
    if (it != textures_.end()) {
        return it->second->IsCubemap();
    }
    return false;
}