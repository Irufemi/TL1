#include "TransformComponent.h"
#include "../GameObject.h"
#include "Engine/Core/Math/MathFunction.h"



void TransformComponent::Update() {
    // ローカル行列の計算
    localMatrix_ = Math::MakeAffineMatrix(scale_, rotation_, position_);

    // 親のワールド行列を加味して自身のワールド行列を計算
    if (auto parent = gameObject_->GetParent()) {
        if (auto parentTransform = parent->GetComponent<TransformComponent>()) {
            worldMatrix_ = Math::Multiply(localMatrix_, parentTransform->GetWorldMatrix());
        } else {
            worldMatrix_ = localMatrix_;
        }
    } else {
        worldMatrix_ = localMatrix_;
    }

    // ワールド行列から座標・回転・スケールを抽出する
    worldPosition_ = { worldMatrix_.m[3][0], worldMatrix_.m[3][1], worldMatrix_.m[3][2] };
    
    Vector3 xaxis = { worldMatrix_.m[0][0], worldMatrix_.m[0][1], worldMatrix_.m[0][2] };
    Vector3 yaxis = { worldMatrix_.m[1][0], worldMatrix_.m[1][1], worldMatrix_.m[1][2] };
    Vector3 zaxis = { worldMatrix_.m[2][0], worldMatrix_.m[2][1], worldMatrix_.m[2][2] };
    
    worldScale_ = { Math::Length(xaxis), Math::Length(yaxis), Math::Length(zaxis) };
    worldRotation_ = Math::ExtractEulerFromMatrix(worldMatrix_);
}

nlohmann::json TransformComponent::Serialize() {
    nlohmann::json j;
    j["position"] = { position_.x, position_.y, position_.z };
    j["rotation"] = { rotation_.x, rotation_.y, rotation_.z };
    j["scale"]    = { scale_.x, scale_.y, scale_.z };
    return j;
}

void TransformComponent::Deserialize(const nlohmann::json& j) {
    if (j.contains("position") && j["position"].is_array() && j["position"].size() == 3) {
        position_.x = j["position"][0];
        position_.y = j["position"][1];
        position_.z = j["position"][2];
    }
    if (j.contains("rotation") && j["rotation"].is_array() && j["rotation"].size() == 3) {
        rotation_.x = j["rotation"][0];
        rotation_.y = j["rotation"][1];
        rotation_.z = j["rotation"][2];
    }
    if (j.contains("scale") && j["scale"].is_array() && j["scale"].size() == 3) {
        scale_.x = j["scale"][0];
        scale_.y = j["scale"][1];
        scale_.z = j["scale"][2];
    }
}
