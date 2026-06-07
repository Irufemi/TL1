#include "GravityPlayerComponent.h"
#include "DebrisComponent.h"
#include "Framework/GameObject.h"
#include "Framework/BaseScene.h"
#include "Framework/Component/TransformComponent.h"
#include "Engine/IrufemiEngine.h"
#include "Engine/Platform/Input/InputManager.h"
#include "Engine/Platform/Input/Mouse.h"
#include "Renderer/Object3D/BaseModel/BaseModel.h"
#include "Engine/Core/Math/Random/Random.h"
#include "Engine/Graphics/Camera/CameraManager.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Core/Math/MathFunction.h"
#include "RailShooterEnemyComponent.h"
#include <algorithm>

void GravityPlayerComponent::OnRegisterProperties() {
    RegisterProperty("Max Orbit Count", &maxOrbitCount_);
    RegisterProperty("Pull Radius", &pullRadius_);
}

void GravityPlayerComponent::Initialize() {
    orbitingDebris_.clear();
    lockedTarget_ = nullptr;
}

void GravityPlayerComponent::Update() {
    // 無効になった（すでに敵に当たって非アクティブになった等）ガレキをリストから除外
    orbitingDebris_.erase(
        std::remove_if(orbitingDebris_.begin(), orbitingDebris_.end(),
            [](const std::shared_ptr<GameObject>& obj) {
                if (!obj || !obj->GetIsActive()) return true;
                auto comp = obj->GetComponent<DebrisComponent>();
                // Thrown など、引き寄せ(Pulled)および周回(Orbiting)以外の状態になったらリストから除外
                return !comp || (comp->GetState() != DebrisState::Orbiting && comp->GetState() != DebrisState::Pulled);
            }),
        orbitingDebris_.end()
    );

    UpdateAim();
    HandlePullInput();
    HandleThrowInput();
}

void GravityPlayerComponent::HandlePullInput() {
    auto input = BaseModel::GetIrufemiEngine()->GetInputManager();
    if (!input) return;

    // 右クリック または Eキー で引き寄せ
    if (input->IsMouseButtonPressed(Mouse::Button::Right) || input->IsKeyPressed('E')) {
        // 現在の保持数が上限に達しているなら引き寄せない
        if (static_cast<int>(orbitingDebris_.size()) >= maxOrbitCount_) return;

        // シーン内のすべての GameObject からガレキを検索
        auto scene = gameObject_->GetScene();
        if (!scene) return;

        auto transform = gameObject_->GetComponent<TransformComponent>();
        if (!transform) return;

        // 一番親である DebrisManager オブジェクト（子としてガレキを保持している）を探す
        std::shared_ptr<GameObject> debrisManager = nullptr;
        for (auto& obj : scene->GetGameObjects()) {
            if (obj && obj->GetName() == "DebrisManager") {
                debrisManager = obj;
                break;
            }
        }

        if (!debrisManager) return;

        // 子オブジェクト（ガレキ）を走査
        for (auto& child : debrisManager->GetChildren()) {
            if (!child || !child->GetIsActive()) continue;

            auto debrisComp = child->GetComponent<DebrisComponent>();
            if (!debrisComp || debrisComp->GetState() != DebrisState::Idle) continue;

            auto childTransform = child->GetComponent<TransformComponent>();
            if (!childTransform) continue;

            // 距離判定
            float dx = childTransform->position_.x - transform->position_.x;
            float dy = childTransform->position_.y - transform->position_.y;
            float dz = childTransform->position_.z - transform->position_.z;
            float distSq = dx*dx + dy*dy + dz*dz;

            if (distSq <= pullRadius_ * pullRadius_) {
                // 引き寄せ対象にする
                debrisComp->SetTarget(gameObject_);
                debrisComp->SetState(DebrisState::Pulled);
                
                // 回転半径や初期角度をランダムに設定
                debrisComp->SetOrbitParams(
                    Random::GeneratorFloat(0.0f, 6.28f),
                    Random::GeneratorFloat(2.0f, 4.0f)
                );

                orbitingDebris_.push_back(child);

                // 最大数に達したら終了
                if (static_cast<int>(orbitingDebris_.size()) >= maxOrbitCount_) {
                    break;
                }
            }
        }
    }
}

void GravityPlayerComponent::HandleThrowInput() {
    auto input = BaseModel::GetIrufemiEngine()->GetInputManager();
    if (!input) return;

    // 左クリック または Qキー で発射
    if (input->IsMouseButtonPressed(Mouse::Button::Left) || input->IsKeyPressed('Q')) {
        if (orbitingDebris_.empty()) return;

        // 保持しているガレキから1つ取り出す
        auto debris = orbitingDebris_.back();
        orbitingDebris_.pop_back();

        if (debris) {
            auto comp = debris->GetComponent<DebrisComponent>();
            if (comp) {
                // ロックオン対象がいればターゲットに設定
                comp->SetTarget(lockedTarget_.get());
                
                // ターゲットがいない場合はカメラの前方へ飛ばす
                if (!lockedTarget_) {
                    auto cameraManager = BaseModel::GetIrufemiEngine()->GetCameraManager();
                    if (cameraManager && cameraManager->GetActiveCamera()) {
                        auto camera = cameraManager->GetActiveCamera();
                        // カメラのRotationから前方を計算
                        Matrix4x4 viewMat = camera->GetViewMatrix();
                        // View行列のZ軸成分の逆（またはカメラのForward）を使う
                        // Irufemiエンジンの Camera は Transform ではなく rotate_ などを保持しているかもしれないが
                        // 今回は単純に ViewMatrix からカメラの前方を抽出
                        // 3行目がForwardベクトル(D3DのView行列の場合、_31, _32, _33)
                        Vector3 forward = { viewMat.m[0][2], viewMat.m[1][2], viewMat.m[2][2] };
                        
                        // Z軸方向が画面奥だとすると、DirectX(LH)なら View行列の3行目は通常Zの正方向を向いている
                        comp->SetThrowDirection({ forward.x, forward.y, forward.z });
                    }
                }
                
                comp->SetState(DebrisState::Thrown);
            }
        }
    }
}

void GravityPlayerComponent::UpdateAim() {
    lockedTarget_ = nullptr; // 毎フレームリセット

    // ガレキを保持していない場合はロックオンしない
    if (orbitingDebris_.empty()) return;

    auto cameraManager = BaseModel::GetIrufemiEngine()->GetCameraManager();
    if (!cameraManager) return;
    auto camera = cameraManager->GetActiveCamera();
    if (!camera) return;

    Matrix4x4 viewProj = camera->GetViewProjectionMatrix3D();
    float viewWidth = camera->GetViewportWidth();
    float viewHeight = camera->GetViewportHeight();
    
    Vector2 screenCenter = { viewWidth * 0.5f, viewHeight * 0.5f };
    float closestDistSq = lockonRadius2D_ * lockonRadius2D_;

    auto scene = gameObject_->GetScene();
    if (!scene) return;

    for (auto& obj : scene->GetGameObjects()) {
        if (!obj || !obj->GetIsActive()) continue;

        auto enemyComp = obj->GetComponent<RailShooterEnemyComponent>();
        if (!enemyComp || !enemyComp->IsAlive()) continue;

        auto transform = obj->GetComponent<TransformComponent>();
        if (!transform) continue;

        // ワールド座標からNDC座標へ
        Vector3 clipPos = Math::Transform(transform->position_, viewProj);
        
        // Zが0～1の範囲外（カメラ後方など）なら除外
        if (clipPos.z < 0.0f || clipPos.z > 1.0f) continue;

        // NDCからスクリーン座標へ変換
        float screenX = (clipPos.x + 1.0f) * 0.5f * viewWidth;
        float screenY = (1.0f - clipPos.y) * 0.5f * viewHeight;

        float dx = screenX - screenCenter.x;
        float dy = screenY - screenCenter.y;
        float distSq = dx * dx + dy * dy;

        // 一番画面中央に近い敵をロックオン対象とする
        if (distSq < closestDistSq) {
            closestDistSq = distSq;
            lockedTarget_ = obj;
        }
    }
}
