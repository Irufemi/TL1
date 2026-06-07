#include "FrameRateController.h"
#include <thread>

/**
 * @brief 初期化
 * @details フレーム計測の基準時間を現在に設定します。
 */
void FrameRateController::Initialize() {
    reference_ = std::chrono::steady_clock::now();
}

/**
 * @brief フレームの更新（待機処理）
 * @details 前回のフレームから1/60秒（16.66ms）経過するまでスリープを行います。
 */
void FrameRateController::Update() {
    // 基準時間の更新（1/60秒）
    const std::chrono::microseconds kMinTime(uint64_t(1000000.0f / 60.0f));

    // 現在の時間を取得
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    // 前回の時間からの経過時間を取得
    std::chrono::microseconds elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - reference_);

    // 1 / 60秒たっていない最は待機する
    if (elapsed < kMinTime) {
        // 微調整
        while (std::chrono::steady_clock::now() - reference_ < kMinTime) {
            // 余裕があるならスリープ
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
    }

    // 基準時間を更新
    reference_ = std::chrono::steady_clock::now();
}
