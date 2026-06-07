#include "IrufemiEngine.h"

IrufemiEngine::IrufemiEngine() = default;

#include "Core/Math/Math.h"
#include "Core/Math/Random/Random.h"

#include <DbgHelp.h>
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <format>
#include <string>

#include "../Resource/Audio/Bgm.h"
#include "../Resource/Audio/Se.h"
#include "../Resource/Texture/Texture.h"
#include "Engine/Graphics/Camera/CameraManager.h"
#include "Graphics/DirectX/DirectXUtils.h"
#include "Manager/DebugUI.h"
#include "Manager/PrimitiveManager.h"
#include "Renderer/Core/BaseResource.h"
#include "Renderer/ParticleGPU/GPUParticleManager.h"
#include "Renderer/Effect/Effect.h"
#include "Renderer/LineInstanced/LineClass.h"
#include "Renderer/LineInstanced/LineResource.h"
#include "Renderer/Object2D/Object2DResource.h"
#include "Renderer/Object2D/Primitive/Circle2D.h"
#include "Renderer/Object2D/Sprite/Sprite.h"
#include "Renderer/Object2D/Text/Text.h"
#include "Renderer/Object3D/AnimationModel/AnimationModel.h"
#include "Renderer/Object3D/BaseModel/BaseModel.h"
#include "Renderer/Object3D/StaticModelObject/StaticModelObject.h"
#include "Renderer/Object3D/Object3DResource.h"
#include "Renderer/Object3D/Primitive/Primitive3DObject.h"

#include "Renderer/ParticleGPU/GPUParticleSystem.h"
#include "Renderer/ParticleGPU/ParticleObject.h"
#include "Renderer/Region/ModelRegion.h"
#include "Renderer/Region/PrimitiveRegion.h"
#include "Renderer/Skybox/Skybox.h"
#include "Graphics/Data/VertexData.h"
#include "Renderer/VoxelParticle/VoxelParticleSystem.h"
#include "Renderer/VoxelParticle/VoxelParticleManager.h"
#include "Renderer/ParticleGPU/GPUParticleManager.h"
#include "Framework/IScene.h"
#include "Framework/Component/ComponentFactory.h"

#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxcompiler.lib")

  // デストラクタ
IrufemiEngine::~IrufemiEngine() { Finalize(); }

// 初期化
void IrufemiEngine::Initialize(const std::wstring &title,
                               const int32_t &clientWidth,
                               const int32_t &clientHeight) {
  /*CrashHandler*/
  SetUnhandledExceptionFilter(WinApp::ExportDump);

  // コンポーネント・ファクトリ登録
  ComponentFactory::RegisterAllCoreComponents();

  // 時間計測の開始
  startTime_ = std::chrono::steady_clock::now();
  lastFrameTime_ = startTime_;

  // WinApp をエンジン内で生成・初期化(COM 初期化もここで実施される)
  winApp_ = std::make_unique<WinApp>();
  if (!winApp_->Initialize(GetModuleHandle(nullptr), clientWidth, clientHeight,
                           title.c_str())) {
    assert(false && "WinApp::Initialize failed");
    return;
  }

  // ログを出せるようにする
  log_ = std::make_unique<Log>();
  log_->Initialize();

  // 乱数エンジンのシードを設定
  Random::SeedEngine();

  // AudioManagerの生成・Media Foundationの初期化
  audioManager_ = std::make_unique<AudioManager>();
  audioManager_->StartUp();
  // AudioManagerの初期化
  audioManager_->Initialize();
  // "resources"フォルダから音声ファイルをすべてロード
  audioManager_->LoadAllSoundsFromFolder("resources/");
  Bgm::SetAudioManager(audioManager_.get());
  Se::SetAudioManager(audioManager_.get());

  // DirectX 基盤
  dxCommon_ = std::make_unique<DirectXCommon>();
  dxCommon_->SetLog(log_.get());
  dxCommon_->SetEngine(this);
  dxCommon_->Initialize(winApp_->GetHwnd(), winApp_->GetClientWidth(),
                        winApp_->GetClientHeight());

  BaseResource::SetDirectXCommon(dxCommon_.get());
  BaseRegion::SetDirectXCommon(dxCommon_.get());
  Line3DRegion::SetDirectXCommon(dxCommon_.get());

  // --- Dynamic Constant Buffer の初期化 ---
  materialBufferManager_ = std::make_unique<DynamicConstantBuffer<Material>>();
  materialBufferManager_->Initialize(dxCommon_.get(),
                                     65536); // 最大6万オブジェクト

  transformBufferManager_ =
      std::make_unique<DynamicConstantBuffer<TransformationMatrix>>();
  transformBufferManager_->Initialize(dxCommon_.get(),
                                      65536); // 最大6万オブジェクト

  // SRV ディスクリプタプール
  {
    DescriptorPool *srvPool = dxCommon_->GetSrvPool();

    // 注入
    Texture::SetDescriptorPool(srvPool);
    BaseRegion::SetSrvAllocator(srvPool);

    Line3DRegion::SetSrvAllocator(srvPool);
  }

  // テクスチャ管理
  textureManager_ = std::make_unique<TextureManager>();
  textureManager_->Initialize(dxCommon_.get());

  textureManager_->LoadAllFromFolder("resources/");

  // フォント管理
  fontManager_ = std::make_unique<FontManager>();
  fontManager_->Initialize(this);
  Text::SetFontManager(fontManager_.get());

  // resources/fonts/ 以下のフォントをすべて自動ロード
  fontManager_->LoadAllFromFolder("resources/fonts/");

  // モデル管理
  modelManager_ = std::make_unique<ModelManager>();
  modelManager_->Initialize(dxCommon_.get(),
                            textureManager_.get()); // dxCommon を渡す

  ModelRegion::SetModelManager(modelManager_.get()); // Regionにも設定

  // プリミティブ管理(シングルトン)の初期化
  PrimitiveManager::Initialize();

  // 既存SRVの走査で free-list 再構築
  {
    DescriptorPool *srvPool = dxCommon_->GetSrvPool();
    ID3D12DescriptorHeap *srvHeap = srvPool->GetHeap();
    const uint32_t inc =
        dxCommon_->GetDevice()->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    auto toIndex = [&](D3D12_GPU_DESCRIPTOR_HANDLE h) -> uint32_t {
      if (h.ptr == 0)
        return DescriptorPool::kInvalid;
      const auto heapStart = srvHeap->GetGPUDescriptorHandleForHeapStart().ptr;
      const uint64_t diff = (h.ptr - heapStart);
      return static_cast<uint32_t>(diff / inc);
    };

    std::vector<uint32_t> used;
    // 白テクスチャ
    if (auto white = textureManager_->GetWhiteTextureHandle(); white.ptr != 0) {
      if (auto idx = toIndex(white); idx != DescriptorPool::kInvalid)
        used.push_back(idx);
    }
    // フォントアトラス
    if (fontManager_) {
      if (auto idx = toIndex(fontManager_->GetAtlasSRV()); idx != DescriptorPool::kInvalid)
        used.push_back(idx);
    }
    // テクスチャキャッシュ
    for (const std::string &name : textureManager_->GetTextureNames()) {
      auto h = textureManager_->GetTextureHandle(name);
      if (auto idx = toIndex(h); idx != DescriptorPool::kInvalid)
        used.push_back(idx);
    }
    for (uint32_t i = 0; i < srvPool->BaseIndex(); ++i)
      used.push_back(i);

    std::sort(used.begin(), used.end());
    used.erase(std::unique(used.begin(), used.end()), used.end());

    srvPool->RebuildFreeListExcept(used);
  }

  // 入力
  inputManager_ = std::make_unique<InputManager>();
  inputManager_->Initialize(winApp_->GetHwnd());
  winApp_->SetInputManager(inputManager_.get());

  // UI
  ui_ = std::make_unique<DebugUI>();
  ui_->Initialize(winApp_->GetHwnd(), dxCommon_.get());
#ifdef EditorMode
  editorManager_ = std::make_unique<EditorManager>();
  editorManager_->Initialize(this);
#endif
  Sprite::SetDebugUI(ui_.get());
  Text::SetDebugUI(ui_.get());
  Circle2D::SetDebugUI(ui_.get());

  Primitive3DObject::SetDebugUI(ui_.get());


  // 描画
  drawManager_ = std::make_unique<DrawManager>();
  drawManager_->Initialize(dxCommon_.get());
  Sprite::SetDrawManager(drawManager_.get());
  Text::SetDrawManager(drawManager_.get());
  Circle2D::SetDrawManager(drawManager_.get());

  BaseRegion::SetDrawManager(drawManager_.get());

  GPUParticleSystem::SetDrawManager(drawManager_.get());
  Primitive3DObject::SetDrawManager(drawManager_.get());

  GPUParticleSystem::SetEngine(this);
  Line3DRegion::SetDrawManager(drawManager_.get());

  // テクスチャ設定の注入
  ui_->SetTextureManager(textureManager_.get());
  drawManager_->SetTextureManager(textureManager_.get());
  Sprite::SetTextureManager(textureManager_.get());
  Circle2D::SetTextureManager(textureManager_.get());

  BaseRegion::SetTextureManager(textureManager_.get());

  GPUParticleSystem::SetTextureManager(textureManager_.get());
    ParticleObject::SetTextureManager(textureManager_.get());
  Primitive3DObject::SetTextureManager(textureManager_.get());

  animationManager_ = std::make_unique<AnimationManager>();
  animationManager_->Initialize(dxCommon_.get());

  BaseModel::SetIrufemiEngine(this);
  winApp_->SetInputManager(inputManager_.get());

  Skybox::SetEngine(this);
  GPUParticleSystem::SetDXCommon(dxCommon_.get());
  VoxelParticleSystem::SetEngine(this);

  voxelParticleManager_ = std::make_unique<VoxelParticleManager>();
  voxelParticleManager_->Initialize(this);

  Circle2D::SetEngine(this);
  Line3DRegion::SetEngine(this);
  Primitive3DObject::SetEngine(this);

  // --- 全画面用 RenderTexture の初期化 ---
  mainRenderTexture_ = std::make_unique<RenderTexture>();
  mainRenderTexture_->Initialize(
      dxCommon_.get(), GetClientWidth(), GetClientHeight(),
      DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
      {clearColor_[0], clearColor_[1], clearColor_[2], clearColor_[3]});

  // --- PostProcessManager の初期化 ---
  postProcessManager_ = std::make_unique<PostProcessManager>();
  postProcessManager_->Initialize(dxCommon_.get(), DXGI_FORMAT_R8G8B8A8_UNORM);

  // ノイズテクスチャのロードとハンドル設定
  postProcessManager_->SetDissolveNoiseHandle(
      0, textureManager_->GetTextureHandle("resources/noise0.png"));
  postProcessManager_->SetDissolveNoiseHandle(
      1, textureManager_->GetTextureHandle("resources/noise1.png"));

  // --- 深度バッファの SRV 作成とマネージャーへの設定 ---
  depthSrvIndex_ = dxCommon_->GetSrvPool()->Allocate();
  D3D12_GPU_DESCRIPTOR_HANDLE depthSrvHandleGPU =
      dxCommon_->GetSrvPool()->GetGPUHandle(depthSrvIndex_);

  D3D12_SHADER_RESOURCE_VIEW_DESC depthSrvDesc{};
  depthSrvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
  depthSrvDesc.Shader4ComponentMapping =
      D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  depthSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  depthSrvDesc.Texture2D.MipLevels = 1;
  dxCommon_->GetDevice()->CreateShaderResourceView(
      dxCommon_->GetDepthStencilResource(), &depthSrvDesc,
      dxCommon_->GetSrvPool()->GetCPUHandle(depthSrvIndex_));

  postProcessManager_->SetDepthSrvHandle(depthSrvHandleGPU);

  // --- SceneTransition の初期化 ---
  sceneTransition_ = std::make_unique<SceneTransition>();
  sceneTransition_->Initialize(postProcessManager_.get());

  // WinAppに自身(Engine)のポインタを設定
  winApp_->SetEngine(this);

  // PSO（パイプラインステート）の事前コンパイルを実行し、実行中のヒッチ（カクつき）を防止
  if (GetPSOManager()) {
    GetPSOManager()->PreWarmCommonPSOs();
  }

  // 初回描画時の遅延ハードウェアコンパイル(JIT)を防止するためのダミー実行
  if (dxCommon_) {
    dxCommon_->PreWarmJITCompile();
  }

  GPUParticleManager::GetInstance()->Initialize();
}

  // クリアカラーをfloat配列で持つ初期化
void IrufemiEngine::Initialize(const std::wstring &title,
                               const int32_t &clientWidth,
                               const int32_t &clientHeight, float r, float g,
                               float b, float a) {
  clearColor_ = {r, g, b, a};
  // 既存の Initialize を呼ぶ(互換性維持)
  Initialize(title, clientWidth, clientHeight);
}

  // クリアカラーをstd::arrayで持つ初期化
void IrufemiEngine::Initialize(const std::wstring &title,
                               const int32_t &clientWidth,
                               const int32_t &clientHeight,
                               const std::array<float, 4> &clearColor) {
  clearColor_ = clearColor;
  // 既存の Initialize を呼ぶ(互換性維持)
  Initialize(title, clientWidth, clientHeight);
}

// 追加: Vector4 版 Initialize
void IrufemiEngine::Initialize(const std::wstring &title,
                               const int32_t &clientWidth,
                               const int32_t &clientHeight,
                               const Vector4 &clearColor) {
  clearColor_ = {clearColor.x, clearColor.y, clearColor.z, clearColor.w};
  Initialize(title, clientWidth, clientHeight);
}

void IrufemiEngine::Finalize() {
  if (isFinalized_) return;

  // 0. シーンと画面遷移・ローディング（これらがリソースの shared_ptr を保持しているため最優先）
  if (sceneManager_) {
    sceneManager_.reset();
  }
  if (loadingScreen_) {
    loadingScreen_.reset();
  }
  if (sceneTransition_) {
    sceneTransition_.reset();
  }
  if (cameraManager_) {
    cameraManager_.reset();
  }

  // アプリケーション終了時、シーン破棄前にGPU処理の完了を待つ
  if (dxCommon_) {
    dxCommon_->WaitForGPU();
  }

  // 1. エディタとUI (描画マネージャ等に依存)
  if (ui_) {
    ui_->Shutdown();
    ui_.reset();
  }
#ifdef EditorMode
  if (editorManager_) {
    editorManager_.reset();
  }
#endif

  // 2. 描画・ポストプロセス系 (DirectX基盤に依存)
  if (drawManager_) {
    drawManager_->Finalize();
    drawManager_.reset();
  }
  if (postProcessManager_) {
    postProcessManager_.reset();
  }
  if (mainRenderTexture_) {
    mainRenderTexture_.reset();
  }

  // 3. アニメーション・モデル・テクスチャ (リソースの実体を保持)
  if (animationManager_) {
    animationManager_.reset();
  }
  if (modelManager_) {
    modelManager_.reset();
  }
  if (textureManager_) {
    textureManager_.reset();
  }
  if (fontManager_) {
    fontManager_->Finalize();
    fontManager_.reset();
  }

  // 4. 定数バッファマネージャー (DirectX基盤のリソースを直接保持するため先に破棄)
  if (materialBufferManager_) {
    materialBufferManager_.reset();
  }
  if (transformBufferManager_) {
    transformBufferManager_.reset();
  }

  // --- 静的ポインタのクリア（デストラクタでの不正アクセス防止） ---
  BaseResource::SetDirectXCommon(nullptr);
  BaseRegion::SetDirectXCommon(nullptr);
  Line3DRegion::SetDirectXCommon(nullptr);

  Texture::SetDescriptorPool(nullptr);
  Texture::SetDirectXCommon(nullptr);
  Texture::SetWhiteTextureResource(nullptr);
  GpuMesh::sDxCommon = nullptr;

  BaseRegion::SetSrvAllocator(nullptr);

  Line3DRegion::SetSrvAllocator(nullptr);

  // DebugUI, DrawManager, TextureManager 等のクラスへの静的セットもクリア
  Sprite::SetDebugUI(nullptr);
  Circle2D::SetDebugUI(nullptr);
  Primitive3DObject::SetDebugUI(nullptr);


  Sprite::SetDrawManager(nullptr);
  Circle2D::SetDrawManager(nullptr);
  BaseRegion::SetDrawManager(nullptr);

  GPUParticleSystem::SetDrawManager(nullptr);
  Primitive3DObject::SetDrawManager(nullptr);
  Line3DRegion::SetDrawManager(nullptr);

  Sprite::SetTextureManager(nullptr);
  Circle2D::SetTextureManager(nullptr);
  BaseRegion::SetTextureManager(nullptr);

  GPUParticleSystem::SetTextureManager(nullptr);
    ParticleObject::SetTextureManager(nullptr);
  Primitive3DObject::SetTextureManager(nullptr);

  Sprite::SetCameraManager(nullptr);
  ModelRegion::SetModelManager(nullptr);

  GPUParticleSystem::SetEngine(nullptr);
  BaseModel::SetIrufemiEngine(nullptr);
  Skybox::SetEngine(nullptr);
  GPUParticleSystem::SetDXCommon(nullptr);
  VoxelParticleSystem::SetEngine(nullptr);
  if (voxelParticleManager_) {
    voxelParticleManager_.reset();
  }
  GPUParticleManager::GetInstance()->Finalize();
  Circle2D::SetEngine(nullptr);
  Line3DRegion::SetEngine(nullptr);
  Primitive3DObject::SetEngine(nullptr);
  Effect::SetEngine(nullptr);
  Bgm::SetAudioManager(nullptr);
  Se::SetAudioManager(nullptr);

  // 5. シングルトンの破棄(GPUリソースを保持している可能性があるためdxCommon破棄前に呼ぶ)
  PrimitiveManager::Finalize();

  // 6. 基盤システム (サウンド・入力)
  if (audioManager_) {
    audioManager_->Finalize();
    audioManager_.reset();
  }
  if (inputManager_) {
    inputManager_.reset();
  }

  // 5.5 Dynamic Constant Buffers
  // (DirectX基盤に依存するため、dxCommon_より先に破棄)
  if (materialBufferManager_) {
    materialBufferManager_.reset();
  }
  if (transformBufferManager_) {
    transformBufferManager_.reset();
  }

  // 6. DirectX基盤
  if (dxCommon_) {
    if (dxCommon_->GetSrvPool() && depthSrvIndex_ != 0xFFFFFFFF) {
      dxCommon_->GetSrvPool()->FreeAfterFence(depthSrvIndex_,
                                              dxCommon_->GetFenceValue());
      depthSrvIndex_ = 0xFFFFFFFF;
    }
    dxCommon_->Finalize();
    dxCommon_.reset();
  }

  // 7. OS・ウィンドウ
  if (winApp_) {
    winApp_.reset();
  }

  isFinalized_ = true;
}

void IrufemiEngine::Execute() {
  // CameraManager設定
  cameraManager_ = std::make_unique<CameraManager>();
  Sprite::SetCameraManager(cameraManager_.get());
  Text::SetCameraManager(cameraManager_.get());
  
  // SceneManager 構築(エンジンは所有のみ)
  sceneManager_ = std::make_unique<SceneManager>(this);

  // ローディング画面の構築
  loadingScreen_ = std::make_unique<LoadingScreen>();
  loadingScreen_->Initialize(this);

  // Application からの登録を反映
  if (sceneRegistrar_) {
    sceneRegistrar_(*sceneManager_);
  }

  // 初期シーンが指定されていれば遷移
  if (!initialSceneName_.empty()) {
    sceneManager_->ChangeTo(initialSceneName_);
  }

  while (winApp_->ProcessMessages()) {
    // フレーム開始時の時間更新
    StartFrame();

    // ImGui_
    ui_->FrameStart();

#ifdef EditorMode
    if (editorManager_) {
        editorManager_->DrawEditorUI();
    }
#endif

#ifdef USE_IMGUI
    ui_->FPSDebug();
    ui_->BeginEngineDebugWindow();
    ui_->SceneSelectorTab(sceneManager_.get());
    ui_->PostProcessTab(this);
    GPUParticleManager::GetInstance()->Debug();
    if (auto *scene = sceneManager_->GetCurrentScene()) {
      scene->DrawDebugTab();
    }
    ui_->EndEngineDebugWindow();
#endif // USE_IMGUI

    // 更新
    audioManager_->Update();
    sceneManager_->Update();
    if (sceneManager_->IsLoading() && loadingScreen_) {
      loadingScreen_->Update(deltaTime_);
    }
    totalTime_ += deltaTime_;
    postProcessManager_->Update(totalTime_);
    sceneTransition_->Update(deltaTime_);
    
    GPUParticleManager::GetInstance()->Update();

    if (voxelParticleManager_) {
        // ポーズ中は VoxelParticle の更新をスキップする
        bool isPaused = (sceneManager_ && sceneManager_->GetCurrent() == "Pause");
        if (!isPaused) {
            voxelParticleManager_->Update(deltaTime_);
        }
    }

    // インプットを更新
    inputManager_->Update();

    // フレーム途中処理
    ProcessFrame();

    // 描画
    sceneManager_->Draw();

    if (voxelParticleManager_) {
        voxelParticleManager_->Draw();
    }

    GPUParticleManager::GetInstance()->Draw();

  // ここで溜まった描画パケットを一斉に処理する
    drawManager_->ExecuteRenderQueues(this);

    // 終了処理
    EndFrame();
  }
}

  // フレーム開始処理
void IrufemiEngine::StartFrame() {
  // 時間の更新
  auto now = std::chrono::steady_clock::now();
  deltaTime_ = std::chrono::duration<float>(now - lastFrameTime_).count();
  totalTime_ = std::chrono::duration<float>(now - startTime_).count();

  // ゲーム内時間の更新（タイムスケールを適用）
  gameDeltaTime_ = deltaTime_ * timeScale_;
  gameTime_ += gameDeltaTime_;

  lastFrameTime_ = now;
}

  // フレーム途中処理
void IrufemiEngine::ProcessFrame() {
  // 非同期スレッドで遅延されていたSRVの更新をメインスレッドで一括適用する（データ競合の防止）
  if (dxCommon_) {
    dxCommon_->FlushPendingSRVUpdates();
  }

  // ステートのリセット（前フレームの描画ステートを引き継がないようにする）
  currentBlend_ = BlendMode::kBlendModeNormal;
  currentDepth_ = PSOManager::DepthWrite::Enable;
  currentCull_ = PSOManager::CullMode::Back;

  // 描画処理に入る前にImGui_::Renderを積む
  ui_->QueueDrawCommands();

  // 1. バックバッファをクリア (念のため)
  drawManager_->PreDraw(clearColor_, 1.0f, 0);

  // (Compute Shaderの一括実行は、RenderGraph内のComputePassに移行しました)

  // 2. メインの描画先を RenderTexture に切り替え、指定のクリアカラーでクリア
  drawManager_->BeginRenderTexture(
      mainRenderTexture_.get(),
      Vector4{clearColor_[0], clearColor_[1], clearColor_[2], clearColor_[3]});
}

  // フレーム終了処理
void IrufemiEngine::EndFrame() {
  // (RenderTexture の SRV 化やバックバッファへの転送、
  // ポストプロセス処理はすべて RenderGraph 内で行われます)

  // 描画先がバックバッファになり、ポストプロセス（暗転など）が掛かった上から
  // 影響を受けない最前面UIとしてロード画面を描画する
  if (sceneManager_ && sceneManager_->IsLoading()) {
    if (loadingScreen_) {
      loadingScreen_->Draw(this);
    }
  }

  // --- 追加: 最前面UIキューの消化 ---
  drawManager_->ExecuteTopMostQueues(this);

  // 描画後処理
  ui_->QueuePostDrawCommands();
  drawManager_->PostDraw();

  // 5) フレーム終端で遅延解放の回収(フェンス完了値を渡す)
  if (auto *srvPool = dxCommon_->GetSrvPool()) {
    const uint64_t completed = dxCommon_->GetFence()->GetCompletedValue();
    srvPool->GarbageCollect(completed);
  }

  // --- 追加: 中間リソースの遅延解放を実行 ---
  dxCommon_->ClearPendingResources();
}

void IrufemiEngine::OnResize(int32_t width, int32_t height) {
  if (width <= 0 || height <= 0)
    return;

  // 1. スワップチェーン、深度バッファのリサイズ
  dxCommon_->ResizeSwapChain(width, height);

  // 2. メインレンダーテクスチャの再生成
  mainRenderTexture_->Initialize(
      dxCommon_.get(), width, height, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
      {clearColor_[0], clearColor_[1], clearColor_[2], clearColor_[3]});

  // 3. 深度バッファの SRV 再作成 (既存のインデックスを再利用)
  if (depthSrvIndex_ != 0xFFFFFFFF) {
    D3D12_SHADER_RESOURCE_VIEW_DESC depthSrvDesc{};
    depthSrvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    depthSrvDesc.Shader4ComponentMapping =
        D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    depthSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    depthSrvDesc.Texture2D.MipLevels = 1;
    dxCommon_->GetDevice()->CreateShaderResourceView(
        dxCommon_->GetDepthStencilResource(), &depthSrvDesc,
        dxCommon_->GetSrvPool()->GetCPUHandle(depthSrvIndex_));

    // ポストプロセスマネージャーに新しいSRVハンドルを設定
    postProcessManager_->SetDepthSrvHandle(
        dxCommon_->GetSrvPool()->GetGPUHandle(depthSrvIndex_));
  }
  
  // 4. カメラの解像度更新 (3D空間の歪み防止)
  if (cameraManager_) {
      cameraManager_->OnResize(width, height);
  }

  // 5. 描画マネージャーへの通知 (RenderGraph等のキャッシュクリア)
  if (drawManager_) {
      drawManager_->OnResize(width, height);
      
      // 再構築された永続リソースの初期ステートをRenderGraphへ再登録する
      if (mainRenderTexture_ && mainRenderTexture_->GetResource()) {
          drawManager_->RegisterResourceState(mainRenderTexture_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
      }
      if (dxCommon_ && dxCommon_->GetDepthStencilResource()) {
          drawManager_->RegisterResourceState(dxCommon_->GetDepthStencilResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
      }
  }
}

void IrufemiEngine::SetCursorLocked(bool lock) {
  if (winApp_) {
    winApp_->SetCursorLocked(lock);
  }
}

bool IrufemiEngine::IsCursorLocked() const {
  if (winApp_) {
    return winApp_->IsCursorLocked();
  }
  return false;
}

void IrufemiEngine::ApplyPSO(const std::string& shaderName) {
  // Shadowパスの場合は自動的にシャドウ用シェーダに切り替える(元のコードの仕様維持)
  if (drawManager_->IsShadowPass()) {
      if (shaderName == "Object3D") {
          auto* pso = GetPSOManager()->GetPSO("Shadow", BlendMode::kBlendModeNone, PSOManager::DepthWrite::Enable, currentCull_);
          if (pso) drawManager_->BindPSO(pso);
          return;
      }
      else if (shaderName == "Skinning") {
          auto* pso = GetPSOManager()->GetPSO("ShadowSkinning", BlendMode::kBlendModeNone, PSOManager::DepthWrite::Enable, currentCull_);
          if (pso) drawManager_->BindPSO(pso);
          return;
      }
      // それ以外はシャドウパスでは描画しない(無視)
      return;
  }
  
  // Skybox用の特殊対応 (元のコードでは CullMode::Front 決め打ちでブレンドと深度は不要だった)
  if (shaderName == "Skybox") {
      auto* pso = GetPSOManager()->GetPSO("Skybox", BlendMode::kBlendModeNone, PSOManager::DepthWrite::Disable, PSOManager::CullMode::Front);
      if (pso) drawManager_->BindPSO(pso);
      return;
  }

  auto* pso = GetPSOManager()->GetPSO(shaderName, currentBlend_, currentDepth_, currentCull_);
  assert(pso && ("PSO is null for " + shaderName).c_str());
  if (pso) {
    drawManager_->BindPSO(pso);
  }
}

void IrufemiEngine::BindLightningParams(D3D12_GPU_VIRTUAL_ADDRESS address) {
  if (address == 0)
    return;
  GetCommandList()->SetGraphicsRootConstantBufferView((UINT)RootSlot::Special,
                                                      address);
}

bool IrufemiEngine::IsAssetLoading() const {
  bool modelsLoaded = !modelManager_ || modelManager_->IsAllLoaded();
  bool texturesLoaded = !textureManager_ || textureManager_->IsAllLoaded();
  bool fontsLoaded = !fontManager_ || fontManager_->IsAllLoaded();
  return !modelsLoaded || !texturesLoaded || !fontsLoaded;
}

