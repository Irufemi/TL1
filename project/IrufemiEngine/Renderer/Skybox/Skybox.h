#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include <string>
#include "Engine/Graphics/Data/Material.h"
#include "../../Engine/Graphics/Data/VertexData.h"
#include "../Core/MultiBufferSyncState.h"
#include "Engine/Core/Math/Transform.h"
#include "Engine/Core/Math/Matrix4x4.h"
#include "Engine/Core/Math/Vector4.h"
#include <vector>
#include <array>
#include "Engine/Graphics/DirectX/DirectXCommon.h"
#include "../../Engine/Graphics/DirectX/ConstantBuffer.h"

// 前方宣言
class Camera;
class IrufemiEngine;

#include "../Core/IRenderable.h"

/**
 * @class Skybox
 * @brief スカイボックスの描画を管理するクラス
 */
class Skybox : public IRenderable, public MultiBufferSyncState
{
public:
    // デフォルトのテクスチャパス
    static inline const std::string kDefaultTexturePath = "resources/rostock_laage_airport_4k.dds";

public: // メンバ関数
    // コンストラクタ
    Skybox();
    // デストラクタ
    ~Skybox();
    // 初期化
    void Initialize(const std::string& textureName = kDefaultTexturePath);
    // 更新
    void Update();
    void SyncBeforeDraw() override;
    void Draw() override;
    // デバッグ
    void Debug();
public: // メンバ関数(セッター/ゲッター)
    // engineセッター
    static void SetEngine(IrufemiEngine* engine) { engine_ = engine; }
    // ID3D12Resource関連ゲッター
    const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const { return vertexBufferView_; }
    const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() const { return indexBufferView_; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetTextureHandle() const { return textureHandle_; }
    // indexのサイズ取得
    UINT GetIndexSize() const { return static_cast<UINT>(indexDataList_.size()); }
private: // メンバ関数(内部ヘルパ)
    // ID3D12Resourceの生成
    void CreateResource();
    // ID3D12ResourceのMap
    void MapResource();
    // Id3D12ResourceのUnMap
    void UnMapResource();


private: // メンバ変数(resource)
    /// vertex
    std::vector<VertexData> vertexDataList_{};
    VertexData* vertexData_ = nullptr;
    //頂点データバッファ
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_ = nullptr;

    /// index
    std::vector<uint32_t> indexDataList_{};
    uint32_t* indexData_ = nullptr;
    //頂点インデックスバッファ
    D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_ = nullptr;

    /// Transform
    // transform(scale,rotate,translate)
    Transform transform_ = {
        {500.0f,500.0f,500.0f},   //scale
        {0.0f,0.0f,0.0f},   //rotate
        {0.0f,0.0f,0.0f}    //translate
    };
    struct SkyboxTransformationMatrix {
        Matrix4x4 WVP;
        Matrix4x4 World;
        Matrix4x4 WorldInverseTranspose;
    };
    SkyboxTransformationMatrix transformationMatrix_{};
    ConstantBuffer<SkyboxTransformationMatrix> transformationBuffer_;

    // Material
    struct SkyboxMaterial {
        Vector4 color;
        float intensity;
        float padding[3];
    };
    ConstantBuffer<SkyboxMaterial> materialBuffer_;

    // texture
    D3D12_GPU_DESCRIPTOR_HANDLE textureHandle_ = {};
    int selectedTextureIndex_ = 0;

    // カメラ(ポインタ参照)

    // engine(ポインタ参照)
    static IrufemiEngine* engine_;

    // 行列更新の最適化用
    bool isDirty_ = true;
    Matrix4x4 lastViewMatrix_ = {};
    Matrix4x4 lastProjectionMatrix_ = {};

};


