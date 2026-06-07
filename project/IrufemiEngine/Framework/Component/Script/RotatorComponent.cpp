#include "RotatorComponent.h"
#include "Framework/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Engine/IrufemiEngine.h"

void RotatorComponent::Initialize() {
    if (gameObject_) {
        transform_ = gameObject_->GetComponent<TransformComponent>();
    }
}

void RotatorComponent::Update() {
    if (!transform_ && gameObject_) {
        transform_ = gameObject_->GetComponent<TransformComponent>();
    }
    
    if (transform_) {
        // IrufemiEngine のデルタタイムをここで取得できればベストですが、
        // 今回はシンプルに固定値で回します（または引数からデルタタイムを取る想定）
        float dt = 0.016f; // 約60fps
        
        transform_->rotation_.x += rotationSpeedX_ * dt;
        transform_->rotation_.y += rotationSpeedY_ * dt;
        transform_->rotation_.z += rotationSpeedZ_ * dt;
    }
}

