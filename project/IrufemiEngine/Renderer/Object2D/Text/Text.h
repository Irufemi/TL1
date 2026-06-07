#pragma once

#include "../../Core/IRenderable.h"
#include <d3d12.h>
#include <vector>
#include <string>
#include <cstdint>
#include "Renderer/Object2D/Object2DResource.h"
#include "Engine/Core/Math/Vector2.h"
#include <wrl.h>
#include <memory>

class FontManager;
class DrawManager;
class DebugUI;
class CameraManager;

enum class TextAlignment {
    Left,
    Center,
    Right
};

/**
 * @class Text
 * @brief 2Dテキストを描画・管理するクラス (MSDF対応)
 * @details アライメント機能付き
 */
class Text : public IRenderable {
public:
    Text();
    ~Text() = default;

    void Initialize(const std::string& fontId = "MainFont");
    void Update();
    void SyncBeforeDraw() override;
    void Draw() override;
    void DrawOutlineMask() override;
    
    // Setters
    void SetText(const std::wstring& text);
    void SetFontId(const std::string& fontId);
    void SetPosition(const float& x, const float& y, const float& z = 0.0f) { if(resource_) resource_->transform_.translate = {x,y,z}; isDirty_ = true; }
    void SetRotation(const float& rotate) { if(resource_) resource_->transform_.rotate = {0.0f, 0.0f, rotate}; isDirty_ = true; }
    void SetScale(const float& scaleX, const float& scaleY) { if(resource_) resource_->transform_.scale = {scaleX, scaleY, 1.0f}; isDirty_ = true; }
    void SetColor(const Vector4& color) { color_ = color; if(resource_) resource_->GetMaterialData()->color = color; isDirty_ = true; }
    Vector4 GetColor() const { return color_; }
    void SetTopMost(bool isTopMost) { isTopMost_ = isTopMost; }
    bool IsTopMost() const { return isTopMost_; }
    void SetBaseScale(float baseScale) { baseScale_ = baseScale; isTextDirty_ = true; }
    void SetAlignment(TextAlignment align) { alignment_ = align; isTextDirty_ = true; }
    
    Object2DResource* GetD3D12Resource() { return resource_.get(); }
    const std::wstring& GetText() const { return text_; }
    const std::string& GetFontId() const { return fontId_; }
    float GetBaseScale() const { return baseScale_; }
    TextAlignment GetAlignment() const { return alignment_; }
    
    const Vector2& GetLocalBoundsMin() const { return localBoundsMin_; }
    const Vector2& GetLocalBoundsMax() const { return localBoundsMax_; }
    
    // Engine dependencies
    static void SetFontManager(FontManager* fm) { fontManager_ = fm; }
    static FontManager* GetFontManager() { return fontManager_; }
    static void SetDrawManager(DrawManager* dm) { drawManager_ = dm; }
    static void SetCameraManager(CameraManager* cm) { cameraManager_ = cm; }
    static void SetDebugUI(DebugUI* ui) { ui_ = ui; }

private:
    void GenerateVertices();

    std::unique_ptr<Object2DResource> resource_ = nullptr;
    std::wstring text_ = L"";
    std::string fontId_ = "MainFont";
    float baseScale_ = 64.0f; // MSDF生成時のピクセルサイズを基準とするスケーリング
    TextAlignment alignment_ = TextAlignment::Left;
    Vector4 color_ = { 1.0f, 1.0f, 1.0f, 1.0f };
    
    Vector2 localBoundsMin_ = {0.0f, 0.0f};
    Vector2 localBoundsMax_ = {0.0f, 0.0f};
    
    bool isDirty_ = true;
    bool isTextDirty_ = true;
    bool isTopMost_ = false;

    static FontManager* fontManager_;
    static DrawManager* drawManager_;
    static CameraManager* cameraManager_;
    static DebugUI* ui_;
    
    Matrix4x4 lastViewMatrix_ = {};
    Matrix4x4 lastProjectionMatrix_ = {};
    D3D12_GPU_DESCRIPTOR_HANDLE lastAtlasSrv_ = {0};
};
