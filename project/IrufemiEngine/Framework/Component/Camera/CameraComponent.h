#pragma once
#include "../Component.h"
#include "Engine/Graphics/Camera/Camera.h"
#include <memory>

/**
 * @class CameraComponent
 * @brief 3D空間のカメラ（視点）をGameObjectとして管理するためのコンポーネント
 * @details 自身の TransformComponent の位置・回転とカメラの行列情報を同期します。
 */
class CameraComponent : public Component {
public:
    CameraComponent();
    ~CameraComponent() override;

    void Initialize() override;
    void Update() override;

    std::string GetComponentName() const override { return "CameraComponent"; }

    // --- ゲッター・セッター ---
    std::shared_ptr<Camera> GetCamera() const { return camera_; }
    
    float GetFovAngleY() const { return fovAngleY_; }
    void SetFovAngleY(float fov) { fovAngleY_ = fov; }

    float GetNearZ() const { return nearZ_; }
    void SetNearZ(float nearZ) { nearZ_ = nearZ; }

    float GetFarZ() const { return farZ_; }
    void SetFarZ(float farZ) { farZ_ = farZ; }

    void OnRegisterProperties() override;

protected:

private:
    std::shared_ptr<Camera> camera_ = nullptr; ///< 内包するカメラオブジェクト
    
    // シリアライズ・インスペクター編集用プロパティ
    float fovAngleY_ = 45.0f * 3.141592654f / 180.0f; ///< 垂直視野角 (ラジアン)
    float nearZ_ = 0.1f;                             ///< 近クリップ面
    float farZ_ = 1000.0f;                           ///< 遠クリップ面
    bool makeActive_ = true;                         ///< 起動時に自動でアクティブカメラにするか
};
