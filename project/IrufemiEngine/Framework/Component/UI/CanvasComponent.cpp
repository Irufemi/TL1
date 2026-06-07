#include "CanvasComponent.h"
#include "../../GameObject.h"
#include "../Renderer/SpriteRendererComponent.h"

void CanvasComponent::OnRegisterProperties() {
    RegisterProperty("Group Alpha", &groupAlpha_);
}

void CanvasComponent::Initialize() {
}

static void ApplyAlphaRecursive(GameObject* obj, float alpha) {
    if (!obj) return;
    
    // 自身のSpriteRendererがあればAlphaを適用
    auto sprite = obj->GetComponent<SpriteRendererComponent>();
    if (sprite && sprite->GetSprite()) {
        Vector4 color = sprite->GetSprite()->GetColor();
        color.w = alpha; // 今回は単純な上書き（必要なら元Alphaとの乗算にする）
        sprite->GetSprite()->SetColor(color);
    }
    
    // 子へ再帰
    for (auto& child : obj->GetChildren()) {
        ApplyAlphaRecursive(child.get(), alpha);
    }
}

void CanvasComponent::Update() {
    if (!gameObject_) return;
    
    // 全ての子オブジェクトのSpriteのAlphaを一括設定
    ApplyAlphaRecursive(gameObject_, groupAlpha_);
}
