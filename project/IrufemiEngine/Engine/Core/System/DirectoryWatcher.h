#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <filesystem>

/**
 * @class DirectoryWatcher
 * @brief 指定したディレクトリ（サブディレクトリ含む）のファイル変更を監視するクラス
 * @details Windows API (ReadDirectoryChangesW) を用いてバックグラウンドスレッドで監視を行い、
 *          ファイル追加・削除・変更・リネームが発生した際にコールバックを発火します。
 */
class DirectoryWatcher {
public:
    /**
     * @brief コンストラクタ
     * @param targetDirectory 監視対象のディレクトリパス
     * @param onChangeCallback 変更検知時に呼ばれるコールバック関数
     */
    DirectoryWatcher(const std::filesystem::path& targetDirectory, std::function<void()> onChangeCallback);

    /**
     * @brief デストラクタ
     * @details バックグラウンドスレッドを安全に終了・破棄します。
     */
    ~DirectoryWatcher();

private:
    void WatchLoop();

    std::filesystem::path targetDirectory_;
    std::function<void()> onChangeCallback_;
    std::atomic<bool> isRunning_;
    std::thread workerThread_;
    void* directoryHandle_ = nullptr; // HANDLE (windows.hのインクルード漏れを防ぐためvoid*で保持)
};
