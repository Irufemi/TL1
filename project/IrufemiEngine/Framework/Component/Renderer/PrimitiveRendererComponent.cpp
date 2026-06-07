#include "PrimitiveRendererComponent.h"

#include "../../GameObject.h"
#include "../TransformComponent.h"
#include "Renderer/Object3D/Primitive/Primitive3DObject.h"
#include "Engine/Manager/PrimitiveManager.h"
#include "Engine/Core/Type/PrimitiveType.h"
#include "Engine/Core/Math/Geometry/Collision.h"
#include "Engine/Core/Math/Geometry/OBB.h"
#include <cmath>

PrimitiveRendererComponent::PrimitiveRendererComponent() {}
PrimitiveRendererComponent::~PrimitiveRendererComponent() {}

void PrimitiveRendererComponent::Initialize() {
    primitive_ = std::make_unique<Primitive3DObject>();
    // 設定された形状（デフォルトはCube）で初期化
    primitive_->Initialize(static_cast<PrimitiveType>(currentTypeIndex_));

    if (gameObject_) {
        transform_ = gameObject_->GetComponent<TransformComponent>();
    }
}

void PrimitiveRendererComponent::Update() {
    if (transform_ && primitive_) {
        primitive_->SetPosition(transform_->worldPosition_);
        primitive_->SetRotate(transform_->worldRotation_);
        primitive_->SetScale(transform_->worldScale_);
    }

    if (primitive_) {
        primitive_->Update();
    }
}

void PrimitiveRendererComponent::Draw() {
    if (primitive_) {
        primitive_->Draw();
    }
}

void PrimitiveRendererComponent::SetShape(PrimitiveType type) {
    currentTypeIndex_ = static_cast<int>(type);
    if (primitive_) {
        primitive_->SetShape(type);
    }
}

void PrimitiveRendererComponent::SetColor(const Vector4& color) {
    if (primitive_) {
        primitive_->SetColor(color);
    }
}

void PrimitiveRendererComponent::SetTexture(const std::string& texturePath) {
    if (primitive_) {
        primitive_->SetTexture(texturePath);
    }
}

void PrimitiveRendererComponent::RebuildMesh() {
    if (!primitive_) return;
    
    PrimitiveType type = static_cast<PrimitiveType>(currentTypeIndex_);
    PrimitiveData data;

    switch (type) {
        case PrimitiveType::Sphere:
        case PrimitiveType::IcoSphere:
            data = PrimitiveManager::CreateSphere(radius_, subdivisions_);
            break;
        case PrimitiveType::Cylinder:
            data = PrimitiveManager::CreateCylinder(bottomRadius_, topRadius_, height_, subdivisions_, hasTop_, hasBottom_);
            break;
        case PrimitiveType::Cone:
            data = PrimitiveManager::CreateCone(radius_, height_, subdivisions_);
            break;
        case PrimitiveType::Torus:
            data = PrimitiveManager::CreateTorus(torusMajorRadius_, torusMinorRadius_, torusMajorSegments_, torusMinorSegments_);
            break;
        case PrimitiveType::Circle:
            data = PrimitiveManager::CreateCircle(radius_, subdivisions_);
            break;
        case PrimitiveType::Cube:
        case PrimitiveType::Plane:
        case PrimitiveType::Triangle:
        case PrimitiveType::Tetra:
        default:
            // これらの基本図形は標準リソースに戻す
            primitive_->SetShape(type);
            return;
    }
    
    primitive_->ReinitializeMesh(data);
}



nlohmann::json PrimitiveRendererComponent::Serialize() {
    nlohmann::json j;
    j["currentTypeIndex"] = currentTypeIndex_;
    j["radius"] = radius_;
    j["subdivisions"] = subdivisions_;
    j["height"] = height_;
    j["topRadius"] = topRadius_;
    j["bottomRadius"] = bottomRadius_;
    j["hasTop"] = hasTop_;
    j["hasBottom"] = hasBottom_;
    j["torusMajorRadius"] = torusMajorRadius_;
    j["torusMinorRadius"] = torusMinorRadius_;
    j["torusMajorSegments"] = torusMajorSegments_;
    j["torusMinorSegments"] = torusMinorSegments_;
    
    if (primitive_) {
        const auto& mat = primitive_->GetMaterial();
        nlohmann::json matJson;
        matJson["texturePath"] = mat.texturePath;
        matJson["color"] = { mat.color.x, mat.color.y, mat.color.z, mat.color.w };
        matJson["enableLighting"] = mat.enableLighting;
        matJson["lightingMode"] = mat.lightingMode;
        matJson["metallic"] = mat.metallic;
        matJson["roughness"] = mat.roughness;
        matJson["alphaReference"] = mat.alphaReference;
        matJson["useClampSampler"] = mat.useClampSampler;
        j["material"] = matJson;
    }
    
    return j;
}

void PrimitiveRendererComponent::Deserialize(const nlohmann::json& j) {
    if (j.contains("currentTypeIndex")) currentTypeIndex_ = j["currentTypeIndex"];
    if (j.contains("radius")) radius_ = j["radius"];
    if (j.contains("subdivisions")) subdivisions_ = j["subdivisions"];
    if (j.contains("height")) height_ = j["height"];
    if (j.contains("topRadius")) topRadius_ = j["topRadius"];
    if (j.contains("bottomRadius")) bottomRadius_ = j["bottomRadius"];
    if (j.contains("hasTop")) hasTop_ = j["hasTop"];
    if (j.contains("hasBottom")) hasBottom_ = j["hasBottom"];
    if (j.contains("torusMajorRadius")) torusMajorRadius_ = j["torusMajorRadius"];
    if (j.contains("torusMinorRadius")) torusMinorRadius_ = j["torusMinorRadius"];
    if (j.contains("torusMajorSegments")) torusMajorSegments_ = j["torusMajorSegments"];
    if (j.contains("torusMinorSegments")) torusMinorSegments_ = j["torusMinorSegments"];
    
    // 形状を再構築
    PrimitiveType types[] = { PrimitiveType::Sphere, PrimitiveType::Plane, PrimitiveType::Cube, PrimitiveType::Cylinder, PrimitiveType::Cone, PrimitiveType::Torus };
    if (currentTypeIndex_ >= 0 && currentTypeIndex_ < 6) {
        SetShape(types[currentTypeIndex_]);
    }
    
    // マテリアル情報の復元
    if (j.contains("material") && primitive_) {
        const auto& matJson = j["material"];
        auto& mat = primitive_->GetMaterial();
        
        if (matJson.contains("texturePath")) mat.texturePath = matJson["texturePath"];
        if (matJson.contains("color") && matJson["color"].is_array() && matJson["color"].size() == 4) {
            mat.color.x = matJson["color"][0];
            mat.color.y = matJson["color"][1];
            mat.color.z = matJson["color"][2];
            mat.color.w = matJson["color"][3];
        }
        if (matJson.contains("enableLighting")) mat.enableLighting = matJson["enableLighting"];
        if (matJson.contains("lightingMode")) mat.lightingMode = matJson["lightingMode"];
        if (matJson.contains("metallic")) mat.metallic = matJson["metallic"];
        if (matJson.contains("roughness")) mat.roughness = matJson["roughness"];
        if (matJson.contains("alphaReference")) mat.alphaReference = matJson["alphaReference"];
        if (matJson.contains("useClampSampler")) mat.useClampSampler = matJson["useClampSampler"];
        
        primitive_->SetTexture(mat.texturePath);
    }
}

Sphere PrimitiveRendererComponent::GetWorldSphere() const {
    Sphere result = { Vector3{0,0,0}, 1.0f }; // default
    if (transform_) {
        result.center = transform_->worldPosition_;
        
        // 形状に応じて大まかな半径を決定
        float baseRadius = radius_;
        if (static_cast<PrimitiveType>(currentTypeIndex_) == PrimitiveType::Cube) {
            baseRadius = 1.0f; // Cubeは1x1x1なので対角線の半分は約0.866だが余裕を持つ
        }
        
        float maxScale = std::fmax(transform_->worldScale_.x, std::fmax(transform_->worldScale_.y, transform_->worldScale_.z));
        result.radius = baseRadius * maxScale * 2.0f; // 安全マージン
    }
    return result;
}

bool PrimitiveRendererComponent::Raycast(const Ray& ray, float& outDistance) const {
    if (!primitive_ || !transform_) return false;

    // プリミティブ形状の基本AABB（一辺1のキューブ）
    Vector3 localHalfSize = { 0.5f, 0.5f, 0.5f };

    OBB obb;
    obb.center = transform_->worldPosition_;

    const Matrix4x4& wmat = transform_->GetWorldMatrix();
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

