#pragma once
#include "../Component.h"
#include <string>
#include "Engine/Core/Math/Vector4.h"
#include "../../UIAnimator.h"

class TransformComponent;
class SpriteRendererComponent;

/**
 * @class ButtonComponent
 * @brief マウスのホバー・クリックを判定し、色変更やシーン遷移を行うUIコンポーネント
 */
class ButtonComponent : public Component {
public:
    ButtonComponent() = default;
    ~ButtonComponent() override = default;

    void Initialize() override;
    void Update() override;
    
    std::string GetComponentName() const override { return "ButtonComponent"; }
    void OnRegisterProperties() override;

    bool IsHovered() const { return isHovered_; }
    bool IsClicked() const { return isClicked_; }

private:
    bool CheckBounds(const struct Vector2& mousePos);

    std::string onClickLoadScene_ = ""; // クリック時に自動で遷移するシーン名
    int transitionType_ = 0;            // 0:Fade, 1:Dissolve, 2:Slide, 3:RadialBlur
    float transitionDuration_ = 1.0f;   // トランジションにかける時間
    
    Vector4 normalColor_ = {1.0f, 1.0f, 1.0f, 1.0f};
    Vector4 hoverColor_  = {0.8f, 0.8f, 0.8f, 1.0f};
    Vector4 clickColor_{ 0.5f, 0.5f, 0.5f, 1.0f };

    bool enableHoverPulse_ = true; // ホバー時にサイン波で明滅するかどうか
    bool enableIdlePulse_ = true;  // 待機中（ホバーしていない時）も明滅するかどうか

    // 当たり判定のスケール調整（画像自体の余白などを省くため）
    Vector2 hitboxScale_{ 1.0f, 1.0f };

    float clickAnimDuration_ = 0.8f;    // クリックアニメーションの長さ（フラッシュなど）
    float transitionDelay_ = 0.8f;      // シーン遷移開始までの待機時間（デフォルトはアニメと同じ）

    // --- 内部状態 ---
    UIAnimator animator_;

    bool isHovered_ = false;
    bool isClicked_ = false;
    bool isPressedOnButton_ = false;
    bool isTransitionPending_ = false;
    float transitionTimer_ = 0.0f;
    Vector3 originalScale_ = {1.0f, 1.0f, 1.0f};

    TransformComponent* transform_ = nullptr;
    SpriteRendererComponent* sprite_ = nullptr;
};
