#include "AudioSourceComponent.h"
#include "../GameObject.h"
#include "../BaseScene.h"
#include "Engine/IrufemiEngine.h"
#include "Resource/Audio/AudioManager.h"

AudioSourceComponent::~AudioSourceComponent() {
    Stop();
}

void AudioSourceComponent::OnRegisterProperties() {
    RegisterProperty("Audio Path", &audioPath_);
    RegisterProperty("Play On Awake", &playOnAwake_);
    RegisterProperty("Loop", &loop_);
    RegisterProperty("Volume", &volume_);
}

void AudioSourceComponent::Initialize() {
    if (playOnAwake_) {
        Play();
    }
}

void AudioSourceComponent::Update() {
    // 毎フレームの処理（必要に応じて音量の動的追従などを追加）
}

void AudioSourceComponent::Play() {
    if (!gameObject_) return;
    auto scene = gameObject_->GetScene();
    if (!scene) return;
    auto engine = scene->GetEngine();
    if (!engine) return;

    auto audioManager = engine->GetAudioManager();
    
    // "resources/" プレフィックスを付けるか、そのまま使うか
    // IrufemiEngine の慣習に合わせて、ユーザーが "audio/..." と入力したものをそのまま使用可能にするため、
    // "resources/" に含まれていなければ付与するなどの工夫も考えられますが、一旦そのまま "resources/" 基準でロードします。
    std::string fullPath = "resources/" + audioPath_;
    if (audioPath_.find("resources/") == 0) {
        fullPath = audioPath_;
    }

    auto soundData = audioManager->GetOrLoadSoundByFile(fullPath);
    if (soundData) {
        voice_ = audioManager->Play(soundData, loop_, volume_);
    }
}

void AudioSourceComponent::Stop() {
    if (voice_.expired() || !gameObject_) return;
    auto scene = gameObject_->GetScene();
    if (!scene) return;
    auto engine = scene->GetEngine();
    if (!engine) return;

    engine->GetAudioManager()->Stop(voice_);
}

void AudioSourceComponent::SetAudioPath(const std::string& path) {
    audioPath_ = path;
}

void AudioSourceComponent::SetVolume(float volume) {
    volume_ = volume;
}

void AudioSourceComponent::SetLoop(bool loop) {
    loop_ = loop;
}
