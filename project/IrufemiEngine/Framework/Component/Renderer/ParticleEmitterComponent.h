#pragma once
#include "../Component.h"
#include <string>
#include <memory>
#include "Renderer/ParticleGPU/ParticleObject.h"

class TransformComponent;

/**
 * @class ParticleEmitterComponent
 * @brief エディタ用コンポーネント。実際の描画・ロジックは内部の ParticleObject に委譲します。
 */
class ParticleEmitterComponent : public Component {
public:
    ParticleEmitterComponent();
    ~ParticleEmitterComponent() override;

    void Initialize() override;
    void Update() override;
    void Draw() override;
    
    IRenderable* GetRenderable() override { return nullptr; }

    std::string GetComponentName() const override { return "ParticleEmitterComponent"; }
    void OnRegisterProperties() override;

    nlohmann::json Serialize() override;
    void Deserialize(const nlohmann::json& j) override;

    void Play();
    void Stop();
    
    void EmitBurst(int count);

    // エディタや他スクリプトから実体へアクセスするためのゲッター
    ParticleObject* GetParticleObject() const { return particleObj_.get(); }

private:
    std::unique_ptr<ParticleObject> particleObj_;
    TransformComponent* transform_ = nullptr;
};
