#pragma once

#include "Engine/Core/Math/Vector3.h"
#include "Engine/Platform/Input/InputManager.h"
#include "Engine/Graphics/Camera/Camera.h"

/**
 * @class DebugCamera
 * @brief デバッグ目的で自由に操作できるカメラ
 *
 * キーボード入力により、シーン内を自由に移動・回転できます。
 */
class DebugCamera {
private: //メンバ変数
    // カメラ注視点
    Vector3 target_{};
    // カメラ注視点までの距離(ピボット回転)
    float distance_{ 10.0f };
    // 入力クラスのポインタ
    InputManager* input_ = nullptr;
    // カメラ
    Camera camera_{};

public: //メンバ関数
    /**
     * @brief 初期化処理
     * @param input InputManagerのポインタ
     * @param windowWidth ウィンドウの幅
     * @param windowHeight ウィンドウの高さ
     */
    void Initialize(InputManager* input, int windowWidth, int windowHeight);

    /**
     * @brief 更新処理
     */
    void Update();

    /**
     * @brief デバッグ表示
     */
    void Debug();

    //ゲッター

    /**
     * @brief 内部で管理しているCameraオブジェクトを取得します
     * @return const Camera& カメラオブジェクト
     */
    const Camera& GetCamera() const { return camera_; }

    /**
     * @brief 内部で管理しているCameraオブジェクトを取得します
     * @return Camera& カメラオブジェクト
    */
    Camera& GetCamera() { return camera_; }
    
    /**
     * @brief プリセット視点の設定
     */
    enum class Preset {
        TopDown,     // 見下ろし
        Diagonal,    // 斜め見下ろし
        Front,       // 正面
        Current      // 現在のカメラに合わせる
    };
    void SetPreset(Preset preset, const Camera& mainCamera);

    // セッター/ゲッター
    void SetTarget(const Vector3& target) { target_ = target; }
    void SetDistance(float distance) { distance_ = distance; }
    float GetDistance() const { return distance_; }
};

