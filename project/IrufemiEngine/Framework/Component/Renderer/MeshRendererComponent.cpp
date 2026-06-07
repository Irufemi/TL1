#include "MeshRendererComponent.h"

#include "../../GameObject.h"
#include "../TransformComponent.h"
#include "Renderer/Object3D/StaticModelObject/StaticModelObject.h"
#include "Engine/Core/Math/Geometry/Collision.h"
#include "Engine/Core/Math/Geometry/OBB.h"
#include <cmath>

MeshRendererComponent::MeshRendererComponent() {}
MeshRendererComponent::~MeshRendererComponent() {}

void MeshRendererComponent::LoadModel(const std::string& filename) {
    modelName_ = filename;
    if (obj_) {
        obj_->Initialize(modelName_);
    }
}

void MeshRendererComponent::Initialize() {
    obj_ = std::make_unique<StaticModelObject>();
    obj_->Initialize(modelName_);

    // 親の GameObject から TransformComponent を探して保持しておく
    if (gameObject_) {
        transform_ = gameObject_->GetComponent<TransformComponent>();
    }
}

void MeshRendererComponent::Update() {
    // TransformComponent があれば、その座標を StaticModelObject に渡す（同期）
    if (transform_ && obj_) {
        obj_->SetTranslate(transform_->worldPosition_);
        obj_->SetRotate(transform_->worldRotation_);
        obj_->SetScale(transform_->worldScale_);
    }

    // StaticModelObject の行列計算などを実行
    if (obj_) {
        obj_->Update();
    }
}

void MeshRendererComponent::Draw() {
    // RenderGraph に向けて描画パケットを積む
    if (obj_) {
        obj_->Draw();
    }
}

Sphere MeshRendererComponent::GetWorldSphere() const {
    Sphere result = { Vector3{0,0,0}, 1.0f }; // default
    if (transform_) {
        result.center = transform_->worldPosition_;
        // StaticModelObject の cpuModel があれば正確な半径を取得
        // ここでは便宜上スケールの最大値を半径として扱う（もしくは定数）
        float maxScale = std::fmax(transform_->worldScale_.x, std::fmax(transform_->worldScale_.y, transform_->worldScale_.z));
        result.radius = maxScale;
    }
    return result;
}

bool MeshRendererComponent::Raycast(const Ray& ray, float& outDistance) const {
    if (!obj_ || !transform_) return false;
    auto cpuModel = obj_->GetCpuModel();
    if (!cpuModel) return false;

    // ローカルAABBから中心とサイズを取得
    Vector3 localCenter = (cpuModel->boundingBox.min + cpuModel->boundingBox.max) * 0.5f;
    Vector3 localHalfSize = (cpuModel->boundingBox.max - cpuModel->boundingBox.min) * 0.5f;

    OBB obb;
    // ワールド行列を用いて中心点を変換
    const Matrix4x4& wmat = transform_->GetWorldMatrix();
    obb.center = Math::Transform(localCenter, wmat);

    // ワールド行列の各軸ベクトルを抽出して正規化（回転）＆スケール適用
    Vector3 xAxis = { wmat.m[0][0], wmat.m[0][1], wmat.m[0][2] };
    Vector3 yAxis = { wmat.m[1][0], wmat.m[1][1], wmat.m[1][2] };
    Vector3 zAxis = { wmat.m[2][0], wmat.m[2][1], wmat.m[2][2] };

    float lenX = Math::Length(xAxis);
    float lenY = Math::Length(yAxis);
    float lenZ = Math::Length(zAxis);

    if (lenX > 0.0001f) obb.orientations[0] = Math::Normalize(xAxis);
    else obb.orientations[0] = {1.0f, 0.0f, 0.0f};

    if (lenY > 0.0001f) obb.orientations[1] = Math::Normalize(yAxis);
    else obb.orientations[1] = {0.0f, 1.0f, 0.0f};

    if (lenZ > 0.0001f) obb.orientations[2] = Math::Normalize(zAxis);
    else obb.orientations[2] = {0.0f, 0.0f, 1.0f};

    obb.size.x = localHalfSize.x * lenX;
    obb.size.y = localHalfSize.y * lenY;
    obb.size.z = localHalfSize.z * lenZ;

    return Collision::IsCollision(ray, obb, outDistance);
}




nlohmann::json MeshRendererComponent::Serialize() {
    nlohmann::json j;
    j["modelName"] = modelName_;
    return j;
}

void MeshRendererComponent::Deserialize(const nlohmann::json& j) {
    if (j.contains("modelName")) {
        std::string modelName = j["modelName"];
        LoadModel(modelName);
    }
}
