#include "CameraFollowPlayerComponent.h"
#include "Framework/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/BaseScene.h"
#include "Engine/IrufemiEngine.h"
#include "Renderer/Object3D/BaseModel/BaseModel.h"
#include <cmath>

void CameraFollowPlayerComponent::OnRegisterProperties() {
    RegisterProperty("Offset", &offset_);
    RegisterProperty("FollowDelay", &followDelay_);
}

void CameraFollowPlayerComponent::Initialize() {
    targetTransform_ = nullptr;
}

void CameraFollowPlayerComponent::Update() {
    if (!gameObject_) return;

    // 正確なデルタタイムの取得
    float deltaTime = BaseModel::GetIrufemiEngine()->GetGameDeltaTime();
    if (deltaTime <= 0.0f) {
        deltaTime = 1.0f / 60.0f;
    }

    // ターゲットが未キャッシュの場合はシーン内から "Player" を探索
    if (!targetTransform_ && gameObject_->GetScene()) {
        const auto& objs = gameObject_->GetScene()->GetGameObjects();
        for (const auto& obj : objs) {
            if (obj->GetName() == "Player") {
                if (auto transform = obj->GetComponent<TransformComponent>()) {
                    targetTransform_ = transform;
                    break;
                }
            }
        }
    }

    if (!targetTransform_) return;

    auto myTransform = gameObject_->GetComponent<TransformComponent>();
    if (!myTransform) return;

    // プレイヤーの向き（回転角度）から進行方向をベースとしたローカル座標系を作成
    float yaw = targetTransform_->rotation_.y;
    float pitch = targetTransform_->rotation_.x;

    // プレイヤーを基準とした回転行列の方向成分を計算
    Vector3 forward = {
        std::sin(yaw) * std::cos(pitch),
        std::sin(-pitch),
        std::cos(yaw) * std::cos(pitch)
    };
    
    // 正規化
    float len = std::sqrt(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
    if (len > 0.0001f) {
        forward.x /= len; forward.y /= len; forward.z /= len;
    } else {
        forward = {0.0f, 0.0f, 1.0f};
    }

    // 右ベクトルと上ベクトルの算出 (外積)
    Vector3 upVec = {0.0f, 1.0f, 0.0f};
    Vector3 right = {
        upVec.y * forward.z - upVec.z * forward.y,
        upVec.z * forward.x - upVec.x * forward.z,
        upVec.x * forward.y - upVec.y * forward.x
    };
    float rLen = std::sqrt(right.x * right.x + right.y * right.y + right.z * right.z);
    if (rLen > 0.0001f) {
        right.x /= rLen; right.y /= rLen; right.z /= rLen;
    } else {
        right = {1.0f, 0.0f, 0.0f};
    }

    Vector3 up = {
        forward.y * right.z - forward.z * right.y,
        forward.z * right.x - forward.x * right.z,
        forward.x * right.y - forward.y * right.x
    };

    // プレイヤー位置に、プレイヤーの向きに基づいたローカルオフセットを足す
    // offset_.z は forward (プレイヤーの後方), offset.y は up (プレイヤーの頭上), offset.x は right (左右のズレ)
    Vector3 targetCamPos = {
        targetTransform_->position_.x + right.x * offset_.x + up.x * offset_.y + forward.x * offset_.z,
        targetTransform_->position_.y + right.y * offset_.x + up.y * offset_.y + forward.y * offset_.z,
        targetTransform_->position_.z + right.z * offset_.x + up.z * offset_.y + forward.z * offset_.z
    };

    // 滑らかな追従 (線形補間/Lerp) を行う (フレームレート非依存)
    float t = 1.0f - std::pow(followDelay_, deltaTime); 
    myTransform->position_.x += (targetCamPos.x - myTransform->position_.x) * t;
    myTransform->position_.y += (targetCamPos.y - myTransform->position_.y) * t;
    myTransform->position_.z += (targetCamPos.z - myTransform->position_.z) * t;

    // カメラの向き（角度）もプレイヤーの向きに追従させる（Lerpで滑らかに旋回）
    myTransform->rotation_.x += (targetTransform_->rotation_.x - myTransform->rotation_.x) * t;
    myTransform->rotation_.y += (targetTransform_->rotation_.y - myTransform->rotation_.y) * t;
    myTransform->rotation_.z += (targetTransform_->rotation_.z - myTransform->rotation_.z) * t;
}
