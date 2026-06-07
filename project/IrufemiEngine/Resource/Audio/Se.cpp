#include "Se.h"
#include <cassert>

AudioManager* Se::audioManager_ = nullptr;

Se::~Se() {
    Stop();
    sound_.reset();
}

void Se::Initialize(const std::string& filePath, const std::string& key, float volume, bool loop, bool autoPlay) {
    Stop();
    sound_.reset();
    soundKey_.clear();
    volume_ = volume;

    if (!audioManager_) return;

    auto sd = audioManager_->GetOrLoadSoundByFile(filePath, key);
    if (!sd) return;

    sound_ = sd;
    soundKey_ = key.empty() ? filePath : key;

    if (autoPlay) Play(loop);
}

bool Se::SetSourceByFile(const std::string& filePath, const std::string& key, float volume) {
    if (!audioManager_) return false;
    auto sd = audioManager_->GetOrLoadSoundByFile(filePath, key);
    if (!sd) return false;

    Stop();
    sound_ = sd;
    soundKey_ = key.empty() ? filePath : key;
    volume_ = volume;
    return true;
}

bool Se::SetSourceByKey(const std::string& key, float volume) {
    if (!audioManager_) return false;
    auto sd = audioManager_->GetSoundData(key);
    if (!sd) return false;

    Stop();
    sound_ = sd;
    soundKey_ = key;
    volume_ = volume;
    return true;
}

void Se::Play(bool loop) {
    if (!audioManager_ || !sound_) return;
    // 再生開始前に既存の再生を止める(同一インスタンスで再生を上書き)
    Stop();
    voice_ = audioManager_->Play(sound_, loop, volume_);
}

void Se::Stop() {
    if (audioManager_) {
        audioManager_->Stop(voice_);
    }
}

void Se::Pause() {
    if (auto v = voice_.lock()) {
        v->Pause();
    }
}

void Se::Resume() {
    if (auto v = voice_.lock()) {
        v->Resume();
    }
}

void Se::SetVolume(float volume) {
    volume_ = volume;
    if (auto v = voice_.lock()) {
        v->SetVolume(volume_);
    }
}
