#pragma once
#include "Component.h"
#include "Engine/Core/Math/Vector3.h"
#include "Engine/Core/Math/Matrix4x4.h"
#include "Engine/Core/Math/MathFunction.h"

class TransformComponent : public Component {
public:
    Vector3 position_ = { 0.0f, 0.0f, 0.0f };
    Vector3 rotation_ = { 0.0f, 0.0f, 0.0f }; // Euler angles in radians
    Vector3 scale_ = { 1.0f, 1.0f, 1.0f };

    Vector3 worldPosition_ = { 0.0f, 0.0f, 0.0f };
    Vector3 worldRotation_ = { 0.0f, 0.0f, 0.0f };
    Vector3 worldScale_ = { 1.0f, 1.0f, 1.0f };

    void Initialize() override {}
    void Update() override;

    bool CanUpdateInEditMode() const override { return true; }

    const Matrix4x4& GetWorldMatrix() const { return worldMatrix_; }
    const Matrix4x4& GetLocalMatrix() const { return localMatrix_; }

    std::string GetComponentName() const override { return "TransformComponent"; }
    nlohmann::json Serialize() override;
    void Deserialize(const nlohmann::json& j) override;

#ifdef EditorMode

#endif

private:
    Matrix4x4 localMatrix_ = Math::MakeIdentity4x4();
    Matrix4x4 worldMatrix_ = Math::MakeIdentity4x4();
};
