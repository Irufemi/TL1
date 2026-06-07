#pragma once
#include "../Component.h"
#include "Renderer/Object2D/Sprite/Sprite.h"
#include <memory>
#include <string>
#include "Engine/Core/Math/Vector4.h"

class TransformComponent;

/**
 * @class SpriteRendererComponent
 * @brief 2Dスプライト描画用コンポーネント
 * @details GameObjectにアタッチして2D画像の描画とUI操作を提供します
 */
class SpriteRendererComponent : public Component {
public:
    SpriteRendererComponent();
    virtual ~SpriteRendererComponent();

    void Initialize() override;
    void Update() override;
    void Draw() override;
    
    bool CanUpdateInEditMode() const override { return true; }
    
    IRenderable* GetRenderable() override { return reinterpret_cast<IRenderable*>(sprite_.get()); }
#ifdef EditorMode
    friend class SpriteRendererComponentEditor;
#endif
    void SetTexture(const std::string& texturePath);
    Sprite* GetSprite() const { return sprite_.get(); }

    std::string GetComponentName() const override { return "SpriteRendererComponent"; }
    nlohmann::json Serialize() override;
    void Deserialize(const nlohmann::json& j) override;

private:
    std::unique_ptr<Sprite> sprite_;
    TransformComponent* transform_ = nullptr;
    
    std::string texturePath_ = "resources/uvChecker.png";
    bool isTopMost_ = false;
    bool isFlipX_ = false;
    bool isFlipY_ = false;
    float anchor_[2] = { 0.5f, 0.5f };
    float size_[2] = { 640.0f, 360.0f };
    Vector4 color_ = { 1.0f, 1.0f, 1.0f, 1.0f };
};
