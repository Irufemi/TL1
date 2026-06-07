#include "SphereColliderComponent.h"
#include "Framework/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Engine/Manager/CollisionManager.h"

#include <algorithm>
#include <cmath>

SphereColliderComponent::SphereColliderComponent() {}

SphereColliderComponent::~SphereColliderComponent() {
    CollisionManager::GetInstance().UnregisterCollider(this);
}

void SphereColliderComponent::Initialize() {
    if (gameObject_) {
        transform_ = gameObject_->GetComponent<TransformComponent>();
    }
    CollisionManager::GetInstance().RegisterCollider(this);
}

void SphereColliderComponent::Update() {
    if (!transform_ && gameObject_) {
        transform_ = gameObject_->GetComponent<TransformComponent>();
    }
}

void SphereColliderComponent::DrawDebug() {
}



Sphere SphereColliderComponent::GetWorldSphere() const {
    Sphere sphere;
    if (transform_) {
        Vector3 worldPos = { transform_->GetWorldMatrix().m[3][0], transform_->GetWorldMatrix().m[3][1], transform_->GetWorldMatrix().m[3][2] };
        
        // スケールの最大成分を半径に掛ける
        float scaleX = std::sqrt(std::pow(transform_->GetWorldMatrix().m[0][0], 2.0f) + std::pow(transform_->GetWorldMatrix().m[0][1], 2.0f) + std::pow(transform_->GetWorldMatrix().m[0][2], 2.0f));
        float scaleY = std::sqrt(std::pow(transform_->GetWorldMatrix().m[1][0], 2.0f) + std::pow(transform_->GetWorldMatrix().m[1][1], 2.0f) + std::pow(transform_->GetWorldMatrix().m[1][2], 2.0f));
        float scaleZ = std::sqrt(std::pow(transform_->GetWorldMatrix().m[2][0], 2.0f) + std::pow(transform_->GetWorldMatrix().m[2][1], 2.0f) + std::pow(transform_->GetWorldMatrix().m[2][2], 2.0f));
        
        float maxXY = scaleX > scaleY ? scaleX : scaleY;
        float maxScale = maxXY > scaleZ ? maxXY : scaleZ;
        
        sphere.center = { worldPos.x + localOffset_.x * scaleX, 
                          worldPos.y + localOffset_.y * scaleY, 
                          worldPos.z + localOffset_.z * scaleZ };
        sphere.radius = localRadius_ * maxScale;
    } else {
        sphere.center = localOffset_;
        sphere.radius = localRadius_;
    }
    return sphere;
}

nlohmann::json SphereColliderComponent::Serialize() {
    nlohmann::json j;
    j["localOffset"] = { localOffset_.x, localOffset_.y, localOffset_.z };
    j["localRadius"] = localRadius_;
    j["layer"] = layer_;
    j["mask"] = mask_;
    return j;
}

void SphereColliderComponent::Deserialize(const nlohmann::json& j) {
    if (j.contains("localOffset")) {
        localOffset_.x = j["localOffset"][0];
        localOffset_.y = j["localOffset"][1];
        localOffset_.z = j["localOffset"][2];
    }
    if (j.contains("localRadius")) {
        localRadius_ = j["localRadius"];
    }
    if (j.contains("layer")) layer_ = j["layer"];
    if (j.contains("mask")) mask_ = j["mask"];
}
