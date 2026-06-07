#include "AudioManager.h"
#include <cassert>
#include <filesystem> // フォルダ内のファイルを探索するために使用
#include <algorithm>  // 文字列を小文字に変換するために使用
#include <Windows.h>

#pragma comment (lib,"xaudio2.lib")
#pragma comment(lib, "Mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "Mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

namespace {
    // UTF-8 -> UTF-16
    std::wstring ToWide(const std::string& s) {
        if (s.empty()) return {};
        int size = ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
        std::wstring w(size, L'\0');
        ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), size);
        return w;
    }
    // パス正規化(キーとして安定化させる)
    std::string NormalizePath(const std::string& path) {
        std::filesystem::path p(path);
        p.make_preferred();
        return p.generic_string(); // すべて'/'に統一
    }
}

AudioManager::~AudioManager() {
    Finalize();
}

void AudioManager::Initialize() {
    // 再初期化に備えて
    finalized_ = false;
    // XAudio2エンジンの生成
    HRESULT hr = XAudio2Create(&pXAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
    assert(SUCCEEDED(hr));

    // マスターボイスの生成
    hr = pXAudio2_->CreateMasteringVoice(&pMasteringVoice_);
    assert(SUCCEEDED(hr));
}

//Media Foundationの初期化
void AudioManager::StartUp() {


    /*サウンド再生*/

    ///Microsoft Media Foundation

    //Media Foundationの初期化
    MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
}

void AudioManager::Finalize() {
    if (finalized_) return;      // 多重 Finalize 防止
    finalized_ = true;           // 以降の操作は無効化

    StopAll(); // すべてのVoiceを安全に停止＆Destroy

    if (pMasteringVoice_) {
        pMasteringVoice_->DestroyVoice();
        pMasteringVoice_ = nullptr;
    }
    if (pXAudio2_) {
        pXAudio2_->Release();
        pXAudio2_ = nullptr;
    }
    HRESULT hr = MFShutdown();
    assert(SUCCEEDED(hr));
}

bool AudioManager::IsManagedVoice(std::shared_ptr<VoiceInstance> instance) const {
    return std::find(activeVoices_.begin(), activeVoices_.end(), instance) != activeVoices_.end();
}

void AudioManager::Update() {
    // 終了したボイスをリストから削除
    // 削除されると shared_ptr の参照が外れ、VoiceInstance のデストラクタで DestroyVoice される
    activeVoices_.erase(
        std::remove_if(activeVoices_.begin(), activeVoices_.end(),
            [](const std::shared_ptr<VoiceInstance>& instance) {
                return instance->GetCallback()->IsFinished();
            }),
        activeVoices_.end());
}

void AudioManager::LoadAllSoundsFromFolder(const std::string& folderPath) {
    soundRegistry_.clear();
    categoryMap_.clear();
    namespace fs = std::filesystem;
    for (const auto& entry : fs::directory_iterator(folderPath)) {
        if (entry.is_directory()) {
            std::string category = entry.path().filename().string();
            LoadSoundsFromFolder(entry.path().string(), category);
        }
    }
}

// サブフォルダ単位でロードするオーバーロード版
void AudioManager::LoadSoundsFromFolder(const std::string& folderPath, const std::string& category) {
    namespace fs = std::filesystem;

    for (const auto& entry : fs::directory_iterator(folderPath)) {
        if (!entry.is_regular_file()) continue;

        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext != ".wav" && ext != ".mp3" && ext != ".wma") continue;

        std::string filename = entry.path().filename().string();
        std::wstring wpath = entry.path().wstring();

        // キーは "カテゴリ/ファイル名" にする
        std::string key = category + "/" + filename;
        if (soundRegistry_.count(key) == 0) {
            auto sd = std::make_shared<Sound>();
            if (sd->Load(wpath)) {
                soundRegistry_[key] = sd;
                categoryMap_[category].push_back(filename);

            }

        }

    }
    // カテゴリごとにソート
    auto& names = categoryMap_[category];
    std::sort(names.begin(), names.end());

}

std::vector<std::string> AudioManager::GetSoundNames(const std::string& category) const {
    auto it = categoryMap_.find(category);
    if (it == categoryMap_.end()) return {};
    return it->second;
}

std::shared_ptr<Sound> AudioManager::GetSoundData(const std::string& name) const {
    auto it = soundRegistry_.find(name);
    if (it != soundRegistry_.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<std::string> AudioManager::GetCategories() const {
    std::vector<std::string> cats;
    cats.reserve(categoryMap_.size());
    for (auto const& [cat, _] : categoryMap_) {
        cats.push_back(cat);

    }
    std::sort(cats.begin(), cats.end());
    return cats;
}

std::weak_ptr<VoiceInstance> AudioManager::Play(std::shared_ptr<Sound> soundData, bool loop, float volume) {
    if (finalized_) return {};
    if (!pXAudio2_ || !soundData) {
        return {};
    }

    auto callback = std::make_unique<VoiceCallback>();
    IXAudio2SourceVoice* pSourceVoice{ nullptr };
    HRESULT hr = pXAudio2_->CreateSourceVoice(&pSourceVoice, soundData->GetFormat(), 0, XAUDIO2_DEFAULT_FREQ_RATIO, callback.get());
    if (FAILED(hr) || !pSourceVoice) {
        return {};
    }

    // 音量を設定
    pSourceVoice->SetVolume(volume);

    // 再生するオーディオバッファの準備
    XAUDIO2_BUFFER buffer{ 0 };
    buffer.pAudioData = soundData->GetData();
    buffer.Flags = XAUDIO2_END_OF_STREAM; // これで再生が最後まで行くとストリームが終了したとみなされる
    buffer.AudioBytes = soundData->GetSize();
    if (loop) {
        buffer.LoopCount = XAUDIO2_LOOP_INFINITE; // ループ再生
    }

    // バッファをソースボイスに送信
    hr = pSourceVoice->SubmitSourceBuffer(&buffer);
    assert(SUCCEEDED(hr));

    // 再生開始
    hr = pSourceVoice->Start(0);
    assert(SUCCEEDED(hr));

    // 管理インスタンスを生成してリストに追加
    auto instance = std::make_shared<VoiceInstance>(pSourceVoice, std::move(callback));
    activeVoices_.push_back(instance);
    return instance;
}

void AudioManager::Stop(std::weak_ptr<VoiceInstance>& instance) {
    auto locked = instance.lock();
    if (!locked) return;

    if (finalized_ || !IsManagedVoice(locked)) {
        return;
    }

    // 管理リストから除去 (shared_ptr が外れて VoiceInstance のデストラクタで破棄される)
    auto it = std::remove(activeVoices_.begin(), activeVoices_.end(), locked);
    if (it != activeVoices_.end()) {
        activeVoices_.erase(it, activeVoices_.end());
    }
}

void AudioManager::StopAll() {
    for (auto& voice : activeVoices_) {
        if (voice) {
            voice->Stop();
        }
    }
}

void AudioManager::PauseAll() {
    for (auto& voice : activeVoices_) {
        if (voice) {
            voice->Pause();
        }
    }
}

void AudioManager::ResumeAll() {
    for (auto& voice : activeVoices_) {
        if (voice) {
            voice->Resume();
        }
    }
}

bool AudioManager::HasSound(const std::string& key) const {
    return soundRegistry_.find(key) != soundRegistry_.end();
}

std::shared_ptr<Sound> AudioManager::GetOrLoadSoundByFile(const std::string& filePath, const std::string& key) {
    std::string normPath = NormalizePath(filePath);
    std::string useKey = key.empty() ? normPath : key;

    auto it = soundRegistry_.find(useKey);
    if (it != soundRegistry_.end()) {
        return it->second;
    }

    auto sound = std::make_shared<Sound>();
    if (!sound->Load(ToWide(normPath))) {
        // 読み込み失敗時は nullptr を返す
        return nullptr;
    }

    soundRegistry_.emplace(useKey, sound);
    return sound;
}