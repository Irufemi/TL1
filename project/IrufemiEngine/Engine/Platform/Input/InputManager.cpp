#include "InputManager.h"

void InputManager::Initialize(HWND hwnd) {
    hwnd_ = hwnd;
    keyboard_ = std::make_unique<Keyboard>();
    gamepad_ = std::make_unique<GamePad>();
    mouse_ = std::make_unique<Mouse>();
    keyboard_->Initialize();
    gamepad_->Initialize();
    mouse_->Initialize(hwnd);
}

void InputManager::Update() {
    if (GetForegroundWindow() != hwnd_) {
        // バックグラウンドにいるときは全入力をクリアして更新をスキップ
        keyboard_->Clear();
        gamepad_->Clear();
        mouse_->Clear();
        return;
    }

    keyboard_->Update();
    gamepad_->Update();
    mouse_->Update();
}

// --- 旧APIフォワード(互換維持)---
bool InputManager::IsKeyDown(uint8_t k)     const { return keyboard_->IsKeyDown(k); }
bool InputManager::IsKeyUp(uint8_t k)       const { return keyboard_->IsKeyUp(k); }
bool InputManager::IsKeyPressed(uint8_t k)  const { return keyboard_->IsKeyPressed(k); }
bool InputManager::IsKeyReleased(uint8_t k) const { return keyboard_->IsKeyReleased(k); }

bool InputManager::IsButtonDown(WORD b)     const { return gamepad_->IsButtonDown(b); }
bool InputManager::IsButtonUp(WORD b)       const { return gamepad_->IsButtonUp(b); }
bool InputManager::IsButtonPressed(WORD b)  const { return gamepad_->IsButtonPressed(b); }
bool InputManager::IsButtonReleased(WORD b) const { return gamepad_->IsButtonReleased(b); }

float InputManager::GetLeftStickX()  const { return gamepad_->GetLeftStickX(); }
float InputManager::GetLeftStickY()  const { return gamepad_->GetLeftStickY(); }
float InputManager::GetRightStickX() const { return gamepad_->GetRightStickX(); }
float InputManager::GetRightStickY() const { return gamepad_->GetRightStickY(); }
float InputManager::GetLeftTrigger() const { return gamepad_->GetLeftTrigger(); }
float InputManager::GetRightTrigger()const { return gamepad_->GetRightTrigger(); }

bool InputManager::IsKeyDownDIK(uint8_t d)     const { return keyboard_->IsKeyDownDIK(d); }
bool InputManager::IsKeyUpDIK(uint8_t d)       const { return keyboard_->IsKeyUpDIK(d); }
bool InputManager::IsKeyPressedDIK(uint8_t d)  const { return keyboard_->IsKeyPressedDIK(d); }
bool InputManager::IsKeyReleasedDIK(uint8_t d) const { return keyboard_->IsKeyReleasedDIK(d); }

// START フォワード
bool InputManager::StartDown() const { return gamepad_->StartDown(); }
bool InputManager::StartPressed() const { return gamepad_->StartPressed(); }
bool InputManager::StartReleased() const { return gamepad_->StartReleased(); }

// --- D-Pad フォワード ---
bool InputManager::DPadUp() const { return gamepad_->DPadUp(); }
bool InputManager::DPadDown() const { return gamepad_->DPadDown(); }
bool InputManager::DPadLeft() const { return gamepad_->DPadLeft(); }
bool InputManager::DPadRight() const { return gamepad_->DPadRight(); }
bool InputManager::DPadUpPressed() const { return gamepad_->DPadUpPressed(); }
bool InputManager::DPadDownPressed() const { return gamepad_->DPadDownPressed(); }
bool InputManager::DPadLeftPressed() const { return gamepad_->DPadLeftPressed(); }
bool InputManager::DPadRightPressed() const { return gamepad_->DPadRightPressed(); }

// --- マウスAPIフォワード ---
bool InputManager::IsMouseButtonDown(Mouse::Button button) const {
    return mouse_->IsButtonDown(button);
}

bool InputManager::IsMouseButtonPressed(Mouse::Button button) const {
    return mouse_->IsButtonPressed(button);
}

bool InputManager::IsMouseButtonReleased(Mouse::Button button) const {
    return mouse_->IsButtonReleased(button);
}

const Vector2& InputManager::GetMousePosition() const {
    return mouse_->GetPosition();
}

const Vector2& InputManager::GetMouseDelta() const {
    return mouse_->GetDelta();
}

float InputManager::GetMouseWheelDelta() const {
    return mouse_->GetWheelDelta();
}