#pragma once
#include "ColliderComponent.h"
#include "Engine/Core/Shape/Sphere.h"
#include "Engine/Core/Math/Vector3.h"
#include <string>

class TransformComponent;

class SphereColliderComponent : public ColliderComponent {
public:
    SphereColliderComponent();
    ~SphereColliderComponent() override;

    void Initialize() override;
    void Update() override;
    void DrawDebug() override;
    nlohmann::json Serialize() override;
    void Deserialize(const nlohmann::json& j) override;

    std::string GetComponentName() const override { return "SphereColliderComponent"; }
    ColliderType GetColliderType() const override { return ColliderType::Sphere; }

    Sphere GetWorldSphere() const;

    void SetLocalOffset(const Vector3& offset) { localOffset_ = offset; }
    const Vector3& GetLocalOffset() const { return localOffset_; }

    void SetLocalRadius(float radius) { localRadius_ = radius; }
    float GetLocalRadius() const { return localRadius_; }

private:
    TransformComponent* transform_ = nullptr;

    Vector3 localOffset_ = { 0.0f, 0.0f, 0.0f };
    float localRadius_   = 1.0f;
};
