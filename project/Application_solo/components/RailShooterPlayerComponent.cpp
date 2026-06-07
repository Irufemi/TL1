#include "RailShooterPlayerComponent.h"
#include "RailPathComponent.h"
#include "Framework/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/BaseScene.h"
#include "Engine/IrufemiEngine.h"
#include "Engine/Platform/Input/InputManager.h"
#include "Renderer/Object3D/BaseModel/BaseModel.h"
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <algorithm>
#include <cmath>

void RailShooterPlayerComponent::OnRegisterProperties() {
    RegisterProperty("Speed", &speed_);
    RegisterProperty("XYSpeed", &xySpeed_);
    RegisterProperty("MoveLimitMin", &moveLimitMin_);
    RegisterProperty("MoveLimitMax", &moveLimitMax_);
}

void RailShooterPlayerComponent::Initialize() {
    // キャッシュのクリア
    cachedPath_ = nullptr;
    isDummyBasePosInitialized_ = false;
}

void RailShooterPlayerComponent::Update() {
    if (!gameObject_) return;

    // 1フレームの経過時間 (エンジンから正確なゲーム内時間差を取得)
    float deltaTime = BaseModel::GetIrufemiEngine()->GetGameDeltaTime();
    if (deltaTime <= 0.0f) {
        deltaTime = 1.0f / 60.0f; // 安全策として仮のフレーム時間を設定
    } 

    // シーン内からレール（軌道）のデータを持っているオブジェクトを自動で探し出す
    if (!cachedPath_ && gameObject_->GetScene()) {
        const auto& objs = gameObject_->GetScene()->GetGameObjects();
        for (const auto& obj : objs) {
            if (auto path = obj->GetComponent<RailPathComponent>()) {
                cachedPath_ = path; // 見つけたら後で使い回すために保存
                break;
            }
        }
    }

    // レールデータが「本当に存在し、かつポイントが1点以上打たれているか」をチェック
    bool hasPath = false;
    if (cachedPath_ && !cachedPath_->GetWaypoints().empty()) {
        hasPath = true;
    }

    Vector3 basePos = {0.0f, 0.0f, 0.0f};
    Vector3 tangent = {0.0f, 0.0f, 1.0f};

    auto transform = gameObject_->GetComponent<TransformComponent>();
    if (!transform) return;

    if (hasPath) {
        // --- 1. ルート（レール）がある場合の自動移動 ---
        // 進行度を前進させる
        progress_ += speed_ * deltaTime;
        if (progress_ > 1.0f) progress_ = 1.0f; // 終点に到達したらストップ

        // レール上の「基準位置」と「進んでいる向き（接線）」を計算して取得
        basePos = cachedPath_->GetPointAt(progress_);
        tangent = cachedPath_->GetTangentAt(progress_);
        isDummyBasePosInitialized_ = false; // パスがあるため直進用データはリセット
    } else {
        // --- 2. ルート（レール）が無い場合の「そのまま真っ直ぐ」自動直進移動 ---
        if (!isDummyBasePosInitialized_) {
            // 直進を開始する時の現在地をスタート地点にする
            dummyBasePos_ = transform->position_;
            isDummyBasePosInitialized_ = true;
        }

        // キャラクターの現在の回転角度から「正面を向くベクトル（向き）」を逆算する
        float yaw = transform->rotation_.y;
        float pitch = transform->rotation_.x;
        tangent = {
            std::sin(yaw) * std::cos(pitch),
            std::sin(-pitch),
            std::cos(yaw) * std::cos(pitch)
        };
        
        // 進む向きベクトルの長さを1に綺麗に整える（正規化）
        float len = std::sqrt(tangent.x * tangent.x + tangent.y * tangent.y + tangent.z * tangent.z);
        if (len > 0.0001f) {
            tangent.x /= len; tangent.y /= len; tangent.z /= len;
        } else {
            tangent = {0.0f, 0.0f, 1.0f}; // 向きが壊れている場合はデフォルトでZ軸の前へ
        }

        // speed_（割合）をゲーム空間内の移動速度（秒速）に変換（100倍を基準とする）
        float moveSpeed = speed_ * 100.0f;
        // 基準位置を「正面ベクトル × 移動スピード」で前進させる
        dummyBasePos_.x += tangent.x * moveSpeed * deltaTime;
        dummyBasePos_.y += tangent.y * moveSpeed * deltaTime;
        dummyBasePos_.z += tangent.z * moveSpeed * deltaTime;

        basePos = dummyBasePos_;
    }

    // 進んでいる向き(tangent)を基準にして、プレイヤーから見た「右方向」と「上方向」を割り出す
    // これにより、どんな斜めのレールの上でも「自分から見た上下左右」に正確に避けることができます
    Vector3 up = {0.0f, 1.0f, 0.0f};
    
    // 右方向を計算 (外積: up x tangent)
    Vector3 right = {
        up.y * tangent.z - up.z * tangent.y,
        up.z * tangent.x - up.x * tangent.z,
        up.x * tangent.y - up.y * tangent.x
    };
    // 右方向ベクトルの長さを1にする
    float rightLen = std::sqrt(right.x * right.x + right.y * right.y + right.z * right.z);
    if (rightLen > 0.0001f) {
        right.x /= rightLen; right.y /= rightLen; right.z /= rightLen;
    } else {
        right = {1.0f, 0.0f, 0.0f};
    }

    // 上方向を再計算 (外積: tangent x right)
    up = {
        tangent.y * right.z - tangent.z * right.y,
        tangent.z * right.x - tangent.x * right.z,
        tangent.x * right.y - tangent.y * right.x
    };

    // --- 3. キー入力による上下左右の回避運動 ---
    auto* input = BaseModel::GetIrufemiEngine()->GetInputManager();
    Vector3 moveDir = {0.0f, 0.0f, 0.0f};

    // WASD または 矢印キーで移動方向を入力
    if (input->IsKeyPressed(DIK_W) || input->IsKeyPressed(DIK_UP)) moveDir.y += 1.0f;
    if (input->IsKeyPressed(DIK_S) || input->IsKeyPressed(DIK_DOWN)) moveDir.y -= 1.0f;
    if (input->IsKeyPressed(DIK_A) || input->IsKeyPressed(DIK_LEFT)) moveDir.x -= 1.0f;
    if (input->IsKeyPressed(DIK_D) || input->IsKeyPressed(DIK_RIGHT)) moveDir.x += 1.0f;

    // 斜め移動したときに移動速度が速くならないように、ベクトルの長さを1に抑える
    if (moveDir.x != 0.0f || moveDir.y != 0.0f) {
        float len = std::sqrt(moveDir.x * moveDir.x + moveDir.y * moveDir.y);
        moveDir.x /= len;
        moveDir.y /= len;

        // レール中心からのズレ幅（オフセット値）を増やす
        currentOffset_.x += moveDir.x * xySpeed_ * deltaTime;
        currentOffset_.y += moveDir.y * xySpeed_ * deltaTime;

        // 指定した画面内の限界範囲（クランプ範囲）を超えないように制限する
        currentOffset_.x = std::clamp(currentOffset_.x, moveLimitMin_.x, moveLimitMax_.x);
        currentOffset_.y = std::clamp(currentOffset_.y, moveLimitMin_.y, moveLimitMax_.y);
    }

    // --- 4. 最終的な位置と向きの決定 ---
    // 「レールの基準位置」に「右方向へのズレ」と「上方向へのズレ」を合算して最終座標を作る
    Vector3 finalPos = {
        basePos.x + right.x * currentOffset_.x + up.x * currentOffset_.y,
        basePos.y + right.y * currentOffset_.x + up.y * currentOffset_.y,
        basePos.z + right.z * currentOffset_.x + up.z * currentOffset_.y
    };

    // プレイヤーのTransform座標に代入して実際に動かす
    transform->position_ = finalPos;
    
    // レール上を動いている時のみ、レールの進行方向に合わせてプレイヤーの向き（首振り）を自動調整する
    // (レールが無い時は、エディタ上で配置したプレイヤーの回転角を優先して直進させます)
    if (hasPath) {
        float yaw = std::atan2(tangent.x, tangent.z);
        float pitch = std::asin(-tangent.y); // Z前方の座標系を想定
        transform->rotation_ = {pitch, yaw, 0.0f};
    }
}
