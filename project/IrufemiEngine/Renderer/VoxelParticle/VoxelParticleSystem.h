#include "../Core/IRenderable.h"
#pragma once
#include "../../Engine/Core/Math/Matrix4x4.h"
#include "../../Engine/Core/Math/Vector3.h"
#include "../../Engine/Core/Math/Vector3Int.h"
#include "../../Engine/Core/Math/Vector4.h"
#include "../../Resource/Model/Data/VoxelizedModel.h"
#include "../../Engine/Core/Type/PerView.h"
#include "../../Engine/Graphics/Compute/IComputeTask.h"
#include <d3d12.h>
#include <memory>
#include <string>
#include <wrl.h>
#include <atomic>
#include <mutex>
#include <future>
#include <array>
#include "../../Engine/Graphics/DirectX/DirectXCommon.h"
#include "../../Engine/Graphics/DirectX/ConstantBuffer.h"


// 前方宣言
class IrufemiEngine;
class Camera;
class ModelManager;
class TextureManager;
struct OBB;

// HLSL側のVoxelParticle構造体と一致させる
struct VoxelParticle {
  Vector3 position;
  float life;
  Vector3 velocity;
  float size;
  Vector4 color;
  Vector3 normal;
  uint32_t isActive; // 0:非アクティブ, 1:アクティブ
  Vector3 rotation; // 各軸の回転角(ラジアン)
  float pad1; // アライメント用
  Vector3 angularVelocity; // 回転速度
  float pad2; // アライメント用
};

// HLSL側のVoxelEmitter構造体と一致させる（16バイトアライメント対応 = 80バイト）
struct VoxelEmitter {
  Vector3 emitPosition = {0.0f, 0.0f, 0.0f};
  float time = 0.0f;
  float lifeTime = 0.8f; // 短くすることで「はじける」感を出し、残像を防ぐ
  float gravity = 2.0f; // 重力を弱める
  uint32_t emit = 0;
  float dispersion = 8.0f; // 拡散を強める
  float convergence = 0.1f; // 収束を弱める
  Vector3 baseVelocity = {0.0f, 0.0f, 0.0f};
  Vector3 rotate = {0.0f, 0.0f, 0.0f};
  float pad1 = 0.0f;
  Vector3 scale = {1.0f, 1.0f, 1.0f};
  uint32_t particleType = 0; // pad0 の代わり

  // 衝突判定用 (OBB近似)
  Vector3 collisionCenter;
  uint32_t useCollision = 0; // 0:無効, 1:有効
  Vector4 collisionOrientations[3]; // 配列要素は16バイトアラインメントが必要
  Vector3 collisionSize;
  float pad2 = 0.0f; // 16バイトアライメント調整用
};


class VoxelParticleSystem : public IComputeTask , public IRenderable {
public:
  enum class LoadingStatus {
    Pending,
    Loading,
    ReadyToCreateResources,
    Loaded,
    Failed
  };

  enum class ParticleType : uint32_t {
    Default = 0,
    Building = 1,
    AshDisintegration = 2,
    FineScatter = 3,
    /// @brief 重力に従って落ちる黒焦げの大きな破片
    DebrisLargeGravity = 4,   
    /// @brief 四散して青白く光る爆発的な破片
    DebrisExplosive = 5
  };

  struct VoxelEmitterParams {
    float lifeTime = 2.0f;
    float gravity = 9.8f;
    float dispersion = 5.0f;
    float convergence = 0.0f;
    ParticleType particleType = ParticleType::Default;

    static VoxelEmitterParams Default() { return { 2.0f, 9.8f, 5.0f, 0.0f, ParticleType::Default }; }
    static VoxelEmitterParams Explode() { return { 0.8f, 2.0f, 8.0f, 0.1f, ParticleType::Default }; }
    static VoxelEmitterParams FineScatter() { return { 0.8f, 10.0f, 60.0f, 0.0f, ParticleType::FineScatter }; }
  };

public:
  VoxelParticleSystem() = default;
  ~VoxelParticleSystem();

  static void SetEngine(IrufemiEngine *engine) { engine_ = engine; }

  void Initialize(const std::string &modelName, const Vector3Int &resolution);

  void DispatchCompute() override;

  void Update(float deltaTime);
  void Draw() override;
  void SyncBeforeDraw() override {}
  void Debug(const char *name);

  void Emit(const Vector3 &position);
  void Explode(const Vector3 &position, const Vector3 &velocity,
               const Vector3 &rotate, const Vector3 &scale);
  
  // 指定したOBBの範囲内にあるボクセルのみをはじけさせる
  void CollisionScatter(const Vector3& position, const Vector3& velocity,
                        const Vector3& rotate, const Vector3& scale,
                        const struct OBB& collisionArea);

  bool IsActive() const {
    if (!hasExploded_)
      return false;
    // 爆散（hasExploded_ = true, time = 0）から lifeTime (+余裕) が経過するまではアクティブ
    return emitterData_.time < (emitterData_.lifeTime + 2.0f);
  }

  float GetEmitterTime() const { return emitterData_.time; }

  void SetParticleType(ParticleType type) { emitterData_.particleType = static_cast<uint32_t>(type); }
  void SetGravity(float gravity) { emitterData_.gravity = gravity; }
  void SetParameters(const VoxelEmitterParams& params);

  bool IsLoaded() const { return status_.load() == LoadingStatus::Loaded; }
  LoadingStatus GetStatus() const { return status_.load(); }

  // 視錐台（Frustum）カリング用
  bool IsInFrustum() const;

private:
  void CreateResources();
  void CreatePSO();
  void CreateCubeMesh(float sizeX, float sizeY, float sizeZ);
  void FinishInitialization();

private:
  std::shared_ptr<VoxelizedModel> voxelModel_;

  // GPUリソース
  Microsoft::WRL::ComPtr<ID3D12Resource> voxelBuffer_;
  Microsoft::WRL::ComPtr<ID3D12Resource> particleBuffer_;
  Microsoft::WRL::ComPtr<ID3D12Resource> cubeVertexBuffer_;
  Microsoft::WRL::ComPtr<ID3D12Resource> cubeIndexBuffer_;

  // デスクリプタハンドル
  D3D12_CPU_DESCRIPTOR_HANDLE voxelSrvHandleCPU_{};
  D3D12_GPU_DESCRIPTOR_HANDLE voxelSrvHandleGPU_{};
  D3D12_CPU_DESCRIPTOR_HANDLE particleSrvHandleCPU_{};
  D3D12_GPU_DESCRIPTOR_HANDLE particleSrvHandleGPU_{};
  D3D12_CPU_DESCRIPTOR_HANDLE particleUavHandleCPU_{};
  D3D12_GPU_DESCRIPTOR_HANDLE particleUavHandleGPU_{};

  // デスクリプタインデックスの保持
  uint32_t voxelSrvIndex_ = 0xFFFFFFFF;
  uint32_t particleUavIndex_ = 0xFFFFFFFF;
  uint32_t particleSrvIndex_ = 0xFFFFFFFF;

  // メッシュビュー
  D3D12_VERTEX_BUFFER_VIEW cubeVertexBufferView_{};
  D3D12_INDEX_BUFFER_VIEW cubeIndexBufferView_{};
  uint32_t cubeIndexCount_ = 0;

  // PSO
  Microsoft::WRL::ComPtr<ID3D12PipelineState> initializePSO_;
  Microsoft::WRL::ComPtr<ID3D12PipelineState> updatePSO_;
  Microsoft::WRL::ComPtr<ID3D12PipelineState> emitPSO_;
  Microsoft::WRL::ComPtr<ID3D12PipelineState> drawPSO_;

  VoxelEmitter emitterData_{};
  ConstantBuffer<VoxelEmitter> emitterBuffer_;
  
  struct PerFrame { float time; float deltaTime; };
  ConstantBuffer<PerFrame> perFrameBuffer_;
  PerFrame perFrameData_{};

  uint32_t voxelCount_ = 0;
  bool isEmitting_ = false;
  bool hasExploded_ = false;
  bool needsInitialize_ = true;
  uint32_t lastUpdateFrame_ = static_cast<uint32_t>(-1);

  struct AsyncLoadData {
    std::shared_ptr<VoxelizedModel> voxelModel;
    uint32_t voxelCount = 0;
    std::atomic<LoadingStatus> status{LoadingStatus::Loading};
  };
  std::shared_ptr<AsyncLoadData> asyncData_;

  std::atomic<LoadingStatus> status_ = LoadingStatus::Pending;
  std::future<void> initializeFuture_;

  bool needsUpdateCS_ = false;
  void SyncConstantBuffers();

  static IrufemiEngine *engine_;
};
