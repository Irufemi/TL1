#pragma once
#include <xaudio2.h>
#include <memory>
#include "VoiceCallback.h"

class VoiceInstance {
public:
    VoiceInstance(IXAudio2SourceVoice* voice, std::unique_ptr<VoiceCallback> callback)
        : voice_(voice), callback_(std::move(callback)) {}

    ~VoiceInstance() {
        if (voice_) {
            voice_->Stop(0);
            voice_->FlushSourceBuffers();
            voice_->DestroyVoice();
            voice_ = nullptr;
        }
    }

    void SetVolume(float volume) {
        if (voice_) {
            voice_->SetVolume(volume);
        }
    }

    void Stop() {
        if (voice_) {
            voice_->Stop(0);
        }
    }

    void Pause() {
        if (voice_) {
            voice_->Stop(0);
        }
    }

    void Resume() {
        if (voice_) {
            voice_->Start(0);
        }
    }

    IXAudio2SourceVoice* GetVoice() const { return voice_; }
    VoiceCallback* GetCallback() const { return callback_.get(); }

private:
    IXAudio2SourceVoice* voice_ = nullptr;
    std::unique_ptr<VoiceCallback> callback_;
};
