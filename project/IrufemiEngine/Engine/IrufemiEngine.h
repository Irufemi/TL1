#pragma once

#include "Graphics/DirectX/DirectXCommon.h"
#include "Graphics/DirectX/D3DResourceLeakChecker.h"
#include "Platform/Input/InputManager.h"
#include "Platform/WindowsAPI/WinApp.h"
#include "Manager/DrawManager.h"
#include "Manager/DebugUI.h"
#include "Manager/EditorManager.h"
#include "../Resource/Texture/TextureManager.h"
#include "../Resource/Audio/AudioManager.h"
#include "../Resource/Model/ModelManager.h"
#include "../Resource/Model/AnimationManager.h"
#include "Core/Type/BlendMode.h"
#include "Core/Utility/Log.h"
#include "../Framework/SceneManager.h"
#include "../Framework/SceneTransition.h"
#include "../Framework/LoadingScreen.h"
#include "Core/Math/Vector4.h"
#include "Core/Math/Vector2.h"
#include "Core/Math/Matrix4x4.h"
#include "Graphics/DirectX/RenderTexture.h"
#include "Graphics/PostProcess/PostProcessManager.h"
#include "Graphics/DirectX/DynamicConstantBuffer.h"
#include <memory>
#include <Windows.h>
#include <d3d12.h>
#include <dxcapi.h>
#include <wrl/client.h>
#include <dxgi1_6.h>
#include <functional>
#include <string>
#include <array>
#include <chrono>
#include <algorithm>

#include "Graphics/Font/FontManager.h"

class SceneManager;
class DebugUI;
class VoxelParticleManager;
#include "Graphics/Camera/CameraManager.h"

/**
 * @class IrufemiEngine
 * @brief IrufemiEngine 全体を制御するメインクラス
 * @details エンジンの初期化、メインループ、終了処理を管理し、各マネージャへのアクセスを提供します。
 */
class IrufemiEngine {
public: // 内部型などは PostProcessManager.h へ移動しました。
    using PostProcessMode = ::PostProcessMode;
    using Mode = ::PostProcessMode; // 互換性のため
    
    using NoiseParams = PostProcessManager::NoiseParams;
    using VignetteParams = PostProcessManager::VignetteParams;
    using SmoothingParams = PostProcessManager::SmoothingParams;
    using GaussianParams = PostProcessManager::GaussianParams;
    using RadialBlurParams = PostProcessManager::RadialBlurParams;
    using OutlineParams = PostProcessManager::OutlineParams;
    using DissolveParams = PostProcessManager::DissolveParams;

public: // メンバ関数
    /**
     * @brief コンストラクタ
     */
    IrufemiEngine();

    /**
     * @brief デストラクタ
     */
    ~IrufemiEngine();

    /**
     * @brief メインループの実行
     * @details ウィンドウが閉じられるまで、Initialize から Finalize までのフローを制御します。
     */
    void Execute();

    /**
     * @brief エンジンの初期化
     * @param[in] title ウィンドウタイトル
     * @param[in] clientWidth 画面横幅 (デフォルト: 1280)
     * @param[in] clientHeight 画面縦幅 (デフォルト: 720)
     */
    void Initialize(const std::wstring& title, const int32_t& clientWidth = 1280, const int32_t& clientHeight = 720);
   
    /**
     * @brief エンジンの初期化（クリアカラー指定付き）
     * @param[in] title ウィンドウタイトル
     * @param[in] clientWidth 画面横幅
     * @param[in] clientHeight 画面縦幅
     * @param[in] r クリアカラー（赤）
     * @param[in] g クリアカラー（緑）
     * @param[in] b クリアカラー（青）
     * @param[in] a クリアカラー（アルファ）
     */
   void Initialize(const std::wstring& title, const int32_t& clientWidth, const int32_t& clientHeight,
                    float r, float g, float b, float a = 1.0f);
    
    /**
     * @brief エンジンの初期化（クリアカラー指定付き - std::array版）
     */
    void Initialize(const std::wstring& title, const int32_t& clientWidth, const int32_t& clientHeight,
                    const std::array<float, 4>& clearColor);

    /**
     * @brief エンジンの初期化（クリアカラー指定付き - Vector4版）
     */
    void Initialize(const std::wstring& title, const int32_t& clientWidth, const int32_t& clientHeight,
                    const Vector4& clearColor);

    /**
     * @brief ウィンドウリサイズ時の処理
     * @param[in] width 新しい横幅
     * @param[in] height 新しい縦幅
     */
    void OnResize(int32_t width, int32_t height);

     // --- Application からの注入用コールバック型とセッター ---
    using SceneRegistrar = std::function<void(SceneManager&)>;
   
    /**
     * @brief シーン登録用コールバックの設定
     * @param[in] registrar シーン登録を行う関数
     */
    void SetSceneRegistrar(SceneRegistrar registrar) { sceneRegistrar_ = std::move(registrar); }

    /**
     * @brief 起動時に読み込むシーン名の設定
     * @param[in] name シーン名
     */
    void SetInitialSceneName(std::string name) { initialSceneName_ = std::move(name); }

 private: // メンバ関数(内部処理)

    /**
     * @brief 終了処理
     */
    void Finalize();

    /**
     * @brief フレーム開始処理
     */
    void StartFrame();

    /**
     * @brief フレーム更新処理
     */
    void ProcessFrame();

    /**
     * @brief フレーム終了処理
     */
    void EndFrame();

public: // ゲッター

    /** @name グラフィックス関連の取得 */
    ///@{
    ID3D12GraphicsCommandList* GetCommandList() { return dxCommon_->GetCommandList(); }
    ID3D12Device* GetDevice() { return dxCommon_->GetDevice(); }
    HWND GetHwnd() { return dxCommon_->GetHwnd(); }
    DXGI_SWAP_CHAIN_DESC1& GetSwapChainDesc() { return dxCommon_->GetSwapChainDesc(); }
    D3D12_RENDER_TARGET_VIEW_DESC& GetRtvDesc() { return dxCommon_->GetRtvDesc(); }
    ID3D12DescriptorHeap* GetSrvDescriptorHeap() { return dxCommon_->GetSrvDescriptorHeap(); }
    ID3D12CommandQueue* GetCommandQueue() { return dxCommon_->GetCommandQueue(); }
    IDXGISwapChain4* GetSwapChain() { return dxCommon_->GetSwapChain(); }
    ID3D12Fence* GetFence() { return dxCommon_->GetFence(); }
    HANDLE GetFenceEvent() { return dxCommon_->GetFenceEvent(); }
    ID3D12CommandAllocator* GetCommandAllocator() { return dxCommon_->GetCommandAllocator(); }
    ID3D12RootSignature* GetRootSignature() { return dxCommon_->GetRootSignature(); }
    ID3D12DescriptorHeap* GetDsvDescriptorHeap() { return dxCommon_->GetDsvDescriptorHeap(); }
    ID3D12Resource* GetSwapChainResources(UINT index) { return dxCommon_->GetSwapChainResources(index); }
    D3D12_CPU_DESCRIPTOR_HANDLE& GetRtvHandles(UINT index) { return dxCommon_->GetRtvHandles(index); }
    uint64_t& GetFenceValue() { return dxCommon_->GetFenceValue(); }
    RenderTexture* GetMainRenderTexture() const { return mainRenderTexture_.get(); }
    ///@}

    /** @name マネージャ類の取得 */
    ///@{
    DirectXCommon* GetDirectXCommon() { return this->dxCommon_.get(); }
    InputManager* GetInputManager() { return this->inputManager_.get(); }
    DrawManager* GetDrawManager() { return this->drawManager_.get(); }
    DebugUI* GetDebugUI() { return this->ui_.get(); }
    AudioManager* GetAudioManager() { return this->audioManager_.get(); }
    FontManager* GetFontManager() { return this->fontManager_.get(); }
    TextureManager* GetTextureManager() { return this->textureManager_.get(); }
    ModelManager* GetObjModelManager() { return modelManager_.get(); }
    AnimationManager* GetAnimationManager() { return animationManager_.get(); }
    CameraManager* GetCameraManager() { return cameraManager_.get(); }
#ifdef EditorMode
    EditorManager* GetEditorManager() { return editorManager_.get(); }
#endif
    /** 
     * @brief ポストプロセス管理者を取得
     * @details シーンから pp->AddActiveMode() や pp->GetNoiseParams() のように使用します。
     */
    PostProcessManager* GetPostProcessManager() { return postProcessManager_.get(); }

    /** @brief 画面遷移管理者を取得 */
    SceneTransition* GetSceneTransition() { return sceneTransition_.get(); }
    ///@}

    /** @name 画面情報の取得 */
    ///@{
    int32_t& GetClientWidth() { return dxCommon_->GetClientWidth(); }
    int32_t& GetClientHeight() { return dxCommon_->GetClientHeight(); }
    D3D12_VIEWPORT& GetViewport() { return dxCommon_->GetViewport(); }
    D3D12_RECT& GetScissorRect() { return dxCommon_->GetScissorRect(); }
    PSOManager* GetPSOManager() { return dxCommon_->GetPSOManager(); }
    ///@}

    // 時間関連のゲッター
    float GetDeltaTime() const { return deltaTime_; }
    float GetTotalTime() const { return totalTime_; }
    
    // 追加: ポーズ対応のゲーム内時間関連
    float GetGameTime() const { return gameTime_; }
    float GetGameDeltaTime() const { return gameDeltaTime_; }
    float GetTimeScale() const { return timeScale_; }
    void SetTimeScale(float scale) { timeScale_ = scale; }

    DynamicConstantBuffer<Material>* GetMaterialBufferManager() { return materialBufferManager_.get(); }
    DynamicConstantBuffer<TransformationMatrix>* GetTransformBufferManager() { return transformBufferManager_.get(); }

    // オプション: 取得用
    DescriptorPool* GetSrvPool() const { return dxCommon_->GetSrvPool(); }
    
    // SceneManager参照
    SceneManager* GetSceneManager() const { return sceneManager_.get(); }
    
    // 追加: アセットがロード中かどうかを判定する
    bool IsAssetLoading() const;

public: // セッター
    void AddFenceValue(uint32_t index) { dxCommon_->GetFenceValue() += index; }
    
    // セッター(引数なし描画のためのプリセット切替)
    void SetBlend(BlendMode m) { currentBlend_ = m; }
    void SetDepthWrite(PSOManager::DepthWrite w) { currentDepth_ = w; }
    // 追加: Cull の切替
    void SetCull(PSOManager::CullMode c) { currentCull_ = c; }

    // 追加: クリアカラーのセッター(いつでも変更可能)
    void SetClearColor(float r, float g, float b, float a = 1.0f) { clearColor_ = { r, g, b, a }; }
    void SetClearColor(const std::array<float, 4>& c) { clearColor_ = c; }
    // 追加: Vector4 版
    void SetClearColor(const Vector4& c) { clearColor_ = { c.x, c.y, c.z, c.w }; }

    PostProcessMode GetPostProcessMode() const { return postProcessManager_->GetMode(); }
    void SetPostProcessMode(PostProcessMode mode) { postProcessManager_->SetMode(mode); }
    VignetteParams& GetVignetteParams() { return postProcessManager_->GetVignetteParams(); }
    OutlineParams& GetOutlineParams() { return postProcessManager_->GetOutlineParams(); }
    DissolveParams& GetDissolveParams() { return postProcessManager_->GetDissolveParams(); }
    SmoothingParams& GetSmoothingParams() { return postProcessManager_->GetSmoothingParams(); }
    GaussianParams& GetGaussianParams() { return postProcessManager_->GetGaussianParams(); }
    RadialBlurParams& GetRadialBlurParams() { return postProcessManager_->GetRadialBlurParams(); }
    NoiseParams& GetNoiseParams() { return postProcessManager_->GetNoiseParams(); }


    void SetCursorLocked(bool lock);
    bool IsCursorLocked() const;

    /**
     * @brief 登録されたシェーダー名と現在の状態からPSOを適用する
     * @param shaderName 登録済みのシェーダー名 (例: "Object3D", "Sprite", "Particle" 等)
     */
    void ApplyPSO(const std::string& shaderName);

    /**
     * @brief 電撃エフェクト用パラメータを特設スロットにバインドする
     * @param address LightningParams 構造体の GPU 仮想アドレス
     */
    void BindLightningParams(D3D12_GPU_VIRTUAL_ADDRESS address);

public:
    // 状態(現在のブレンドと深度書き込み)
    BlendMode currentBlend_ = BlendMode::kBlendModeNormal;
    PSOManager::DepthWrite currentDepth_ = PSOManager::DepthWrite::Enable;
    PSOManager::CullMode currentCull_ = PSOManager::CullMode::Back; // 追加: デフォルトは Back

private: // メンバ変数

    // --- Debug & Logging ---

    // リソース解放リークチェック
    D3DResourceLeakChecker leakCheck_;
    
    // ログ
    std::unique_ptr<Log> log_ = nullptr;
   
    // WinApp
    std::unique_ptr<WinApp> winApp_ = nullptr;
    
    // DirectX基盤
    std::unique_ptr<DirectXCommon> dxCommon_ = nullptr;
    
    // --- Manager ---

    // InputManager
    std::unique_ptr <InputManager> inputManager_ = nullptr;
    
    // DrawManager
    std::unique_ptr<DrawManager> drawManager_ = nullptr;
    
    // DebugUI
    std::unique_ptr<DebugUI> ui_ = nullptr;
    
#ifdef EditorMode
    // EditorManager
    std::unique_ptr<EditorManager> editorManager_ = nullptr;
#endif
    
    // TextureManager
    std::unique_ptr<TextureManager> textureManager_ = nullptr;
    
    // AudioManager
    std::unique_ptr<AudioManager> audioManager_ = nullptr;
    
    // SceneManager
    std::unique_ptr<SceneManager> sceneManager_ = nullptr;
    
    // LoadingScreen (SceneManagerから移管)
    std::unique_ptr<LoadingScreen> loadingScreen_ = nullptr;
    
    // FontManager
    std::unique_ptr<FontManager> fontManager_ = nullptr;

    // ModelManager
    std::unique_ptr<ModelManager> modelManager_ = nullptr;
 
    // AnimationManager
    std::unique_ptr<AnimationManager> animationManager_ = nullptr;

    // CameraManager
    std::unique_ptr<CameraManager> cameraManager_ = nullptr;

    // VoxelParticleManager
    std::unique_ptr<VoxelParticleManager> voxelParticleManager_ = nullptr;

    // 画面の色
    std::array<float, 4> clearColor_{ 0.1f, 0.25f, 0.5f, 1.0f };

    // バックバッファのインデックス
    UINT backBufferIndex_{};
    
    // Application から注入
    SceneRegistrar sceneRegistrar_{};
    std::string initialSceneName_{};

    // --- 時間管理 ---
    std::chrono::steady_clock::time_point startTime_{};
    std::chrono::steady_clock::time_point lastFrameTime_{};
    float deltaTime_ = 0.0f;
    float totalTime_ = 0.0f;

    // 追加: ポーズ対応のゲーム内時間管理
    float gameTime_ = 0.0f;
    float gameDeltaTime_ = 0.0f;
    float timeScale_ = 1.0f;

    // --- Dynamic Constant Buffer ---
    std::unique_ptr<DynamicConstantBuffer<Material>> materialBufferManager_ = nullptr;
    std::unique_ptr<DynamicConstantBuffer<TransformationMatrix>> transformBufferManager_ = nullptr;

    // --- 全画面用 RenderTexture ---
    std::unique_ptr<RenderTexture> mainRenderTexture_ = nullptr;
    std::unique_ptr<PostProcessManager> postProcessManager_ = nullptr;
    std::unique_ptr<SceneTransition> sceneTransition_ = nullptr;
    uint32_t depthSrvIndex_ = 0xFFFFFFFF; // 深度SRVのインデックスを保持
    bool isFinalized_ = false; // 終了処理済みフラグ
};
