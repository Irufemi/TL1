#pragma once
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Platform/Input/InputManager.h"
#include "Engine/Core/Math/Vector3.h"

class EditorCameraController {
public:
    void UpdateCameraInput(Camera* camera, InputManager* input);

private:
    bool isInitialized_ = false;
    Vector3 target_ = {0.0f, 0.0f, 0.0f};
    float distance_ = 50.0f;
};
