#include "CameraComponent.h"
#include "Framework/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Engine/IrufemiEngine.h"
#include "Renderer/Object3D/BaseModel/BaseModel.h"

CameraComponent::CameraComponent() = default;
CameraComponent::~CameraComponent() = default;

void CameraComponent::OnRegisterProperties() {
    RegisterProperty("FOV", &fovAngleY_);
    RegisterProperty("NearZ", &nearZ_);
    RegisterProperty("FarZ", &farZ_);
    RegisterProperty("MakeActive", &makeActive_);
}

void CameraComponent::Initialize() {
    camera_ = std::make_shared<Camera>();
    camera_->Initialize();
    
    // プロパティ値を適用
    camera_->SetFovY(fovAngleY_);
    camera_->SetFarClip(farZ_);
    
    // カメラマネージャに登録
    if (gameObject_) {
        auto* engine = BaseModel::GetIrufemiEngine();
        if (engine && engine->GetCameraManager()) {
            engine->GetCameraManager()->AddCamera(gameObject_->GetName(), camera_);
            if (makeActive_) {
                engine->GetCameraManager()->SetActiveCamera(gameObject_->GetName());
            }
        }
    }
}

void CameraComponent::Update() {
    if (!gameObject_ || !camera_) return;

    auto transform = gameObject_->GetComponent<TransformComponent>();
    if (!transform) return;

    // GameObjectのTransformとCameraの座標・角度を同期
    camera_->SetTranslate(transform->position_);
    camera_->SetRotate(transform->rotation_);
    
    // パラメータの動的更新反映
    camera_->SetFovY(fovAngleY_);
    camera_->SetFarClip(farZ_);
    
    camera_->UpdateMatrix();
}
