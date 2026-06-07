#include "RaycastComponent.h"
#include "Framework/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Engine/Core/Math/MathFunction.h"


void RaycastComponent::Initialize() {
    if (gameObject_) {
        transform_ = gameObject_->GetComponent<TransformComponent>();
    }
}

void RaycastComponent::Update() {
    if (!transform_ && gameObject_) {
        transform_ = gameObject_->GetComponent<TransformComponent>();
    }

    if (transform_) {
        // ワールド空間でのレイの起点と方向を計算
        Vector3 worldPos = transform_->worldPosition_;
        Matrix4x4 worldMat = transform_->GetWorldMatrix();
        
        // オフセットの適用
        Vector3 worldOffset = Math::TransformNormal(localOffset_, worldMat);
        currentRay_.origin = worldPos + worldOffset;
        
        // 方向の適用（ローカル方向ベクトルをワールドへ回転）
        Vector3 worldDir = Math::TransformNormal(localDirection_, worldMat);
        worldDir = Math::Normalize(worldDir);
        currentRay_.diff = worldDir; // diffを方向として扱う

        // 判定実行（自分自身が持つ他のコライダーには当たらないようにgameObject_を渡す）
        CollisionManager::GetInstance().Raycast(currentRay_, hitInfo_, maxDistance_, mask_, gameObject_);

        if (hitInfo_.isHit && onHit_) {
            onHit_(hitInfo_);
        }

        // 毎フレーム判定後にデバッグ描画を行う
        DrawDebug();
    }
}

void RaycastComponent::DrawDebug() {
    if (showDebugLine_) {
        Vector4 color = hitInfo_.isHit ? Vector4{ 1.0f, 0.0f, 0.0f, 1.0f } : Vector4{ 0.0f, 1.0f, 0.0f, 1.0f };
        float drawDist = hitInfo_.isHit ? hitInfo_.distance : maxDistance_;
        CollisionManager::GetInstance().DrawDebugRay(currentRay_, drawDist, color);
    }
}



nlohmann::json RaycastComponent::Serialize() {
    nlohmann::json j;
    j["localOffset"] = { localOffset_.x, localOffset_.y, localOffset_.z };
    j["localDirection"] = { localDirection_.x, localDirection_.y, localDirection_.z };
    j["maxDistance"] = maxDistance_;
    j["mask"] = mask_;
    j["showDebugLine"] = showDebugLine_;
    return j;
}

void RaycastComponent::Deserialize(const nlohmann::json& j) {
    if (j.contains("localOffset")) {
        localOffset_.x = j["localOffset"][0];
        localOffset_.y = j["localOffset"][1];
        localOffset_.z = j["localOffset"][2];
    }
    if (j.contains("localDirection")) {
        localDirection_.x = j["localDirection"][0];
        localDirection_.y = j["localDirection"][1];
        localDirection_.z = j["localDirection"][2];
    }
    if (j.contains("maxDistance")) maxDistance_ = j["maxDistance"];
    if (j.contains("mask")) mask_ = j["mask"];
    if (j.contains("showDebugLine")) showDebugLine_ = j["showDebugLine"];
}
