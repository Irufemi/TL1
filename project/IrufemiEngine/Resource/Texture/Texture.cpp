#include "Texture.h"
#include "../../../externals/DirectXTex/DirectXTex.h"
#include "../../../externals/DirectXTex/d3dx12.h"
#include "../../Engine/Graphics/DirectX/DirectXCommon.h"
#include "../../Engine/Graphics/DirectX/DescriptorPool.h"
#include <cassert>

DirectXCommon* Texture::dxCommon_ = nullptr;
uint32_t Texture::index_ = 0;
DescriptorPool* Texture::s_srvPool_ = nullptr;
ID3D12Resource* Texture::s_whiteResource_ = nullptr;

Texture::Texture() {
    // コンストラクタでSRV枠を先に確保して、暫定的に白テクスチャを割り当てておく
    if (s_srvPool_) {
        srvIndex_ = s_srvPool_->Allocate();
        if (srvIndex_ != DescriptorPool::kInvalid) {
            textureSrvHandleCPU_ = s_srvPool_->GetCPUHandle(srvIndex_);
            textureSrvHandleGPU_ = s_srvPool_->GetGPUHandle(srvIndex_);

            // とりあえず白テクスチャでSRVを作っておく(セーフティ)
            if (s_whiteResource_ && dxCommon_) {
                D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
                srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
                srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                srvDesc.Texture2D.MipLevels = 1;
                dxCommon_->GetDevice()->CreateShaderResourceView(s_whiteResource_, &srvDesc, textureSrvHandleCPU_);
            }
        }
    }
}

Texture::~Texture() {
    if (s_srvPool_ && srvIndex_ != UINT32_MAX && dxCommon_) {
        // GPU が参照し終わるまで遅延解放
        s_srvPool_->FreeAfterFence(srvIndex_, dxCommon_->GetFenceValue());
        srvIndex_ = UINT32_MAX;
    }
}

void Texture::Initialize(const std::string& filePath) {
    this->filePath_ = filePath;
    status_.store(LoadingStatus::Loading);

    try {
        mipImages_ = dxCommon_->LoadTexture(filePath_);
        const DirectX::TexMetadata& metadata = mipImages_.GetMetadata();
        width_ = static_cast<uint32_t>(metadata.width);
        height_ = static_cast<uint32_t>(metadata.height);

        textureResource_ = dxCommon_->CreateTextureResource(metadata);
        intermediateResource_ = dxCommon_->UploadTextureData(textureResource_.Get(), mipImages_);

        // --- アップロード完了後に中間リソースを解放するように登録 ---
        dxCommon_->ReleaseAfterFence(intermediateResource_);
        intermediateResource_ = nullptr;

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = metadata.format;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        isCubemap_ = metadata.IsCubemap();
        if (isCubemap_) {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
            srvDesc.TextureCube.MostDetailedMip = 0;
            srvDesc.TextureCube.MipLevels = UINT_MAX;
            srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
        }
        else {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);
        }

        // SRV上書き (データ競合を防ぐため、メインスレッドの安全なタイミングで更新するようキューに積む)
        if (textureSrvHandleCPU_.ptr != 0) {
            dxCommon_->EnqueueSRVUpdate(textureResource_, srvDesc, textureSrvHandleCPU_);
        }

        status_.store(LoadingStatus::Loaded);
    }
    catch (...) {
        status_.store(LoadingStatus::Failed);
        // 失敗してもSRV自体は白テクスチャを指したままなので描画上は安全
    }
}

void Texture::InitializeFromMemory(const std::string& name, const uint32_t* pixels, uint32_t width, uint32_t height) {
    this->filePath_ = name;
    this->width_ = width;
    this->height_ = height;
    status_.store(LoadingStatus::Loading);

    try {
        // sRGB フォーマットで初期化
        HRESULT hr = mipImages_.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, width, height, 1, 1);
        assert(SUCCEEDED(hr));

        // ピクセルデータのコピー
        memcpy(mipImages_.GetImage(0, 0, 0)->pixels, pixels, width * height * sizeof(uint32_t));

        const DirectX::TexMetadata& metadata = mipImages_.GetMetadata();
        textureResource_ = dxCommon_->CreateTextureResource(metadata);
        intermediateResource_ = dxCommon_->UploadTextureData(textureResource_.Get(), mipImages_);

        dxCommon_->ReleaseAfterFence(intermediateResource_);
        intermediateResource_ = nullptr;

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = metadata.format;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        // SRV上書き
        if (textureSrvHandleCPU_.ptr != 0) {
            dxCommon_->EnqueueSRVUpdate(textureResource_, srvDesc, textureSrvHandleCPU_);
        }

        status_.store(LoadingStatus::Loaded);
    }
    catch (...) {
        status_.store(LoadingStatus::Failed);
    }
}

void Texture::InitializeCubeFromMemory(const std::string& name, const uint32_t* pixels, uint32_t width, uint32_t height) {
    this->filePath_ = name;
    this->width_ = width;
    this->height_ = height;
    status_.store(LoadingStatus::Loading);

    try {
        // CubeMap として初期化
        HRESULT hr = mipImages_.InitializeCube(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, width, height, 1, 1);
        assert(SUCCEEDED(hr));

        // 6面分のピクセルデータのコピー
        for (size_t i = 0; i < 6; ++i) {
            const DirectX::Image* img = mipImages_.GetImage(0, i, 0);
            memcpy(img->pixels, pixels + (i * width * height), width * height * sizeof(uint32_t));
        }

        const DirectX::TexMetadata& metadata = mipImages_.GetMetadata();
        textureResource_ = dxCommon_->CreateTextureResource(metadata);
        intermediateResource_ = dxCommon_->UploadTextureData(textureResource_.Get(), mipImages_);

        dxCommon_->ReleaseAfterFence(intermediateResource_);
        intermediateResource_ = nullptr;

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = metadata.format;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MostDetailedMip = 0;
        srvDesc.TextureCube.MipLevels = 1;
        srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

        // SRV上書き
        if (textureSrvHandleCPU_.ptr != 0) {
            dxCommon_->EnqueueSRVUpdate(textureResource_, srvDesc, textureSrvHandleCPU_);
        }

        isCubemap_ = true;
        status_.store(LoadingStatus::Loaded);
    }
    catch (...) {
        status_.store(LoadingStatus::Failed);
    }
}
