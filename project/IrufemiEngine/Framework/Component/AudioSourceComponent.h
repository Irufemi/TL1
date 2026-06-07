#pragma once
#include "Component.h"
#include <string>
#include <memory>

class VoiceInstance;

/**
 * @class AudioSourceComponent
 * @brief 音声再生（BGM/SE）を管理するコンポーネント
 */
class AudioSourceComponent : public Component {
public:
    AudioSourceComponent() = default;
    ~AudioSourceComponent() override;

    void Initialize() override;
    void Update() override;
    
    std::string GetComponentName() const override { return "AudioSourceComponent"; }
    void OnRegisterProperties() override;

    /** @brief 音声を再生する */
    void Play();
    /** @brief 音声を停止する */
    void Stop();

    void SetAudioPath(const std::string& path);
    void SetVolume(float volume);
    void SetLoop(bool loop);

private:
    std::string audioPath_ = "audio/BGM/bgm_default.wav"; // デフォルト
    bool playOnAwake_ = false;
    bool loop_ = false;
    float volume_ = 1.0f;
    
    std::weak_ptr<VoiceInstance> voice_;
};
