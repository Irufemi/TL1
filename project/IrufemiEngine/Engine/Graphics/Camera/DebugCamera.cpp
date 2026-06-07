#define NOMINMAX
#include "DebugCamera.h"
#include "Engine/Platform/Input/Mouse.h"
#include "Engine/Platform/Input/Keyboard.h"
#include "Engine/Core/Math/Math.h"
#include <algorithm>
#include <string>

void DebugCamera::Initialize(InputManager* input, int windowWidth, int windowHeight) {
    input_ = input;
    camera_.Initialize(windowWidth, windowHeight);
    // 初期回転と位置を計算
    Vector3 rotate = camera_.GetRotate();
    Matrix4x4 rotMat = Math::MakeRotateXYZMatrix(rotate);
    Vector3 offset = { 0.0f, 0.0f, -distance_ };
    offset = Math::TransformNormal(offset, rotMat);
    camera_.SetTranslate(Math::Add(target_, offset));
    camera_.UpdateMatrix();
}

void DebugCamera::Update() {
    // 行列更新
    camera_.Update();

    bool isMiddleButtonDown = input_->IsMouseButtonDown(Mouse::Button::Middle);
    Keyboard* keyboard = input_->GetKeyboard();
    bool isShiftDown = keyboard->IsKeyDown(VK_LSHIFT) || keyboard->IsKeyDown(VK_RSHIFT);
    Vector2 mouseDelta = input_->GetMouseDelta();

    // Blenderライクな操作
    if (isMiddleButtonDown) {
        if (isShiftDown) {
            // パン操作 (Shift + 中ボタンドラッグ)
            const float panSpeed = 0.05f;
            Matrix4x4 viewInverse = Math::Inverse(camera_.GetViewMatrix());
            Vector3 right = { viewInverse.m[0][0], viewInverse.m[0][1], viewInverse.m[0][2] };
            Vector3 up = { viewInverse.m[1][0], viewInverse.m[1][1], viewInverse.m[1][2] };
            target_ = Math::Add(target_, Math::Multiply(-panSpeed * mouseDelta.x, right));
            target_ = Math::Add(target_, Math::Multiply(panSpeed * mouseDelta.y, up));
        }
        else {
            // オービット操作 (中ボタンドラッグ)
            const float rotationSpeed = 0.005f;
            Vector3 rotate = camera_.GetRotate();
            rotate.y += mouseDelta.x * rotationSpeed;
            rotate.x += mouseDelta.y * rotationSpeed;
            // X軸回転を制限
            rotate.x = std::clamp(rotate.x, -Math::PIDiv2, Math::PIDiv2);
            camera_.SetRotate(rotate);
        }
    }

    // ズーム操作 (マウスホイール)
    float wheelDelta = input_->GetMouseWheelDelta();
    if (wheelDelta != 0.0f) {
        // --- デバッグコード追加 ---
        std::string dbgMsg = "[DebugCamera] GetWheelDelta: " + std::to_string(wheelDelta) + ", old distance: " + std::to_string(distance_) + "\n";
        OutputDebugStringA(dbgMsg.c_str());
        // -------------------------

        const float zoomSpeed = 2.0f;
        distance_ -= wheelDelta * zoomSpeed;
        distance_ = std::max(distance_, 1.0f); // 最小距離制限

        // --- デバッグコード追加 ---
        dbgMsg = "[DebugCamera] new distance: " + std::to_string(distance_) + "\n";
        OutputDebugStringA(dbgMsg.c_str());
        // -------------------------
    }

    // カメラの位置を更新
    Vector3 rotate = camera_.GetRotate();
    Matrix4x4 rotMat = Math::MakeRotateXYZMatrix(rotate);
    Vector3 offset = { 0.0f, 0.0f, -distance_ };
    offset = Math::TransformNormal(offset, rotMat);
    camera_.SetTranslate(Math::Add(target_, offset));

    // マウス操作後の最終的な行列を更新
    camera_.UpdateMatrix();
}


void DebugCamera::SetPreset(Preset preset, const Camera& mainCamera) {
    switch (preset) {
    case Preset::TopDown:
        camera_.SetRotate({ Math::PIDiv2, 0.0f, 0.0f });
        target_ = { 0.0f, 0.0f, 0.0f };
        distance_ = 50.0f;
        break;
    case Preset::Diagonal:
        camera_.SetRotate({ 0.6f, 0.78f, 0.0f }); // 約35度見下ろし、45度回転
        target_ = { 0.0f, 0.0f, 0.0f };
        distance_ = 50.0f;
        break;
    case Preset::Front:
        camera_.SetRotate({ 0.0f, 0.0f, 0.0f });
        target_ = { 0.0f, 0.0f, 0.0f };
        distance_ = 50.0f;
        break;
    case Preset::Current: {
        // 現在のメインカメラの回転をコピー
        camera_.SetRotate(mainCamera.GetRotate());
        // メインカメラの位置にくるように target を調整
        Matrix4x4 rotMat = Math::MakeRotateXYZMatrix(camera_.GetRotate());
        Vector3 offset = { 0.0f, 0.0f, -distance_ };
        offset = Math::TransformNormal(offset, rotMat);
        target_ = Math::Subtract(mainCamera.GetTranslate(), offset);
        break;
    }
    }
    camera_.UpdateMatrix();
}

