#include "ButtonComponent.h"
#include "../../GameObject.h"
#include "../../BaseScene.h"
#include "../TransformComponent.h"
#include "../Renderer/SpriteRendererComponent.h"
#include "Engine/IrufemiEngine.h"
#include "Engine/Platform/Input/InputManager.h"
#include "../../SceneTransition.h"

void ButtonComponent::OnRegisterProperties() {
    RegisterProperty("Load Scene Name", &onClickLoadScene_);
    // 0:Fade, 1:Dissolve, 2:Slide, 3:RadialBlur
    RegisterProperty("Transition Type(0-3)", &transitionType_);
    RegisterProperty("Transition Duration", &transitionDuration_);
    RegisterProperty("Transition Delay", &transitionDelay_);
    RegisterProperty("Click Anim Duration", &clickAnimDuration_);
    RegisterProperty("Normal Color", &normalColor_);
    RegisterProperty("Hover Color", &hoverColor_);
    RegisterProperty("Click Color", &clickColor_);
    RegisterProperty("Enable Hover Pulse", &enableHoverPulse_);
    RegisterProperty("Enable Idle Pulse", &enableIdlePulse_);
    RegisterProperty("Hitbox Scale", &hitboxScale_);
}

void ButtonComponent::Initialize() {
    if (gameObject_) {
        transform_ = gameObject_->GetComponent<TransformComponent>();
        sprite_ = gameObject_->GetComponent<SpriteRendererComponent>();
    }
}

bool ButtonComponent::CheckBounds(const Vector2& mousePos) {
    if (!transform_ || !sprite_) return false;
    
    Vector3 pos = transform_->worldPosition_;
    Vector3 scale = transform_->worldScale_;
    
    auto* s = sprite_->GetSprite();
    if (!s) return false;
    
    // スプライトのアンカーとサイズを取得
    Vector2 anchor = s->GetAnchor();
    Vector2 baseSize = s->GetSize();
    
    // Hitbox Scale を加味した幅・高さを算出
    // （※sprite_->GetSize() は既に Transform の Scale が適用された描画上のサイズを返すため、
    //  ここでは scale.x/y を二重に掛けないようにする）
    float width = baseSize.x * hitboxScale_.x;
    float height = baseSize.y * hitboxScale_.y;
    
    // アンカー位置を加味して当たり判定矩形を計算
    // （例えば anchor が 0.5 の場合、pos が中心になる）
    float left = pos.x - width * anchor.x;
    float right = pos.x + width * (1.0f - anchor.x);
    float top = pos.y - height * anchor.y;
    float bottom = pos.y + height * (1.0f - anchor.y);
    
    return (mousePos.x >= left && mousePos.x <= right &&
            mousePos.y >= top && mousePos.y <= bottom);
}

void ButtonComponent::Update() {
    if (!sprite_ || !gameObject_) return;
    
    auto scene = gameObject_->GetScene();
    if (!scene) return;
    auto engine = scene->GetEngine();
    if (!engine) return;
    
    auto input = engine->GetInputManager();
    Vector2 mousePos = input->GetMousePosition();
    
    isHovered_ = CheckBounds(mousePos);
    isClicked_ = false;

    // アニメーターの更新（1/60固定とするか deltaTime を取得するか。簡易的に1/60）
    animator_.Update(1.0f / 60.0f);

    // 遷移待機中の処理（シーン遷移待ち＆クリックアニメーション）
    if (isTransitionPending_) {
        float dt = 1.0f / 60.0f; // 簡易フレームレート
        transitionTimer_ -= dt;
        
        // クリック中の色を維持
        sprite_->GetSprite()->SetColor(clickColor_);
        
        // アニメーション（押し込み ＆ 高速フラッシュ演出）
        if (transform_) {
            float timePassed = transitionDelay_ - transitionTimer_;
            if (timePassed <= clickAnimDuration_ && clickAnimDuration_ > 0.0f) {
                // アニメーション中は少し縮小する（0.9倍）
                transform_->worldScale_ = originalScale_ * 0.9f;
                
                // 高速フラッシュ演出 (PromptController を参考)
                bool isVisible = animator_.GetFlashVisibility(40.0f);
                if (isVisible) {
                    sprite_->GetSprite()->SetColor(clickColor_);
                } else {
                    // 非表示状態（アルファ0）
                    Vector4 transparentColor = clickColor_;
                    transparentColor.w = 0.0f;
                    sprite_->GetSprite()->SetColor(transparentColor);
                }
            } else {
                // アニメーションが終わったら元のスケールと色に戻す
                transform_->worldScale_ = originalScale_;
                sprite_->GetSprite()->SetColor(clickColor_);
            }
        }
        
        // 待機時間が終了したらシーン遷移を実行
        if (transitionTimer_ <= 0.0f) {
            isTransitionPending_ = false;
            
            if (!onClickLoadScene_.empty()) {
                SceneTransition::Type type = SceneTransition::Type::Fade;
                switch (transitionType_) {
                    case 0: type = SceneTransition::Type::Fade; break;
                    case 1: type = SceneTransition::Type::Dissolve; break;
                    case 2: type = SceneTransition::Type::Slide; break;
                    case 3: type = SceneTransition::Type::RadialBlur; break;
                }
                engine->GetSceneManager()->LoadScene(onClickLoadScene_, type, transitionDuration_);
            }
        }
        
        return; // 遷移待機中はホバー等の他の入力を受け付けない
    }

    if (isHovered_) {
        // ホバーした瞬間に押下されたらフラグを立てる
        if (input->IsMouseButtonPressed(Mouse::Button::Left)) {
            isPressedOnButton_ = true;
        }

        if (input->IsMouseButtonDown(Mouse::Button::Left)) {
            // 押下中（ボタン上で押下開始した場合のみ色を変える）
            if (isPressedOnButton_) {
                sprite_->GetSprite()->SetColor(clickColor_);
            } else {
                sprite_->GetSprite()->SetColor(normalColor_);
            }
        } else {
            // ホバー中
            Vector4 color = hoverColor_;
            if (enableHoverPulse_) {
                float animAlpha = animator_.GetPulseAlpha(0.7f, 0.3f, 5.0f);
                color.w *= animAlpha;
            }
            sprite_->GetSprite()->SetColor(color);
            
            // 離された瞬間（クリック完了）
            if (input->IsMouseButtonReleased(Mouse::Button::Left) && isPressedOnButton_) {
                isClicked_ = true;
                
                if (!onClickLoadScene_.empty()) {
                    isTransitionPending_ = true;
                    transitionTimer_ = transitionDelay_;
                    if (transform_) {
                        originalScale_ = transform_->worldScale_;
                    }
                }
            }
        }
    } else {
        // 通常状態（待機中）
        Vector4 color = normalColor_;
        if (enableIdlePulse_) {
            // PromptControllerと同じパルスアニメーション（ベース0.6、振幅0.4、速度3.0）
            float animAlpha = animator_.GetPulseAlpha(0.6f, 0.4f, 3.0f);
            color.w *= animAlpha;
        } else {
            // アニメーション無効時はリセットしておく
            animator_.Reset();
        }
        sprite_->GetSprite()->SetColor(color);
    }

    // どこかでマウスが離されたらフラグをリセットする
    if (input->IsMouseButtonReleased(Mouse::Button::Left)) {
        isPressedOnButton_ = false;
    }
}
