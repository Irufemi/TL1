#include "VoxelParticleManager.h"
#include "Engine/IrufemiEngine.h"
#include "Engine/Graphics/Pipeline/PSOManager.h"

void VoxelParticleManager::Initialize(IrufemiEngine* engine) {
    engine_ = engine;
}

void VoxelParticleManager::ReservePool(const std::string& modelName, const Vector3Int& resolution, int poolSize) {
    auto& poolData = pools_[modelName];
    int currentCount = static_cast<int>(poolData.systems.size());
    int addCount = poolSize - currentCount;

    for (int i = 0; i < addCount; ++i) {
        auto voxel = std::make_unique<VoxelParticleSystem>();
        voxel->Initialize(modelName, resolution);
        // バックグラウンドロードを行いつつプールに追加
        poolData.systems.push_back(std::move(voxel));
        totalSystemCount_++;
    }
}

VoxelParticleSystem* VoxelParticleManager::AllocateSystem(const std::string& modelName) {
    auto& poolData = pools_[modelName];
    size_t size = poolData.systems.size();

    if (size > 0) {
        // 1. 最優先：ラウンドロビン方式でロード完了かつ非アクティブなシステムを探す
        size_t startIdx = poolData.nextSearchIndex;
        for (size_t i = 0; i < size; ++i) {
            size_t idx = (startIdx + i) % size;
            auto& sys = poolData.systems[idx];
            if (sys->GetStatus() == VoxelParticleSystem::LoadingStatus::Loaded && !sys->IsActive()) {
                poolData.nextSearchIndex = (idx + 1) % size; // 次回はこの次のインデックスから探す
                return sys.get();
            }
        }

        // 2. 準優先：すべて使用中の場合、一番古いものを上書き再利用する
        VoxelParticleSystem* oldestSystem = nullptr;
        float maxTime = -1.0f;
        size_t oldestIdx = 0;
        for (size_t i = 0; i < size; ++i) {
            auto& sys = poolData.systems[i];
            if (sys->GetStatus() == VoxelParticleSystem::LoadingStatus::Loaded) {
                float t = sys->GetEmitterTime();
                if (t > maxTime) {
                    maxTime = t;
                    oldestSystem = sys.get();
                    oldestIdx = i;
                }
            }
        }
        if (oldestSystem) {
            poolData.nextSearchIndex = (oldestIdx + 1) % size;
            return oldestSystem;
        }
    }

    // 3. フォールバック：ロード完了しているものが1つも存在しない場合のみ、安全上限（60）を越えない範囲で新規生成を許可
    if (totalSystemCount_ < 60) {
        auto voxel = std::make_unique<VoxelParticleSystem>();
        voxel->Initialize(modelName, {32, 32, 32}); 
        poolData.systems.push_back(std::move(voxel));
        totalSystemCount_++;
        return poolData.systems.back().get();
    }
    
    // 安全上限に達しており、かつロード完了したものがない場合は nullptr を返して発生を諦める
    return nullptr;
}

void VoxelParticleManager::Update(float deltaTime) {
    for (auto& pair : pools_) {
        for (auto& sys : pair.second.systems) {
            auto status = sys->GetStatus();
            if (sys->IsActive() || 
                status == VoxelParticleSystem::LoadingStatus::Pending ||
                status == VoxelParticleSystem::LoadingStatus::Loading ||
                status == VoxelParticleSystem::LoadingStatus::ReadyToCreateResources) {
                sys->Update(deltaTime);
            }
        }
    }
}

void VoxelParticleManager::Draw() {
    for (auto& pair : pools_) {
        for (auto& sys : pair.second.systems) {
            if (sys->IsActive()) {
                engine_->SetBlend(BlendMode::kBlendModeNormal);
                engine_->SetDepthWrite(PSOManager::DepthWrite::Enable);
                engine_->SetCull(PSOManager::CullMode::Back);
                sys->Draw();
            }
        }
    }
}

void VoxelParticleManager::PlayExplosion(const std::string& modelName, 
                                         const Vector3& position, 
                                         const Vector3& velocity, 
                                         const Vector3& rotate, 
                                         const Vector3& scale,
                                         VoxelParticleSystem::ParticleType type) {
    auto system = AllocateSystem(modelName);
    if (system) {
        system->SetParticleType(type);
        if (type == VoxelParticleSystem::ParticleType::Building) {
            system->SetGravity(40.0f);
        }
        system->Explode(position, velocity, rotate, scale);
    }
}

void VoxelParticleManager::PlayCollisionScatter(const std::string& modelName, 
                                                const Vector3& position, 
                                                const Vector3& velocity, 
                                                const Vector3& rotate, 
                                                const Vector3& scale, 
                                                const struct OBB& collisionArea,
                                                VoxelParticleSystem::ParticleType type) {
    auto system = AllocateSystem(modelName);
    if (system) {
        system->SetParticleType(type);
        if (type == VoxelParticleSystem::ParticleType::Building) {
            system->SetGravity(40.0f);
        }
        system->CollisionScatter(position, velocity, rotate, scale, collisionArea);
    }
}
