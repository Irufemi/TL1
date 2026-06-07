#include "OBBColliderComponent.h"
#include "Framework/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Engine/Manager/CollisionManager.h"
#include "Engine/Core/Math/MathFunction.h"


OBBColliderComponent::OBBColliderComponent() {}

OBBColliderComponent::~OBBColliderComponent() {
    CollisionManager::GetInstance().UnregisterCollider(this);
}

void OBBColliderComponent::Initialize() {
    if (gameObject_) {
        transform_ = gameObject_->GetComponent<TransformComponent>();
    }
    CollisionManager::GetInstance().RegisterCollider(this);
}

void OBBColliderComponent::Update() {
    if (!transform_ && gameObject_) {
        transform_ = gameObject_->GetComponent<TransformComponent>();
    }
}

void OBBColliderComponent::DrawDebug() {
}



OBB OBBColliderComponent::GetWorldOBB() const {
    OBB obb;
    if (transform_) {
        // ワールド行列の各軸ベクトルからスケールと回転を抽出
        Vector3 right = { transform_->GetWorldMatrix().m[0][0], transform_->GetWorldMatrix().m[0][1], transform_->GetWorldMatrix().m[0][2] };
        Vector3 up    = { transform_->GetWorldMatrix().m[1][0], transform_->GetWorldMatrix().m[1][1], transform_->GetWorldMatrix().m[1][2] };
        Vector3 forward = { transform_->GetWorldMatrix().m[2][0], transform_->GetWorldMatrix().m[2][1], transform_->GetWorldMatrix().m[2][2] };
        
        float scaleX = Math::Length(right);
        float scaleY = Math::Length(up);
        float scaleZ = Math::Length(forward);
        
        obb.orientations[0] = Math::Normalize(right);
        obb.orientations[1] = Math::Normalize(up);
        obb.orientations[2] = Math::Normalize(forward);
        
        Vector3 worldPos = { transform_->GetWorldMatrix().m[3][0], transform_->GetWorldMatrix().m[3][1], transform_->GetWorldMatrix().m[3][2] };
        
        // Offsetも回転・スケールを考慮
        obb.center = worldPos 
                   + obb.orientations[0] * (localOffset_.x * scaleX)
                   + obb.orientations[1] * (localOffset_.y * scaleY)
                   + obb.orientations[2] * (localOffset_.z * scaleZ);
                   
        obb.size = { localSize_.x * scaleX, localSize_.y * scaleY, localSize_.z * scaleZ };
    } else {
        obb.center = localOffset_;
        obb.orientations[0] = {1.0f, 0.0f, 0.0f};
        obb.orientations[1] = {0.0f, 1.0f, 0.0f};
        obb.orientations[2] = {0.0f, 0.0f, 1.0f};
        obb.size = localSize_;
    }
    return obb;
}

nlohmann::json OBBColliderComponent::Serialize() {
    nlohmann::json j;
    j["localOffset"] = { localOffset_.x, localOffset_.y, localOffset_.z };
    j["localSize"] = { localSize_.x, localSize_.y, localSize_.z };
    j["layer"] = layer_;
    j["mask"] = mask_;
    return j;
}

void OBBColliderComponent::Deserialize(const nlohmann::json& j) {
    if (j.contains("localOffset")) {
        localOffset_.x = j["localOffset"][0];
        localOffset_.y = j["localOffset"][1];
        localOffset_.z = j["localOffset"][2];
    }
    if (j.contains("localSize")) {
        localSize_.x = j["localSize"][0];
        localSize_.y = j["localSize"][1];
        localSize_.z = j["localSize"][2];
    }
    if (j.contains("layer")) layer_ = j["layer"];
    if (j.contains("mask")) mask_ = j["mask"];
}
