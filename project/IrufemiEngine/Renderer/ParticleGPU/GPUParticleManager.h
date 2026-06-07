#pragma once
#include "GPUParticleSystem.h"
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include "../../Engine/Core/Type/BlendMode.h"

class GPUParticleManager {
public:
    static GPUParticleManager* GetInstance();

    void Initialize();
    void Update();
    void Draw();
    void Finalize();
    void Debug();

    struct EmitterHandle {
        GPUParticleSystem* system = nullptr;
        uint32_t emitterIndex = 0xFFFFFFFF;
        bool IsValid() const { return system != nullptr && emitterIndex != 0xFFFFFFFF; }
    };

    /**
     * @brief 指定したテクスチャ、ブレンドモード、タイムスケール設定に対するシステムを取得し、エミッターを登録する
     */
    EmitterHandle RegisterEmitter(const std::string& texturePath, BlendMode blendMode, bool isUnscaledTime);

    /**
     * @brief 登録したエミッターを解放する
     */
    void UnregisterEmitter(const EmitterHandle& handle);

    /**
     * @brief エミッターデータを更新する
     */
    void UpdateEmitterData(const EmitterHandle& handle, const GPUParticleEmitter& data);

private:
    GPUParticleManager() = default;
    ~GPUParticleManager() = default;
    GPUParticleManager(const GPUParticleManager&) = delete;
    GPUParticleManager& operator=(const GPUParticleManager&) = delete;

    struct SystemContext {
        std::unique_ptr<GPUParticleSystem> system;
        std::vector<uint32_t> freeIndices;
        uint32_t nextIndex = 0;
    };

    struct SystemKey {
        std::string texturePath;
        BlendMode blendMode;
        bool isUnscaledTime;

        bool operator==(const SystemKey& other) const {
            return texturePath == other.texturePath && 
                   blendMode == other.blendMode && 
                   isUnscaledTime == other.isUnscaledTime;
        }
    };

    struct SystemKeyHasher {
        std::size_t operator()(const SystemKey& k) const {
            std::size_t h1 = std::hash<std::string>()(k.texturePath);
            std::size_t h2 = std::hash<int>()(static_cast<int>(k.blendMode));
            std::size_t h3 = std::hash<bool>()(k.isUnscaledTime);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };

    std::unordered_map<SystemKey, SystemContext, SystemKeyHasher> systems_;
};
