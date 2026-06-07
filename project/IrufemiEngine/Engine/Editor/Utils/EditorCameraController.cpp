#include "EditorCameraController.h"
#include "Engine/Core/Math/MathFunction.h"
#include <windows.h> // VK_LSHIFT, VK_RSHIFT

void EditorCameraController::UpdateCameraInput(Camera* camera, InputManager* input) {
    if (!camera || !input) return;

    bool isMiddleButtonDown = input->IsMouseButtonDown(Mouse::Button::Middle);
    auto* keyboard = input->GetKeyboard();
    bool isShiftDown = keyboard->IsKeyDown(VK_LSHIFT) || keyboard->IsKeyDown(VK_RSHIFT);
    Vector2 mouseDelta = input->GetMouseDelta();
    
    // 初期化されていない場合は現在のカメラからTarget等を逆算する
    if (!isInitialized_) {
        target_ = {0.0f, 0.0f, 0.0f};
        distance_ = 50.0f;
        
        Matrix4x4 rotMat = Math::MakeRotateXYZMatrix(camera->GetRotate());
        Vector3 offset = { 0.0f, 0.0f, -distance_ };
        offset = Math::TransformNormal(offset, rotMat);
        target_ = Math::Subtract(camera->GetTranslate(), offset);
        
        isInitialized_ = true;
    }
    
    bool cameraChanged = false;
    
    if (isMiddleButtonDown) {
        if (isShiftDown) {
            // パン操作 (Shift + 中ボタンドラッグ)
            const float panSpeed = 0.05f;
            Matrix4x4 viewInverse = Math::Inverse(camera->GetViewMatrix());
            Vector3 right = { viewInverse.m[0][0], viewInverse.m[0][1], viewInverse.m[0][2] };
            Vector3 up = { viewInverse.m[1][0], viewInverse.m[1][1], viewInverse.m[1][2] };
            target_ = Math::Add(target_, Math::Multiply(-panSpeed * mouseDelta.x, right));
            target_ = Math::Add(target_, Math::Multiply(panSpeed * mouseDelta.y, up));
            cameraChanged = true;
        } else {
            // オービット操作 (中ボタンドラッグ)
            const float rotationSpeed = 0.005f;
            Vector3 rotate = camera->GetRotate();
            rotate.y += mouseDelta.x * rotationSpeed;
            rotate.x += mouseDelta.y * rotationSpeed;
            // X軸回転を制限
            rotate.x = Math::Clamp(rotate.x, -Math::PIDiv2, Math::PIDiv2);
            camera->SetRotate(rotate);
            cameraChanged = true;
        }
    }
    
    // ズーム操作 (マウスホイール)
    float wheelDelta = input->GetMouseWheelDelta();
    if (wheelDelta != 0.0f) {
        const float zoomSpeed = 2.0f;
        distance_ -= wheelDelta * zoomSpeed;
        if (distance_ < 1.0f) distance_ = 1.0f; // 最小距離制限
        cameraChanged = true;
    }
    
    // カメラの位置を更新
    if (cameraChanged || isMiddleButtonDown) {
        Vector3 rotate = camera->GetRotate();
        Matrix4x4 rotMat = Math::MakeRotateXYZMatrix(rotate);
        Vector3 offset = { 0.0f, 0.0f, -distance_ };
        offset = Math::TransformNormal(offset, rotMat);
        camera->SetTranslate(Math::Add(target_, offset));
        camera->UpdateMatrix();
    }
}
