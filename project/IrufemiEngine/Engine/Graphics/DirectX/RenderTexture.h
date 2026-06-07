#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include <memory>
#include <string>
#include "../../Core/Math/Vector4.h"

class DirectXCommon;
class DrawManager;
class Camera;

class RenderTexture {
public:
    RenderTexture() = default;
    ~RenderTexture();

    void Initialize(DirectXCommon* dxCommon, uint32_t width, uint32_t height, DXGI_FORMAT format, const Vector4& clearColor);
    void InitializeFromResource(DirectXCommon* dxCommon, ID3D12Resource* resource, DXGI_FORMAT format);
    
    // スプライトの初期化 (廃止予定だが、互換性のために残すか?)
    // 今回は全画面コピーに移行するため、基本的には不要
    // void InitializeSprite(Camera* camera);

    // 現在のターゲットに対して自身を描画
    void Draw(DrawManager* drawManager, ID3D12PipelineState* pso = nullptr, D3D12_GPU_VIRTUAL_ADDRESS cbvAddress = 0, D3D12_GPU_DESCRIPTOR_HANDLE depthSrvHandle = { 0 });

    ID3D12Resource* GetResource() const { return resource_.Get(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetRtvHandle() const { return rtvHandle_; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU() const { return srvHandleGPU_; }
    
    uint32_t GetWidth() const { return width_; }
    uint32_t GetHeight() const { return height_; }
    DXGI_FORMAT GetFormat() const { return format_; }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_{};
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU_{};
    
    uint32_t rtvIndex_ = 0xFFFFFFFF;
    uint32_t srvIndex_ = 0xFFFFFFFF;

    uint32_t width_ = 0;
    uint32_t height_ = 0;
    DXGI_FORMAT format_ = DXGI_FORMAT_UNKNOWN;
    DirectXCommon* dxCommon_ = nullptr;
};
