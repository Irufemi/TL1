#define NOMINMAX
#include "VoxelParticleSystem.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Graphics/DirectX/DescriptorPool.h"
#include "Engine/Graphics/DirectX/DirectXCommon.h"
#include "Engine/Graphics/DirectX/DirectXUtils.h"
#include "Engine/Graphics/Pipeline/PSOManager.h"
#include "Engine/IrufemiEngine.h"
#include "Engine/Manager/DebugUI.h"
#include "Engine/Manager/DrawManager.h"
#include "../../Engine/Graphics/Data/VertexData.h"
#include "Engine/Graphics/DirectX/RootSignatureConfig.h"
#include "Resource/Model/ModelManager.h"
#include <cassert>
#include <cstdio>
#include "Engine/Core/Math/Geometry/OBB.h"
#include "Engine/Core/Math/Geometry/Collision.h"
#include "Engine/Core/Shape/Sphere.h"
#include <Windows.h>
#include <algorithm>

IrufemiEngine *VoxelParticleSystem::engine_ = nullptr;

VoxelParticleSystem::~VoxelParticleSystem() {
  if (engine_) {
    if (auto* dxCommon = engine_->GetDirectXCommon()) {
      uint64_t fv = dxCommon->GetCurrentFrameFenceValue();
      if (auto* srvPool = dxCommon->GetSrvPool()) {
        srvPool->FreeAfterFence(voxelSrvIndex_, fv);
        srvPool->FreeAfterFence(particleUavIndex_, fv);
        srvPool->FreeAfterFence(particleSrvIndex_, fv);
      }
      dxCommon->ReleaseAfterFence(voxelBuffer_);
      dxCommon->ReleaseAfterFence(particleBuffer_);
      dxCommon->ReleaseAfterFence(cubeVertexBuffer_);
      dxCommon->ReleaseAfterFence(cubeIndexBuffer_);
    }
  }
}

void VoxelParticleSystem::Initialize(const std::string &modelName,
                                     const Vector3Int &resolution) {
  assert(engine_);

  status_.store(LoadingStatus::Loading);

  auto* modelManager = engine_->GetObjModelManager();

  auto asyncData = std::make_shared<AsyncLoadData>();
  asyncData_ = asyncData;

  // スレッドプールのデッドロック（ワーカー枯渇）を防ぐため、ボクセル化タスクを積む前に
  // 元となるモデルデータを同期ロード（またはキャッシュから取得）しておく
  auto managedModel = modelManager->GetModel(modelName);

  // 非同期でボクセル化（またはキャッシュから取得）を開始
  initializeFuture_ = modelManager->EnqueueTask([asyncData, modelName, resolution, modelManager, managedModel]() {
    // ModelManager側でキャッシュ済みのものがあればそれを返し、無ければ新規計算する
    auto vModel = modelManager->GetVoxelizedModel(modelName, resolution);

    if (!vModel || vModel->voxels.empty()) {
      asyncData->status.store(LoadingStatus::Failed);
      OutputDebugStringA("[Voxel] ERROR: Voxel count is ZERO or failed to load.\n");
      return;
    }

    asyncData->voxelCount = static_cast<uint32_t>(vModel->voxels.size());
    asyncData->voxelModel = vModel;
    // 計算完了
    asyncData->status.store(LoadingStatus::ReadyToCreateResources);
  });
}

void VoxelParticleSystem::FinishInitialization() {
  if (status_.load() != LoadingStatus::ReadyToCreateResources) {
    return;
  }

  // 1. 立方体メッシュの作成 (ボクセルサイズを計算して渡す)
  float voxelW, voxelH, voxelD;
  voxelW = (voxelModel_->aabbMax.x - voxelModel_->aabbMin.x) /
                 voxelModel_->resolution.x;
  voxelH = (voxelModel_->aabbMax.y - voxelModel_->aabbMin.y) /
                 voxelModel_->resolution.y;
  voxelD = (voxelModel_->aabbMax.z - voxelModel_->aabbMin.z) /
                 voxelModel_->resolution.z;
  
  CreateCubeMesh(voxelW, voxelH, voxelD);

  // 2. GPUリソースの作成
  CreateResources();

  // 3. PSOの作成
  CreatePSO();

  // 4. 定数バッファのマッピング (CreateResources内で実施済みのため削除)

  needsInitialize_ = true;
  status_.store(LoadingStatus::Loaded);

  char log[256];
  sprintf_s(log, "[Voxel] Async Initialization Finished. Count: %u\n", voxelCount_);
  OutputDebugStringA(log);
}

void VoxelParticleSystem::Update(float deltaTime) {
  if (asyncData_) {
    auto s = asyncData_->status.load();
    if (s == LoadingStatus::ReadyToCreateResources) {
        voxelModel_ = std::move(asyncData_->voxelModel);
        voxelCount_ = asyncData_->voxelCount;
        status_.store(LoadingStatus::ReadyToCreateResources);
        asyncData_.reset();
        FinishInitialization();
    } else if (s == LoadingStatus::Failed) {
        status_.store(LoadingStatus::Failed);
        asyncData_.reset();
    }
  }

  if (status_.load() != LoadingStatus::Loaded || voxelCount_ == 0)
    return;

  // エミッターデータ更新
  float actualDeltaTime = engine_->GetGameDeltaTime();
  emitterData_.time += actualDeltaTime;
  emitterData_.emit = isEmitting_ ? 1 : 0;
  
  uint32_t frameIndex = engine_->GetDrawManager()->GetDxCommon()->GetFrameIndex();
  
  // PerFrame データを更新（time と deltaTime を CS シェーダーへ渡す）
  perFrameData_.time = emitterData_.time;
  perFrameData_.deltaTime = actualDeltaTime;

  // GPUバッファへの同期をUpdate内で1回だけ行う
  SyncConstantBuffers();

  needsUpdateCS_ = true;
  if (engine_ && engine_->GetDrawManager()) {
      engine_->GetDrawManager()->RegisterComputeTask(this);
  }
}

void VoxelParticleSystem::SyncConstantBuffers() {
    uint32_t frameIndex = engine_->GetDrawManager()->GetDxCommon()->GetFrameIndex();
    
    // lastUpdateFrame_ による早期リターンを削除
    // 理由：Update()の後にScatterAt()が呼ばれるとパラメータが変わるが、
    // ここで弾かれると同フレーム内で変更がGPUへ反映されず、古いパラメータでComputeShaderが走ってしまうため。
    emitterBuffer_.Update(emitterData_, frameIndex);
    perFrameBuffer_.Update(perFrameData_, frameIndex);
    lastUpdateFrame_ = frameIndex;
}


bool VoxelParticleSystem::IsInFrustum() const {
    if (!engine_ || !hasExploded_) return false;
    auto* camManager = engine_->GetCameraManager();
    if (!camManager) return true;
    Camera* activeCam = camManager->GetActiveCamera();
    if (!activeCam) return true;

    Sphere sphere;
    sphere.center = emitterData_.emitPosition;
    // 拡散するパーティクルの最大範囲を見積もる
    sphere.radius = 80.0f; 
    
    return Collision::IsCollision(activeCam->GetFrustum(), sphere);
}

void VoxelParticleSystem::DispatchCompute() {
  if (status_.load() != LoadingStatus::Loaded || !voxelBuffer_ || !engine_)
    return;

  if (!IsInFrustum()) return;

  ID3D12GraphicsCommandList *commandList = engine_->GetCommandList();
  auto *dxCommon = engine_->GetDirectXCommon();
  uint32_t frameIndex = dxCommon->GetFrameIndex();

  if (needsUpdateCS_ || isEmitting_ || needsInitialize_) {
    ID3D12DescriptorHeap *ppHeaps[] = {dxCommon->GetSrvPool()->GetHeap()};
    commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
    commandList->SetComputeRootSignature(dxCommon->GetComputeRootSignature());

    if (needsInitialize_) {
      commandList->SetPipelineState(initializePSO_.Get());
      commandList->SetComputeRootDescriptorTable(0, voxelSrvHandleGPU_);    // t0
      commandList->SetComputeRootDescriptorTable(3, particleUavHandleGPU_); // u0
      commandList->Dispatch((voxelCount_ + 63) / 64, 1, 1);
      DirectXUtils::UAVBarrier(commandList, particleBuffer_.Get());
      needsInitialize_ = false;
    }

    if (isEmitting_) {
      commandList->SetPipelineState(emitPSO_.Get());
      commandList->SetComputeRootDescriptorTable(0, voxelSrvHandleGPU_);    // t0
      commandList->SetComputeRootDescriptorTable(3, particleUavHandleGPU_); // u0
      commandList->SetComputeRootConstantBufferView(4, emitterBuffer_.GetGPUVirtualAddress(frameIndex));
      commandList->SetComputeRootConstantBufferView(5, perFrameBuffer_.GetGPUVirtualAddress(frameIndex));
      commandList->Dispatch((voxelCount_ + 63) / 64, 1, 1);
      DirectXUtils::UAVBarrier(commandList, particleBuffer_.Get());
      isEmitting_ = false;
    }

    if (needsUpdateCS_) {
        commandList->SetPipelineState(updatePSO_.Get());
        commandList->SetComputeRootDescriptorTable(3, particleUavHandleGPU_); // u0
        commandList->SetComputeRootConstantBufferView(4, emitterBuffer_.GetGPUVirtualAddress(frameIndex));
        commandList->SetComputeRootConstantBufferView(5, perFrameBuffer_.GetGPUVirtualAddress(frameIndex));
        commandList->Dispatch((voxelCount_ + 63) / 64, 1, 1);
        DirectXUtils::UAVBarrier(commandList, particleBuffer_.Get());
        needsUpdateCS_ = false;
    }
  }
}

void VoxelParticleSystem::Draw() {
  if (status_.load() != LoadingStatus::Loaded || !voxelBuffer_ || !engine_)
    return;

  // シャドウパス中は描画しない
  if (engine_->GetDrawManager()->IsShadowPass()) {
      return;
  }

  auto* engine = engine_;
  auto* dxCommon = engine_->GetDirectXCommon();
  uint32_t frameIndex = dxCommon->GetFrameIndex();

  // 2. Graphics Draw
  if (!hasExploded_)
    return;

  if (!IsInFrustum()) return;

  engine_->GetDrawManager()->SubmitVoxelParticle(
      voxelCount_,
      cubeVertexBufferView_,
      cubeIndexBufferView_,
      cubeIndexCount_,
      emitterBuffer_.GetGPUVirtualAddress(frameIndex),
      particleSrvHandleGPU_,
      particleBuffer_.Get(),
      drawPSO_.Get()
  );
}

void VoxelParticleSystem::Emit(const Vector3 &position) {
  emitterData_.emitPosition = position;
  emitterData_.baseVelocity = {0, 0, 0};
  emitterData_.rotate = {0, 0, 0};
  emitterData_.scale = {1, 1, 1};
  emitterData_.time = 0.0f;
  emitterData_.useCollision = 0; // 衝突判定無効
  isEmitting_ = true;
  emitterData_.emit = 1;
  hasExploded_ = true;
  SyncConstantBuffers();
}

void VoxelParticleSystem::Explode(const Vector3 &position,
                                  const Vector3 &velocity,
                                  const Vector3 &rotate,
                                  const Vector3 &scale) {
  emitterData_.emitPosition = position;
  emitterData_.baseVelocity = velocity;
  emitterData_.rotate = rotate;
  emitterData_.scale = scale;
  emitterData_.time = 0.0f;
  emitterData_.useCollision = 0; // 衝突判定無効
  isEmitting_ = true;
  emitterData_.emit = 1;
  hasExploded_ = true;
  SyncConstantBuffers();
}

void VoxelParticleSystem::CollisionScatter(const Vector3 &position,
                                           const Vector3 &velocity,
                                           const Vector3 &rotate,
                                           const Vector3 &scale,
                                           const OBB &collisionArea) {
  if (isEmitting_) {
    // 既にエミット待ちの場合は、領域を広げて両方の衝突をカバーするようにする
    // 簡易的に AABB ベースで合成領域を計算
    Vector3 minA = Math::Subtract(emitterData_.collisionCenter, emitterData_.collisionSize);
    Vector3 maxA = Math::Add(emitterData_.collisionCenter, emitterData_.collisionSize);
    Vector3 minB = Math::Subtract(collisionArea.center, collisionArea.size);
    Vector3 maxB = Math::Add(collisionArea.center, collisionArea.size);

    Vector3 newMin = { (std::min)(minA.x, minB.x), (std::min)(minA.y, minB.y), (std::min)(minA.z, minB.z) };
    Vector3 newMax = { (std::max)(maxA.x, maxB.x), (std::max)(maxA.y, maxB.y), (std::max)(maxA.z, maxB.z) };

    emitterData_.collisionCenter = Math::Multiply(0.5f, Math::Add(newMin, newMax));
    emitterData_.collisionSize = Math::Multiply(0.5f, Math::Subtract(newMax, newMin));
    // 合成後は軸並行（回転なし）として扱う
    for (int i = 0; i < 3; ++i) {
      emitterData_.collisionOrientations[i] = { 0.0f, 0.0f, 0.0f, 0.0f };
    }
    emitterData_.collisionOrientations[0].x = 1.0f;
    emitterData_.collisionOrientations[1].y = 1.0f;
    emitterData_.collisionOrientations[2].z = 1.0f;
  } else {
    emitterData_.emitPosition = position;
    emitterData_.baseVelocity = velocity;
    emitterData_.rotate = rotate;
    emitterData_.scale = scale;
    emitterData_.time = 0.0f;

    // 衝突判定用データ設定
    emitterData_.useCollision = 1;
    emitterData_.collisionCenter = collisionArea.center;
    emitterData_.collisionSize = collisionArea.size;
    for (int i = 0; i < 3; ++i) {
      emitterData_.collisionOrientations[i].x = collisionArea.orientations[i].x;
      emitterData_.collisionOrientations[i].y = collisionArea.orientations[i].y;
      emitterData_.collisionOrientations[i].z = collisionArea.orientations[i].z;
      emitterData_.collisionOrientations[i].w = 0.0f;
    }
  }

  isEmitting_ = true;
  emitterData_.emit = 1;
  hasExploded_ = true;
  SyncConstantBuffers();
}

void VoxelParticleSystem::CreateCubeMesh(float sizeX, float sizeY,
                                         float sizeZ) {
  auto *dxCommon = engine_->GetDirectXCommon();
  std::vector<VertexData> vertices;
  std::vector<uint32_t> indices;

  // 立方体(中心原点、各軸の辺長 = sizeX/Y/Z)
  const float hx = sizeX * 0.5f;
  const float hy = sizeY * 0.5f;
  const float hz = sizeZ * 0.5f;
  vertices = {
      // 前
      {{-hx, -hy, -hz, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
      {{-hx, hy, -hz, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
      {{hx, hy, -hz, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
      {{hx, -hy, -hz, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
      // 後
      {{hx, -hy, hz, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
      {{hx, hy, hz, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
      {{-hx, hy, hz, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
      {{-hx, -hy, hz, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
      // 左
      {{-hx, -hy, hz, 1.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
      {{-hx, hy, hz, 1.0f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
      {{-hx, hy, -hz, 1.0f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
      {{-hx, -hy, -hz, 1.0f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
      // 右
      {{hx, -hy, -hz, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
      {{hx, hy, -hz, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
      {{hx, hy, hz, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
      {{hx, -hy, hz, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
      // 下
      {{-hx, -hy, hz, 1.0f}, {0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
      {{-hx, -hy, -hz, 1.0f}, {0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
      {{hx, -hy, -hz, 1.0f}, {1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
      {{hx, -hy, hz, 1.0f}, {1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
      // 上
      {{-hx, hy, -hz, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
      {{-hx, hy, hz, 1.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
      {{hx, hy, hz, 1.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
      {{hx, hy, -hz, 1.0f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
  };
  indices = {
      0,  1,  2,  0,  2,  3,  // 前
      4,  5,  6,  4,  6,  7,  // 後
      8,  9,  10, 8,  10, 11, // 左
      12, 13, 14, 12, 14, 15, // 右
      16, 17, 18, 16, 18, 19, // 下
      20, 21, 22, 20, 22, 23, // 上
  };
  cubeIndexCount_ = static_cast<uint32_t>(indices.size());

  // Vertex Buffer
  cubeVertexBuffer_ =
      dxCommon->CreateBufferResource(sizeof(VertexData) * vertices.size());
  VertexData *vertexData = nullptr;
  cubeVertexBuffer_->Map(0, nullptr, reinterpret_cast<void **>(&vertexData));
  std::memcpy(vertexData, vertices.data(),
              sizeof(VertexData) * vertices.size());
  cubeVertexBuffer_->Unmap(0, nullptr);

  cubeVertexBufferView_.BufferLocation =
      cubeVertexBuffer_->GetGPUVirtualAddress();
  cubeVertexBufferView_.SizeInBytes =
      sizeof(VertexData) * static_cast<UINT>(vertices.size());
  cubeVertexBufferView_.StrideInBytes = sizeof(VertexData);

  // Index Buffer
  cubeIndexBuffer_ =
      dxCommon->CreateBufferResource(sizeof(uint32_t) * indices.size());
  uint32_t *indexData = nullptr;
  cubeIndexBuffer_->Map(0, nullptr, reinterpret_cast<void **>(&indexData));
  std::memcpy(indexData, indices.data(), sizeof(uint32_t) * indices.size());
  cubeIndexBuffer_->Unmap(0, nullptr);

  cubeIndexBufferView_.BufferLocation =
      cubeIndexBuffer_->GetGPUVirtualAddress();
  cubeIndexBufferView_.SizeInBytes =
      sizeof(uint32_t) * static_cast<UINT>(indices.size());
  cubeIndexBufferView_.Format = DXGI_FORMAT_R32_UINT;
}

void VoxelParticleSystem::CreateResources() {
  auto *dxCommon = engine_->GetDirectXCommon();
  auto *srvPool = dxCommon->GetSrvPool();
  auto *device = engine_->GetDevice();

  // Voxelデータ用バッファ (SRV)
  voxelBuffer_ = dxCommon->CreateBufferResource(sizeof(Voxel) * voxelCount_);
  Voxel *voxelData = nullptr;
  voxelBuffer_->Map(0, nullptr, reinterpret_cast<void **>(&voxelData));
  std::memcpy(voxelData, voxelModel_->voxels.data(),
              sizeof(Voxel) * voxelCount_);
  voxelBuffer_->Unmap(0, nullptr);

  uint32_t voxelSrvIndex = srvPool->Allocate();
  voxelSrvIndex_ = voxelSrvIndex;
  voxelSrvHandleCPU_ = srvPool->GetCPUHandle(voxelSrvIndex);
  voxelSrvHandleGPU_ = srvPool->GetGPUHandle(voxelSrvIndex);

  D3D12_SHADER_RESOURCE_VIEW_DESC voxelSrvDesc{};
  voxelSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
  voxelSrvDesc.Shader4ComponentMapping =
      D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  voxelSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
  voxelSrvDesc.Buffer.FirstElement = 0;
  voxelSrvDesc.Buffer.NumElements = voxelCount_;
  voxelSrvDesc.Buffer.StructureByteStride = sizeof(Voxel);
  device->CreateShaderResourceView(voxelBuffer_.Get(), &voxelSrvDesc,
                                    voxelSrvHandleCPU_);

  // Particleデータ用バッファ (UAV & SRV)
  particleBuffer_ =
      dxCommon->CreateUAVBufferResource(sizeof(VoxelParticle) * voxelCount_);

  // UAV
  uint32_t particleUavIndex = srvPool->Allocate();
  particleUavIndex_ = particleUavIndex;
  particleUavHandleCPU_ = srvPool->GetCPUHandle(particleUavIndex);
  particleUavHandleGPU_ = srvPool->GetGPUHandle(particleUavIndex);

  D3D12_UNORDERED_ACCESS_VIEW_DESC particleUavDesc{};
  particleUavDesc.Format = DXGI_FORMAT_UNKNOWN;
  particleUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
  particleUavDesc.Buffer.FirstElement = 0;
  particleUavDesc.Buffer.NumElements = voxelCount_;
  particleUavDesc.Buffer.StructureByteStride = sizeof(VoxelParticle);
  device->CreateUnorderedAccessView(particleBuffer_.Get(), nullptr,
                                     &particleUavDesc, particleUavHandleCPU_);

  // SRV
  uint32_t particleSrvIndex = srvPool->Allocate();
  particleSrvIndex_ = particleSrvIndex;
  particleSrvHandleCPU_ = srvPool->GetCPUHandle(particleSrvIndex);
  particleSrvHandleGPU_ = srvPool->GetGPUHandle(particleSrvIndex);

  D3D12_SHADER_RESOURCE_VIEW_DESC particleSrvDesc{};
  particleSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
  particleSrvDesc.Shader4ComponentMapping =
      D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  particleSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
  particleSrvDesc.Buffer.FirstElement = 0;
  particleSrvDesc.Buffer.NumElements = voxelCount_;
  particleSrvDesc.Buffer.StructureByteStride = sizeof(VoxelParticle);
  device->CreateShaderResourceView(particleBuffer_.Get(), &particleSrvDesc,
                                    particleSrvHandleCPU_);

  emitterBuffer_.Initialize(dxCommon);
  perFrameBuffer_.Initialize(dxCommon);
}

void VoxelParticleSystem::CreatePSO() {
  auto *dxCommon = engine_->GetDirectXCommon();
  auto *psoManager = dxCommon->GetPSOManager();

  // --- Compute PSO ---
  initializePSO_ = psoManager->GetComputePSO("VoxelParticleInitialize");
  emitPSO_ = psoManager->GetComputePSO("VoxelParticleEmit");
  updatePSO_ = psoManager->GetComputePSO("VoxelParticleUpdate");
  assert(initializePSO_ && emitPSO_ && updatePSO_);

  // --- Graphics PSO ---
  drawPSO_ = psoManager->GetPSO("VoxelParticle", BlendMode::kBlendModeNormal,
                                          PSOManager::DepthWrite::Enable,
                                          PSOManager::CullMode::Back);
  assert(drawPSO_);
}

void VoxelParticleSystem::Debug([[maybe_unused]] const char *name) {

#ifdef USE_IMGUI

    if (ImGui::TreeNode(name)) {
        ImGui::Text("Voxel Count: %u", voxelCount_);
        ImGui::DragFloat3("Emit Position", &emitterData_.emitPosition.x, 0.1f);
        if (ImGui::Button("Emit")) {
            Emit(emitterData_.emitPosition);
        }

        ImGui::Separator();
        ImGui::Text("Emitter Settings");
        ImGui::DragFloat("Life Time", &emitterData_.lifeTime, 0.1f, 0.1f, 10.0f);
        ImGui::DragFloat("Gravity", &emitterData_.gravity, 0.1f, -20.0f, 20.0f);
        ImGui::DragFloat("Dispersion", &emitterData_.dispersion, 0.1f, 0.0f, 20.0f);
        ImGui::DragFloat("Convergence", &emitterData_.convergence, 0.1f, 0.0f,
            20.0f);

        ImGui::TreePop();
    }

#endif // USE_IMGUI

}

void VoxelParticleSystem::SetParameters(const VoxelEmitterParams& params) {
    emitterData_.lifeTime = params.lifeTime;
    emitterData_.gravity = params.gravity;
    emitterData_.dispersion = params.dispersion;
    emitterData_.convergence = params.convergence;
    emitterData_.particleType = static_cast<uint32_t>(params.particleType);
}
