#pragma once
#include "../Component.h"
#include "Renderer/Object2D/Text/Text.h"
#include <memory>
#include <string>
#include "Engine/Core/Math/Vector4.h"

class TransformComponent;

/**
 * @class TextRendererComponent
 * @brief 2Dテキスト描画用コンポーネント (MSDF対応)
 */
class TextRendererComponent : public Component {
public:
    TextRendererComponent();
    virtual ~TextRendererComponent();

    void Initialize() override;
    void Update() override;
    void Draw() override;
    
    bool CanUpdateInEditMode() const override { return true; }
    
    // エディタのRaycast用
    bool Raycast(const Ray& ray, float& outDistance) const override;
    
    IRenderable* GetRenderable() override { return reinterpret_cast<IRenderable*>(textObj_.get()); }
    void DrawOutlineMask() override {
        if (textObj_) {
            textObj_->DrawOutlineMask();
        }
    }

    // 文字列の設定
    void SetText(const std::wstring& text);
    std::wstring GetText() const { return text_; }

    // フォントの変更
    void SetFontId(const std::string& fontId);
    std::string GetFontId() const { return fontId_; }

    // ベーススケール（文字サイズ）
    void SetBaseScale(float baseScale);
    float GetBaseScale() const { return baseScale_; }

    // アライメント
    void SetAlignment(TextAlignment align);
    TextAlignment GetAlignment() const { return alignment_; }

    // 文字色
    void SetColor(const Vector4& color);
    Vector4 GetColor() const { return color_; }
    
    // UIとして最前面に描画するか
    void SetTopMost(bool isTopMost);
    bool IsTopMost() const { return isTopMost_; }
    
    // バウンディングボックス取得（ローカル座標系）
    Vector2 GetLocalBoundsMin() const { return textObj_ ? textObj_->GetLocalBoundsMin() : Vector2{0.0f, 0.0f}; }
    Vector2 GetLocalBoundsMax() const { return textObj_ ? textObj_->GetLocalBoundsMax() : Vector2{0.0f, 0.0f}; }

    Text* GetTextObject() const { return textObj_.get(); }

    std::string GetComponentName() const override { return "TextRendererComponent"; }
    void OnRegisterProperties() override;

private:
    std::unique_ptr<Text> textObj_;
    TransformComponent* transform_ = nullptr;
    
    std::wstring text_ = L"Text";
    std::string textU8_ = "Text"; // For Reflection
    std::string fontId_ = "MainFont";
    float baseScale_ = 64.0f;
    Vector4 color_ = { 1.0f, 1.0f, 1.0f, 1.0f };
    TextAlignment alignment_ = TextAlignment::Left;
    int alignmentInt_ = 0; // For Reflection (0:Left, 1:Center, 2:Right)
    bool isTopMost_ = false;
};
