#pragma once
#include <string>
#include "Engine/Core/Math/Vector3.h"
#include "Engine/Core/Math/Vector4.h"
#include "Renderer/ParticleGPU/GPUParticleManager.h"

#include "Engine/Core/Type/BlendMode.h"
#include <nlohmann/json.hpp>

class TextureManager;

/**
 * @class ParticleObject
 * @brief C++べた書きでGPUパーティクルを制御するための描画オブジェクトラッパー
 */
class ParticleObject {
public:
    ParticleObject();
    ~ParticleObject();

    void Initialize();
    void Play();
    void Stop();
    void EmitBurst(int count);
    void Update();
    void DebugUI(const char* name = "Particle Object");

    void Serialize(nlohmann::json& j) const;
    void Deserialize(const nlohmann::json& j);
    bool LoadFromJson(const std::string& filepath);

    // トランスフォーム
    Vector3 position_ = { 0.0f, 0.0f, 0.0f };
    Vector3 rotation_ = { 0.0f, 0.0f, 0.0f }; // 現在は主に方向として使用

    std::string texturePath_ = "resources/circle.png";
    BlendMode blendMode_ = BlendMode::kBlendModeAdd;
    bool isUnscaledTime_ = false;
    bool emitOnAwake_ = true;
    
    // エミッターの基本パラメータ
    int emitType_ = 0; // 0: Sphere, 1: Beam, 2: Box, 3: Cylinder
    float emissionRate_ = 50.0f; // 1秒あたりの発生数
    float lifeTimeMin_ = 0.5f;
    float lifeTimeMax_ = 1.0f;
    float velocity_ = 1.0f;
    float radius_ = 1.0f;
    float spread_ = 0.1f;
    
    // アニメーション設定
    int atlasRows_ = 1;
    int atlasCols_ = 1;

    // 物理・挙動パラメータ
    float gravity_ = 0.0f;
    float damping_ = 0.0f;
    float bounce_ = 0.0f;          // 床でのバウンド係数
    float groundHeight_ = -100.0f; // 床のY座標
    float attractorStrength_ = 0.0f; // 吸引力
    Vector3 attractorPos_ = {0.0f, 0.0f, 0.0f}; // 吸引位置
    float jitter_ = 0.0f;          // 不規則なブレ

    // ビジュアル・ライフタイム
    int billboardMode_ = 1; // 0: None, 1: Billboard, 2: Y-Axis
    Vector4 color_ = { 1.0f, 1.0f, 1.0f, 1.0f };
    Vector4 midColor_ = { 1.0f, 1.0f, 1.0f, 1.0f }; // 中間色
    Vector3 startScale_ = { 1.0f, 1.0f, 1.0f };
    Vector3 midScale_ = { 1.0f, 1.0f, 1.0f };       // 中間スケール
    Vector3 endScale_ = { 0.0f, 0.0f, 0.0f };
    float midPoint_ = 0.5f;                         // 中間点の位置(0.0~1.0)
    
    Vector3 direction_ = { 0.0f, 0.0f, 1.0f };
    Vector3 areaSize_ = { 10.0f, 10.0f, 10.0f };    // Boxエミッター用サイズ

    static void SetTextureManager(TextureManager* tm) { textureManager_ = tm; }
    static TextureManager* GetTextureManager() { return textureManager_; }

    void MarkDirty() { isDirty_ = true; }

private:
    void UpdateSystem();

    static TextureManager* textureManager_;
    GPUParticleManager::EmitterHandle emitterHandle_;
    bool isPlaying_ = false;
    int burstCountPending_ = 0;
    bool isDirty_ = true; // パラメータ変更検知フラグ
};
