#pragma once
#include <atomic>
#include <cstdint>

/**
 * @class TaskGroup
 * @brief 非同期タスクのグループ進捗（残数）を管理するクラス
 */
class TaskGroup {
public:
    TaskGroup() : pendingCount_(0) {}
    ~TaskGroup() = default;

    /**
     * @brief タスクの開始を通知（カウントアップ）
     */
    void NotifyTaskStarted() {
        pendingCount_.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief タスクの完了を通知（カウントダウン）
     */
    void NotifyTaskFinished() {
        // カウントが 0 の状態で呼ばれることがないよう注意が必要（通常は ThreadPool 側で保証）
        if (pendingCount_ > 0) {
            pendingCount_.fetch_sub(1, std::memory_order_release);
        }
    }

    /**
     * @brief 全てのタスクが完了したか確認
     * @return true: 全完了, false: 未完了タスクあり
     */
    bool IsAllDone() const {
        return pendingCount_.load(std::memory_order_acquire) == 0;
    }

    /**
     * @brief 現在の待機中タスク数を取得
     */
    uint32_t GetPendingCount() const {
        return pendingCount_.load(std::memory_order_relaxed);
    }

private:
    std::atomic<uint32_t> pendingCount_;
};
