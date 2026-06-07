#pragma once
#include "ColliderComponent.h"
#include "Engine/Core/Math/Geometry/OBB.h"
#include "Engine/Core/Math/Vector3.h"
#include <string>

class TransformComponent;

class OBBColliderComponent : public ColliderComponent {
public:
    OBBColliderComponent();
    ~OBBColliderComponent() override;

    void Initialize() override;
    void Update() override;
    void DrawDebug() override;
    nlohmann::json Serialize() override;
    void Deserialize(const nlohmann::json& j) override;

    std::string GetComponentName() const override { return "OBBColliderComponent"; }
    ColliderType GetColliderType() const override { return ColliderType::OBB; }

    OBB GetWorldOBB() const;

    void SetLocalOffset(const Vector3& offset) { localOffset_ = offset; }
    const Vector3& GetLocalOffset() const { return localOffset_; }

    void SetLocalSize(const Vector3& size) { localSize_ = size; }
    const Vector3& GetLocalSize() const { return localSize_; }

private:
    TransformComponent* transform_ = nullptr;

    Vector3 localOffset_ = { 0.0f, 0.0f, 0.0f };
    Vector3 localSize_   = { 1.0f, 1.0f, 1.0f }; // Extents
};
