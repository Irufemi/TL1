#include "SpriteRendererComponent.h"
#include "../../GameObject.h"
#include "../TransformComponent.h"
#include "Resource/Texture/TextureManager.h"



SpriteRendererComponent::SpriteRendererComponent() {}
SpriteRendererComponent::~SpriteRendererComponent() {}

void SpriteRendererComponent::Initialize() {
    sprite_ = std::make_unique<Sprite>();
    sprite_->Initialize(texturePath_);
    
    // 初期設定
    sprite_->SetAnchor(anchor_[0], anchor_[1]);
    sprite_->SetFlip(isFlipX_, isFlipY_);
    sprite_->SetTopMost(isTopMost_);
    sprite_->SetColor(color_);
    
    // テクスチャサイズを取得して初期サイズに設定（Deserializeで既にサイズが設定されていない場合のみ）
    if (size_[0] == 640.0f && size_[1] == 360.0f) { // デフォルト値の場合は上書き
        size_[0] = sprite_->GetSize().x;
        size_[1] = sprite_->GetSize().y;
    } else if (size_[0] == 0.0f && size_[1] == 0.0f) {
        size_[0] = sprite_->GetSize().x;
        size_[1] = sprite_->GetSize().y;
    }

    if (gameObject_) {
        transform_ = gameObject_->GetComponent<TransformComponent>();
    }
}

void SpriteRendererComponent::Update() {
    if (transform_ && sprite_) {
        // SpriteはZ位置も保持できるが基本は2D
        sprite_->SetPosition(transform_->worldPosition_.x, transform_->worldPosition_.y, transform_->worldPosition_.z);
        // Spriteの回転はZ軸のみ
        sprite_->SetRotation(transform_->worldRotation_.z);
        
        // TransformのScaleは、SpriteのBaseサイズに対するスケーリングとして扱う
        sprite_->SetSize(size_[0] * transform_->worldScale_.x, size_[1] * transform_->worldScale_.y);
    }

    if (sprite_) {
        sprite_->Update();
    }
}

void SpriteRendererComponent::Draw() {
    if (sprite_) {
        sprite_->Draw(); // SyncBeforeDrawはSprite内で呼ばれる
    }
}

void SpriteRendererComponent::SetTexture(const std::string& texturePath) {
    texturePath_ = texturePath;
    if (sprite_) {
        sprite_->SetTexture(texturePath_);
        // テクスチャ変更に合わせてサイズを更新
        size_[0] = sprite_->GetSize().x;
        size_[1] = sprite_->GetSize().y;
    }
}



nlohmann::json SpriteRendererComponent::Serialize() {
    nlohmann::json j;
    j["texturePath"] = texturePath_;
    j["isTopMost"] = isTopMost_;
    j["isFlipX"] = isFlipX_;
    j["isFlipY"] = isFlipY_;
    j["anchor"] = { anchor_[0], anchor_[1] };
    j["size"] = { size_[0], size_[1] };
    j["color"] = { color_.x, color_.y, color_.z, color_.w };
    return j;
}

void SpriteRendererComponent::Deserialize(const nlohmann::json& j) {
    if (j.contains("texturePath")) SetTexture(j["texturePath"]);
    if (j.contains("isTopMost")) isTopMost_ = j["isTopMost"];
    if (j.contains("isFlipX")) isFlipX_ = j["isFlipX"];
    if (j.contains("isFlipY")) isFlipY_ = j["isFlipY"];
    if (j.contains("anchor") && j["anchor"].is_array() && j["anchor"].size() == 2) {
        anchor_[0] = j["anchor"][0];
        anchor_[1] = j["anchor"][1];
    }
    if (j.contains("size") && j["size"].is_array() && j["size"].size() == 2) {
        size_[0] = j["size"][0];
        size_[1] = j["size"][1];
    }
    if (j.contains("color") && j["color"].is_array() && j["color"].size() == 4) {
        color_.x = j["color"][0];
        color_.y = j["color"][1];
        color_.z = j["color"][2];
        color_.w = j["color"][3];
    }
    
    // 反映
    if (sprite_) {
        sprite_->SetAnchor(anchor_[0], anchor_[1]);
        sprite_->SetFlip(isFlipX_, isFlipY_);
        sprite_->SetTopMost(isTopMost_);
        sprite_->SetColor(color_);
        // サイズの反映はUpdateでscaleを考慮して行われるが、ベースサイズとして保持
    }
}
