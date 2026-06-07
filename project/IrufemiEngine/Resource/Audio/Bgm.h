#pragma once

#include "AudioManager.h"
#include <string>
#include <memory>

class Bgm {
private:
    std::weak_ptr<VoiceInstance> voice_;
    float volume_ = 0.1f;
    int selectedCat_ = 0;
    int selectedTrack_ = 0;
    static AudioManager* audioManager_;

    // fixedMode_ を廃止しました(currentSound_ の有無で判定します)
    std::string soundKey_;   // UI 用に保持(必須ではない)
    bool fixedLoop_ = true;  // 固定再生時のループ設定

    // 現在参照している音源データを保持(AudioManager の shared_ptr)
    std::shared_ptr<Sound> currentSound_ = nullptr;

public:
    Bgm() = default;
    ~Bgm();

    // 初期化: ファイル指定で音源をセット(AudioManager に未登録ならロードしてキャッシュ)
    // audioManager は事前に Bgm::SetAudioManager(...) でセットしておいてください
    void Initialize(const std::string& filePath, const std::string& key = "", bool loop = true);

    // ランタイムで音源を差し替える
    // ファイルから(未登録ならロード)差し替え
    bool SetSourceByFile(const std::string& filePath, const std::string& key = "", bool loop = true);

    // 登録済みのキーから差し替え
    bool SetSourceByKey(const std::string& key, bool loop = true);

    // 現在の音源をクリア(再生停止)
    void ClearSource();

    // 更新
    void Update();

    // デバッグ
    void Debug();

    void Play(const std::string& category, const std::string& track, bool loop = true);
    void PlayFirstTrack(); // 最初のカテゴリと最初のトラックで再生
    void Stop();
    void Pause();
    void Resume();

    // 固定トラック(currentSound_)を再生
    void PlayFixed();

    void SetVolume(float volume);
    const float& GetVolume() const { return volume_; }

    const int& GetSelectedCategoryIndex() const { return selectedCat_; }
    const int& GetSelectedTrackIndex() const { return selectedTrack_; }
    void SetSelectedCategoryIndex(int index) { selectedCat_ = index; }
    void SetSelectedTrackIndex(int index) { selectedTrack_ = index; }

    static void SetAudioManager(AudioManager* audioManager) { audioManager_ = audioManager; }
};