#pragma once

#include "Sound.h"
#include "VoiceInstance.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

// IXAudio2SourceVoice構造体を前方宣言
struct IXAudio2SourceVoice;

/**
 * @class AudioManager
 * @brief XAudio2 を使用した音声再生とリソース管理を行うマネージャクラス
 * @details サウンドデータのロード、再生中のボイス（VoiceInstance）の管理、およびカテゴリごとの整理を行います。
 */
class AudioManager {
private:
    // コピー禁止
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    // XAudio2のコアインターフェース
    IXAudio2* pXAudio2_{ nullptr };
    IXAudio2MasteringVoice* pMasteringVoice_{ nullptr };

    // ロードした音声データをファイル名をキーにして保持するマップ
    std::map<std::string, std::shared_ptr<Sound>> soundRegistry_;

    // 再生中の VoiceInstance を一元管理
    std::vector<std::shared_ptr<VoiceInstance>> activeVoices_;

    // カテゴリ名 → その中にあるファイル名リスト(ソート済み)
    std::map<std::string, std::vector<std::string>> categoryMap_;

    // ファイナライズ済みフラグ
    bool finalized_{ false };

    /**
     * @brief 管理対象のボイスかどうか判定する
     */
    bool IsManagedVoice(std::shared_ptr<VoiceInstance> instance) const;

public:
    /**
     * @brief コンストラクタ
     */
    AudioManager() = default;

    /**
     * @brief デストラクタ
     */
    ~AudioManager();

    /**
     * @brief XAudio2 エンジンの初期化
     */
    void Initialize();

    /**
     * @brief 終了処理
     * @details すべての再生中ボイスを停止し、リソースを解放します。
     */
    void Finalize();

    /**
     * @brief Media Foundation の開始処理
     */
    void StartUp();

    /**
     * @brief 指定フォルダから対応する音声ファイルをすべてロードする
     * @param[in] folderPath ロード対象のフォルダパス
     */
    void LoadAllSoundsFromFolder(const std::string& folderPath);

    /**
     * @brief サブフォルダをカテゴリとしてロードする
     * @param[in] folderPath ロード対象のパス
     * @param[in] category カテゴリ名
     */
    void LoadSoundsFromFolder(const std::string& folderPath, const std::string& category);

    /**
     * @brief カテゴリ内のサウンド名一覧を取得（ソート済み）
     */
    std::vector<std::string> GetSoundNames(const std::string& category) const;

    /**
     * @brief ロード済みのサウンドデータを取得
     */
    std::shared_ptr<Sound> GetSoundData(const std::string& name) const;

    /**
     * @brief 利用可能なサウンドカテゴリ一覧を取得
     */
    std::vector<std::string> GetCategories() const;

    /**
     * @brief 毎フレームの更新処理
     * @details 再生が終了したボイスのクリーンアップなどを行います。
     */
    void Update();

    /**
     * @brief サウンドを再生する
     * @param[in] soundData ロード済みのサウンドデータ
     * @param[in] loop ループ再生するか
     * @param[in] volume 音量 (0.0 ～ 1.0)
     * @return 再生中インスタンスへの弱参照。操作が必要な場合に保持してください。
     */
    std::weak_ptr<VoiceInstance> Play(
        std::shared_ptr<Sound> soundData, bool loop = false, float volume = 1.0f);

    /**
     * @brief 再生中のサウンドを停止する
     * @param[in] instance 停止させたいインスタンスの弱参照
     */
    void Stop(std::weak_ptr<VoiceInstance>& instance);

    /**
     * @brief すべての再生中サウンドを強制停止する
     */
    void StopAll();

    /**
     * @brief すべての再生中サウンドを一時停止する
     */
    void PauseAll();

    /**
     * @brief すべての一時停止中のサウンドを再開する
     */
    void ResumeAll();

    /**
     * @brief 重複を避けてファイルからロードまたは取得する
     * @param[in] filePath ファイルパス
     * @param[in] key 識別キー（省略時はファイルパスをキーにする）
     * @return サウンドデータへの共有ポインタ
     */
    std::shared_ptr<Sound> GetOrLoadSoundByFile(const std::string& filePath, const std::string& key = "");

    /**
     * @brief 指定キーのサウンドがロード済みか確認する
     */
    bool HasSound(const std::string& key) const;
};