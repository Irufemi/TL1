#include "ShaderManager.h"
#include <cassert>

/**
 * @brief 初期化
 */
void ShaderManager::Initialize() {
    compiler_ = std::make_unique<ShaderCompiler>();
    compiler_->Initialize();
}

/**
 * @brief シェーダーを取得またはコンパイルする
 */
Microsoft::WRL::ComPtr<IDxcBlob> ShaderManager::GetOrCompile(
    const std::wstring& filePath,
    const ShaderCompileOptions& options,
    const wchar_t* profileOverride
) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 1. キャッシュキーの構築
    ShaderKey key;
    key.filePath = filePath;
    key.entryPoint = options.entryPoint;
    key.macros = options.macros;

    // 2. キャッシュの検索
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        return it->second;
    }

    // 3. コンパイル
    std::wstring profile = profileOverride ? profileOverride : ShaderCompiler::GetInferredProfile(filePath);
    auto blob = compiler_->Compile(filePath, profile.c_str(), options);
    
    // 4. キャッシュに登録
    if (blob) {
        cache_[key] = blob;
    }

    return blob;
}

/**
 * @brief キャッシュをクリアする
 */
void ShaderManager::ClearCache() {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.clear();
}
