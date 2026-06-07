#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <cstdint>
#include <array>
#include <wrl.h>
#include "../Graphics/Data/TransformationMatrix.h"
#include "../Graphics/Data/LightCommonData.h"
#include "../Graphics/Data/PointLight.h"
#include "../Graphics/Data/SpotLight.h"
#include "../Graphics/Data/AreaLight.h"
#include "../Graphics/Data/DirectionalLight.h"
#include "../Graphics/Data/SceneGPUStructs.h"
#include "../Graphics/DirectX/RenderTexture.h"
#include "../Graphics/DirectX/DirectXCommon.h" // kMaxFramesInFlight のために追加
#include "../Graphics/DirectX/RootSignatureConfig.h"
#include "../Core/Math/Vector4.h"
#include <vector>
#include <memory>
#include "../Graphics/Compute/IComputeTask.h"
#include "../Graphics/Data/RenderPackets.h"

class ShadowMap;

// 前方宣言
class TextureManager;
class DirectXCommon;
class BaseParticle;
class BaseGPU_Particle;
class Primitive3DObject;
class PrimitiveRegion;
struct GpuMesh;
struct ManagedModel;
class Line2DClass;
class Line3DClass;
class Line3DRegion;
class Skybox;
struct SkinCluster;
struct GpuMaterial;

// 構造体を前方宣言

// 描画のCommandListを積む順番
// Viewport → RootSignature → Pipeline → Topology → Buffers → CBV → SRV → Draw

/**
 * @class DrawManager
 * @brief 描画コマンドの発行とパイプライン管理を担うクラス
 * @details 各レンダラーからの描画リクエストを受け取り、適切な順序でコマンドリストに積みます。
 *          ライト情報の管理や、RenderTexture を用いたポストプロセス実行の制御も行います。
 */
class DrawManager {
private:
public:
private:
    // --- Render Queues ---
    std::vector<RenderPackets::Standard3DPacket> standard3DQueue_;
    std::vector<RenderPackets::Standard3DPacket> ui3DQueue_;
    std::vector<RenderPackets::Standard3DPacket> selectionMaskQueue_;
    std::vector<RenderPackets::SpritePacket> selectionMaskQueue2D_;
    std::vector<RenderPackets::SpritePacket> spriteQueue_;

    std::vector<RenderPackets::LinePacket> lineQueue_;
    std::vector<RenderPackets::GPUParticlePacket> gpuParticleQueue_;
    std::vector<RenderPackets::VoxelParticlePacket> voxelParticleQueue_;
    std::vector<RenderPackets::SkyboxPacket> skyboxQueue_;
    std::vector<RenderPackets::PrimitiveRegionPacket> primitiveRegionQueue_;
    std::vector<RenderPackets::ModelRegionPacket> modelRegionQueue_;
    std::vector<std::function<void()>> postRenderQueue_;
    
    // 最前面UI描画用キュー (PostProcess適用後のバックバッファに直接描画)
    std::vector<RenderPackets::SpritePacket> topMostSpriteQueue_;
    std::vector<RenderPackets::SpritePacket> textQueue_;
    std::vector<RenderPackets::SpritePacket> topMostTextQueue_;

    // レンダーグラフ
    std::unique_ptr<class RenderGraph> renderGraph_;

public:
    // --- Queue Getters for RenderPasses ---
    const std::vector<RenderPackets::Standard3DPacket>& GetStandard3DQueue() const { return standard3DQueue_; }
    const std::vector<RenderPackets::Standard3DPacket>& GetUI3DQueue() const { return ui3DQueue_; }
    const std::vector<RenderPackets::Standard3DPacket>& GetSelectionMaskQueue() const { return selectionMaskQueue_; }
    const std::vector<RenderPackets::SpritePacket>& GetSelectionMaskQueue2D() const { return selectionMaskQueue2D_; }
    const std::vector<RenderPackets::SpritePacket>& GetSpriteQueue() const { return spriteQueue_; }

    const std::vector<RenderPackets::LinePacket>& GetLineQueue() const { return lineQueue_; }
    const std::vector<RenderPackets::GPUParticlePacket>& GetGPUParticleQueue() const { return gpuParticleQueue_; }
    const std::vector<RenderPackets::VoxelParticlePacket>& GetVoxelParticleQueue() const { return voxelParticleQueue_; }
    const std::vector<RenderPackets::SkyboxPacket>& GetSkyboxQueue() const { return skyboxQueue_; }
    const std::vector<RenderPackets::PrimitiveRegionPacket>& GetPrimitiveRegionQueue() const { return primitiveRegionQueue_; }
    const std::vector<RenderPackets::ModelRegionPacket>& GetModelRegionQueue() const { return modelRegionQueue_; }
    const std::vector<std::function<void()>>& GetPostRenderQueue() const { return postRenderQueue_; }
    const std::vector<RenderPackets::SpritePacket>& GetTopMostSpriteQueue() const { return topMostSpriteQueue_; }
    
    // --- Text Queues ---
    const std::vector<RenderPackets::SpritePacket>& GetTextQueue() const { return textQueue_; }
    const std::vector<RenderPackets::SpritePacket>& GetTopMostTextQueue() const { return topMostTextQueue_; }

    // --- Execute Queues ---
    void ExecuteRenderQueues(class IrufemiEngine* engine);
    void ExecuteTopMostQueues(class IrufemiEngine* engine); // 最前面UIの描画キューを消化する
    void ClearRenderQueues();
    void ClearAllQueues() {
        ClearRenderQueues();
        computeTasks_.clear();
    }

    DirectXCommon* dxCommon_ = nullptr;
    ID3D12GraphicsCommandList* commandList_ = nullptr; // コマンドリストをキャッシュ

    std::vector<IComputeTask*> computeTasks_;

    // 各フレームごとの動的リソース
    struct FrameResource {
        Microsoft::WRL::ComPtr<ID3D12Resource> frameResource;
        Microsoft::WRL::ComPtr<ID3D12Resource> pointLightResource;
        Microsoft::WRL::ComPtr<ID3D12Resource> spotLightResource;
        Microsoft::WRL::ComPtr<ID3D12Resource> areaLightResource;

        PerFrameData* perFrameData = nullptr;
        LightCommonData* lightCommonData = nullptr;

        D3D12_GPU_DESCRIPTOR_HANDLE lightSrvHandle{};
        uint32_t lightSrvBaseIndex = 0xFFFFFFFFu;
        
        // カメラやライト共通情報を格納するデータ
        struct FrameData {
            D3D12_GPU_VIRTUAL_ADDRESS camera;
            D3D12_GPU_VIRTUAL_ADDRESS lightCommon; // register b1
        } frameData{};
    };
    std::array<FrameResource, kMaxFramesInFlight> frameResources_;

    D3D12_GPU_DESCRIPTOR_HANDLE environmentMapHandle_{}; // 環境マップ用SRVハンドル

    // シャドウマップ・レンダーターゲット関連
    std::array<std::unique_ptr<ShadowMap>, kMaxFramesInFlight> shadowMaps_;
    bool isShadowPass_ = false;
    class RenderTexture* currentRenderTexture_ = nullptr;

public: //メンバ関数

    /** @name 初期化・終了処理 */
    ///@{
    DrawManager();
    ~DrawManager();

    void Initialize(DirectXCommon* dx);
    void Finalize();
    void OnResize(int32_t width, int32_t height);
    
    /**
     * @brief RenderGraphにリソースの初期ステートを登録する（リサイズ時用）
     */
    void RegisterResourceState(ID3D12Resource* resource, D3D12_RESOURCE_STATES state);
    ///@}

    /** @name パイプライン・描画フロー制御 */
    ///@{
    /**
     * @brief パイプラインステートをバインドする
     * @param[in] pso バインドするパイプラインステート
     */
    void BindPSO(ID3D12PipelineState* pso);
    void BeginShadowPass();
    void EndShadowPass();
    bool IsShadowPass() const { return isShadowPass_; }

    /**
     * @brief フレームの描画開始処理
     * @details レンダーターゲットのクリアとビューポートの設定を行います。
     */
    void PreDraw(
        std::array<float, 4> clearColor = { 0.1f, 0.25f, 0.5f, 1.0f },
        float clearDepth = 1.0f,
        uint8_t clearStencil = 0
    );

    /**
     * @brief フレームの描画終了処理
     * @details リソースバリアの変更とコマンドリストのクローズ準備を行います。
     */
    void PostDraw();

    /**
     * @brief フレーム共通のルートパラメータをバインドする
     * @details カメラ、ライト、各種管理用定数バッファを一括でレジスタに設定します。
     */
    void BindCommonParameters();

    /** @name Computeタスク管理 */
    ///@{
    /**
     * @brief 今フレームで実行すべきComputeタスクを登録する
     * @param task 登録するタスク（GPUParticleSystem等）
     */
    void RegisterComputeTask(IComputeTask* task) {
        computeTasks_.push_back(task);
    }
    
    /**
     * @brief 登録された全Computeタスクを一括実行し、リストをクリアする
     */
    void ExecuteComputePasses();

    // カスタム描画コールバック用キュー
    void SubmitPostRender(std::function<void()> drawFunc) {
        postRenderQueue_.push_back(drawFunc);
    }
    ///@}
    ///@}

    /** @name レンダーターゲット・ポストプロセス操作 */
    ///@{
    /**
     * @brief 指定した RenderTexture への描画を開始する
     * @param[in] rt 出力先の RenderTexture
     * @param[in] clearColor 背景クリア色
     */
    void BeginRenderTexture(class RenderTexture* rt, const struct Vector4& clearColor);

    /**
     * @brief RenderTexture への描画を終了する
     * @param[in] rt 描画していた RenderTexture
     */
    void EndRenderTexture(class RenderTexture* rt);

    /**
     * @brief レンダーターゲットをバックバッファ（画面）に戻す
     * @param[in] useDepth 深度バッファを使用するかどうか
     */
    void SetRenderTargetToBackBuffer(bool useDepth = true);

    /**
     * @brief RenderTexture を全画面に描画（ポストプロセス用）
     * @param[in] renderTexture 描画元のテクスチャ
     * @param[in] pso 使用するポストプロセス用パイプラインステート
     * @param[in] cbvAddress 追加の定数バッファアドレス（オプション）
     * @param[in] depthSrvHandle 深度テクスチャのハンドル（オプション）
     */
    void DrawRenderTexture(class RenderTexture* renderTexture, ID3D12PipelineState* pso = nullptr, D3D12_GPU_VIRTUAL_ADDRESS cbvAddress = 0, D3D12_GPU_DESCRIPTOR_HANDLE depthSrvHandle = { 0 });
    ///@}

    /** @name 共通データ設定 */
    ///@{
    /**
     * @brief フレーム単位の共通データを定数バッファに書き込む
     */
    void SetFrameData(const CameraForGPU& camera, float time, float deltaTime, const DirectionalLight& light, const std::vector<PointLight*>& pointLights, const std::vector<SpotLight*>& spotLights, const std::vector<AreaLight*>& areaLights);

    /**
     * @brief キャッシュされたフレームデータを用いて現在のフレームバッファを同期する（ポーズなどでSetFrameDataが呼ばれなかった時用）
     */
    void SyncCachedFrameData();
    
private:
    PerFrameData cachedPerFrame_{};
    DirectionalLight cachedDirectionalLight_{};
    std::vector<PointLight> cachedPointLights_;
    std::vector<SpotLight> cachedSpotLights_;
    std::vector<AreaLight> cachedAreaLights_;
    
    // シャドウマップのカスタムパラメータ
    Vector3 shadowTargetPos_{ 0, 0, 0 };
    float shadowOrthoSize_{ 128.0f };
    bool useCustomShadowParams_{ false };

    TextureManager* textureManager_ = nullptr; ///< 環境マップフォールバック等に使用するテクスチャマネージャー
    
public:

    /**
     * @brief シャドウマップの注視点とサイズをカスタマイズする
     * @param targetPos 注視点（通常はプレイヤーとボスの中心）
     * @param orthoSize 描画範囲（通常はプレイヤーとボスの距離に基づく）
     */
    void SetShadowParameters(const Vector3& targetPos, float orthoSize) {
        shadowTargetPos_ = targetPos;
        shadowOrthoSize_ = orthoSize;
        useCustomShadowParams_ = true;
    }

    /**
     * @brief カスタムシャドウパラメータをリセットし、デフォルト（カメラ追従）に戻す
     */
    void ResetShadowParameters() {
        useCustomShadowParams_ = false;
    }

    /**
     * @brief テクスチャマネージャーのポインタを設定する
     * @param[in] textureManager 依存注入するテクスチャマネージャーへのポインタ
     */
    void SetTextureManager(TextureManager* textureManager) { textureManager_ = textureManager; }

    /**
     * @brief 環境マップを設定する
     * @param[in] envMapHandle 環境マップテクスチャのGPUハンドル
     */
    void SetEnvironmentMap(D3D12_GPU_DESCRIPTOR_HANDLE envMapHandle);
    D3D12_GPU_DESCRIPTOR_HANDLE GetEnvironmentMap() const { return environmentMapHandle_; }
    ///@}

    /** @name 各種オブジェクト描画メソッド */
    ///@{

    /**
     * @brief 矩形領域（Region）の描画
     */
    void SubmitModelRegion(const RenderPackets::ModelRegionPacket& packet);
    void DrawModelRegion(const RenderPackets::ModelRegionPacket& packet);

    /**
     * @brief 汎用的な領域描画（頂点バッファ・インデックスバッファ直接指定）
     */
    void SubmitPrimitiveRegion(const RenderPackets::PrimitiveRegionPacket& packet);
    void DrawPrimitiveRegion(const RenderPackets::PrimitiveRegionPacket& packet);

    /**
     * @brief インスタンス化された線の描画
     */
    void SubmitLineInstanced(const class LineResource* resource, const D3D12_GPU_DESCRIPTOR_HANDLE& instancingSrvHandleGPU, const UINT& instanceCount);
    void DrawLineInstanced(const RenderPackets::LinePacket& packet);

    /**
     * @brief 標準的な3Dオブジェクトの描画 (Object3d.hlsl)
     * @param vertexBufferViewOverride スキニング等でVBVを差し替えたい場合に指定
     */
    void SubmitStandard3D(const class Object3DResource* resource, const D3D12_VERTEX_BUFFER_VIEW* vertexBufferViewOverride = nullptr, bool castShadows = true, ID3D12Resource* vertexBufferResourceOverride = nullptr);
    void SubmitUI3D(const class Object3DResource* resource, const D3D12_VERTEX_BUFFER_VIEW* vertexBufferViewOverride = nullptr);
    void SubmitOutlineMask(const class Object3DResource* resource, const D3D12_VERTEX_BUFFER_VIEW* vertexBufferViewOverride = nullptr);
    void SubmitTextOutlineMask(const class Object2DResource* resource);
    void DrawStandard3D(const RenderPackets::Standard3DPacket& packet);

    /**
     * @brief 2Dオブジェクト（スプライト等）の標準描画 (Sprite.hlsl)
     */
    void SubmitSprite(const class Object2DResource* resource);
    void SubmitTopMostSprite(const class Object2DResource* resource); // 最前面UI描画用
    void DrawSprite(const RenderPackets::SpritePacket& packet);

    // --- Text ---
    void SubmitText(const class Object2DResource* resource);
    void SubmitTopMostText(const class Object2DResource* resource);
    void DrawText(const RenderPackets::SpritePacket& packet);

    /**
     * @brief スカイボックスの描画
     */
    void SubmitSkybox(const D3D12_VERTEX_BUFFER_VIEW& vertexBufferView, const D3D12_INDEX_BUFFER_VIEW& indexBufferView, D3D12_GPU_VIRTUAL_ADDRESS materialAddress, D3D12_GPU_VIRTUAL_ADDRESS transformationAddress, D3D12_GPU_DESCRIPTOR_HANDLE textureHandle, const UINT& indexCount);
    void DrawSkybox(const RenderPackets::SkyboxPacket& packet);

    /**
     * @brief GPUパーティクルのインスタンス描画 (GPUParticle.hlsl)
     */
    void SubmitGPUParticle(const RenderPackets::GPUParticlePacket& packet);
    void DrawGPUParticle(const RenderPackets::GPUParticlePacket& packet);
    
    // VoxelParticle 用の描画 (VoxelParticle.hlsl)
    void SubmitVoxelParticle(
        uint32_t instanceCount,
        const D3D12_VERTEX_BUFFER_VIEW& vbv,
        const D3D12_INDEX_BUFFER_VIEW& ibv,
        uint32_t indexCount,
        D3D12_GPU_VIRTUAL_ADDRESS emitterAddress,
        D3D12_GPU_DESCRIPTOR_HANDLE particleDataHandle,
        ID3D12Resource* particleResource,
        ID3D12PipelineState* drawPSO
    );
    void DrawVoxelParticle(const RenderPackets::VoxelParticlePacket& packet);
    ///@}

    /** @name コンピュートシェーダ（GPGPU）操作 */
    ///@{
    /**
     * @brief スキニング計算（Compute Shader）の実行
     */
    void DispatchSkinning(const SkinCluster& skinCluster, const ManagedModel* model, uint32_t numVertices);

    /**
     * @brief UAVバリアの実行（リソース競合の解決）
     */
    void ExecuteUAVBarrier(ID3D12Resource* resource = nullptr);
    ///@}

    /** @name 状態取得・ユーティリティ */
    ///@{
    PerFrameData* GetPerFrameData() const { return frameResources_[dxCommon_->GetFrameIndex()].perFrameData; }
    class RenderGraph* GetRenderGraph() const { return renderGraph_.get(); }
    DirectXCommon* GetDxCommon() const { return dxCommon_; }
    ShadowMap* GetShadowMap() const { return shadowMaps_[dxCommon_->GetFrameIndex()].get(); }
    ///@}
};