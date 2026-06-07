#include "RailShooterEnemyComponent.h"
#include "Framework/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Renderer/MeshRendererComponent.h" // 描画オンオフ用
#include "Engine/IrufemiEngine.h"
#include "Renderer/Object3D/BaseModel/BaseModel.h"

void RailShooterEnemyComponent::OnRegisterProperties() {
    RegisterProperty("SpawnProgress", &spawnProgress_);
    RegisterProperty("Speed", &speed_);
    RegisterProperty("HP", &hp_);
}

void RailShooterEnemyComponent::Initialize() {
    isActive_ = false;
    
    // 初期状態では非表示にしておく
    if (gameObject_) {
        if (auto renderer = gameObject_->GetComponent<MeshRendererComponent>()) {
            // (仮) 初期状態は非アクティブとして振る舞う
            // エンジン側に Enable/Disable の機能があればそれを使用する
            // ここでは簡易的にスケールを 0 にして見えないようにするか、Rendererを調整する
        }
    }
}

void RailShooterEnemyComponent::Update() {
    if (!gameObject_) return;

    // TODO: プレイヤーの進行度をグローバルまたはManagerから取得して比較
    // ここでは単純に isActive になったら前に進むだけの仮実装
    if (isActive_) {
        // エンジンから正確なゲーム内時間差を取得
        float deltaTime = BaseModel::GetIrufemiEngine()->GetGameDeltaTime();
        if (deltaTime <= 0.0f) {
            deltaTime = 1.0f / 60.0f;
        } 
        if (auto transform = gameObject_->GetComponent<TransformComponent>()) {
            // 前方(Z軸正方向など)に進む
            // Transformの rotation_ を元に向きベクトルを計算して足す
            float yaw = transform->rotation_.y;
            Vector3 forward = { std::sin(yaw), 0.0f, std::cos(yaw) };
            
            transform->position_.x += forward.x * speed_ * deltaTime;
            transform->position_.y += forward.y * speed_ * deltaTime;
            transform->position_.z += forward.z * speed_ * deltaTime;
        }
    } else {
        // 条件を満たしたらアクティブ化
        // (仮: 常にアクティブにする)
        isActive_ = true;
    }
}

void RailShooterEnemyComponent::TakeDamage(int damage) {
    if (!IsAlive()) return;

    hp_ -= damage;
    if (hp_ <= 0) {
        hp_ = 0;
        isActive_ = false;
        if (gameObject_) {
            gameObject_->SetIsActive(false); // 死亡して非アクティブ化
        }
    }
}
