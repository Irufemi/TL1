#include "DebrisComponent.h"
#include "Framework/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Engine/IrufemiEngine.h"
#include "Engine/Platform/Input/InputManager.h"
#include "Renderer/Object3D/BaseModel/BaseModel.h"
#include "RailShooterEnemyComponent.h"
#include "DebrisManagerComponent.h"
#include <cmath>

void DebrisComponent::OnRegisterProperties() {
    RegisterProperty("Pull Speed", &pullSpeed_);
    RegisterProperty("Throw Speed", &throwSpeed_);
    RegisterProperty("Orbit Speed", &orbitSpeed_);
}

void DebrisComponent::Initialize() {
    state_ = DebrisState::Idle;
    targetObject_ = nullptr;
    idleTimeY_ = static_cast<float>(rand() % 100); // ランダムな位相で開始
    
    if (auto transform = gameObject_->GetComponent<TransformComponent>()) {
        baseIdleY_ = transform->position_.y;
    }
}

void DebrisComponent::Update() {
    if (!gameObject_) return;
    auto transform = gameObject_->GetComponent<TransformComponent>();
    if (!transform) return;

    float deltaTime = BaseModel::GetIrufemiEngine()->GetGameDeltaTime();
    if (deltaTime <= 0.0f) deltaTime = 1.0f / 60.0f;

    switch (state_) {
        case DebrisState::Idle: {
            // フワフワと上下に漂う疑似アニメーション
            idleTimeY_ += deltaTime * 2.0f;
            transform->position_.y = baseIdleY_ + std::sin(idleTimeY_) * 0.5f;
            break;
        }
        case DebrisState::Pulled: {
            if (targetObject_) {
                auto targetTransform = targetObject_->GetComponent<TransformComponent>();
                if (targetTransform) {
                    // ターゲット(プレイヤー)に向かってLerpで移動
                    Vector3 diff = {
                        targetTransform->position_.x - transform->position_.x,
                        targetTransform->position_.y + 2.0f - transform->position_.y, // 少し上に引き寄せる
                        targetTransform->position_.z - transform->position_.z
                    };
                    transform->position_.x += diff.x * pullSpeed_ * deltaTime;
                    transform->position_.y += diff.y * pullSpeed_ * deltaTime;
                    transform->position_.z += diff.z * pullSpeed_ * deltaTime;

                    // 一定距離に近づいたらOrbitingへ自動遷移
                    float distSq = diff.x*diff.x + diff.y*diff.y + diff.z*diff.z;
                    if (distSq < 2.0f) {
                        state_ = DebrisState::Orbiting;
                    }
                }
            }
            break;
        }
        case DebrisState::Orbiting: {
            if (targetObject_) {
                auto targetTransform = targetObject_->GetComponent<TransformComponent>();
                if (targetTransform) {
                    orbitAngle_ += orbitSpeed_ * deltaTime;
                    
                    // プレイヤーの周囲を回転するローカル座標を計算
                    Vector3 offset = {
                        std::cos(orbitAngle_) * orbitRadius_,
                        std::sin(orbitAngle_ * 2.0f) * 0.5f + 1.0f, // 8の字にフワフワ
                        std::sin(orbitAngle_) * orbitRadius_
                    };
                    
                    transform->position_.x = targetTransform->position_.x + offset.x;
                    transform->position_.y = targetTransform->position_.y + offset.y;
                    transform->position_.z = targetTransform->position_.z + offset.z;
                }
            }
            break;
        }
        case DebrisState::Thrown: {
            if (targetObject_ && targetObject_->GetIsActive()) {
                auto targetTransform = targetObject_->GetComponent<TransformComponent>();
                if (targetTransform) {
                    // 敵に向かって高速ホーミング移動
                    Vector3 diff = {
                        targetTransform->position_.x - transform->position_.x,
                        targetTransform->position_.y - transform->position_.y,
                        targetTransform->position_.z - transform->position_.z
                    };
                    // 正規化して一定速度で飛ばす
                    float len = std::sqrt(diff.x*diff.x + diff.y*diff.y + diff.z*diff.z);
                    if (len > 0.001f) {
                        throwDirection_ = { diff.x / len, diff.y / len, diff.z / len };
                    }
                    
                    // 簡易ヒット判定
                    if (len < 1.0f) {
                        auto enemyComp = targetObject_->GetComponent<RailShooterEnemyComponent>();
                        if (enemyComp) {
                            enemyComp->TakeDamage(100);
                        }
                        gameObject_->SetIsActive(false); 
                        if (manager_) {
                            manager_->NotifyDestroyed(virtualId_);
                        }
                    }
                }
            }
            
            // ターゲットがない（または既に死んだ）場合でも、計算された(または初期設定された)方向に飛び続ける
            transform->position_.x += throwDirection_.x * throwSpeed_ * deltaTime;
            transform->position_.y += throwDirection_.y * throwSpeed_ * deltaTime;
            transform->position_.z += throwDirection_.z * throwSpeed_ * deltaTime;
            
            // TODO: 一定距離/時間で消滅させる等の処理が必要
            break;
        }
    }
}
