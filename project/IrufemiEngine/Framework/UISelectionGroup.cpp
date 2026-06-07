#include "UISelectionGroup.h"
#include "Engine/Platform/Input/InputManager.h"
#include "Renderer/Object2D/Sprite/Sprite.h"
#include "Renderer/Object3D/StaticModelObject/StaticModelObject.h"

UISelectionGroup::UISelectionGroup() {
    animator_.Reset();
}

void UISelectionGroup::AddItem(Sprite* sprite) {
    if (sprite) {
        items_.push_back(sprite);
    }
}

void UISelectionGroup::AddItem(StaticModelObject* obj) {
    if (obj) {
        items_.push_back(obj);
    }
}

void UISelectionGroup::Reset() {
    isDecided_ = false;
    selectedIndex_ = 0;
    transitionDelayTimer_ = 0.0f;
    isVisible_ = true;
    animator_.Reset();
}

void UISelectionGroup::Update(InputManager* input) {
    // 1フレームの時間を 1.0f / 60.0f と仮定
    animator_.Update(1.0f / 60.0f);

    if (!isDecided_) {
        // --- 待機中 ---
        isVisible_ = true;
        
        if (!items_.empty() && input) {
            bool isMenuChanged = false;

            if (isHorizontal_) {
                // 左キー (A または LEFT)
                if (input->IsKeyPressedDIK(0x1E /* A */) || input->IsKeyPressedDIK(0xCB /* LEFT */)) {
                    selectedIndex_--;
                    if (selectedIndex_ < 0) {
                        selectedIndex_ = static_cast<int>(items_.size()) - 1;
                    }
                    isMenuChanged = true;
                }
                // 右キー (D または RIGHT)
                if (input->IsKeyPressedDIK(0x20 /* D */) || input->IsKeyPressedDIK(0xCD /* RIGHT */)) {
                    selectedIndex_++;
                    if (selectedIndex_ >= static_cast<int>(items_.size())) {
                        selectedIndex_ = 0;
                    }
                    isMenuChanged = true;
                }
            } else {
                // 上キー (W または UP)
                if (input->IsKeyPressedDIK(0x11 /* W */) || input->IsKeyPressedDIK(0xC8 /* UP */)) {
                    selectedIndex_--;
                    if (selectedIndex_ < 0) {
                        selectedIndex_ = static_cast<int>(items_.size()) - 1;
                    }
                    isMenuChanged = true;
                }
                // 下キー (S または DOWN)
                if (input->IsKeyPressedDIK(0x1F /* S */) || input->IsKeyPressedDIK(0xD0 /* DOWN */)) {
                    selectedIndex_++;
                    if (selectedIndex_ >= static_cast<int>(items_.size())) {
                        selectedIndex_ = 0;
                    }
                    isMenuChanged = true;
                }
            }

            if (isMenuChanged) {
                animator_.Reset(); // 項目切り替え時にアニメーションをリセット
            }

            // 決定キー (Space または Enter または テンキーのEnter)
            if (input->IsKeyPressedDIK(0x39 /* Space */) || 
                input->IsKeyPressedDIK(0x1C /* Enter */) || 
                input->IsKeyPressedDIK(0x9C /* Numpad Enter */)) {
                isDecided_ = true;
                animator_.Reset(); // 決定時にアニメーションをリセット
                transitionDelayTimer_ = 0.0f;
            }
        }

        // 選択状況に応じて色を適用
        float animAlpha = animator_.GetPulseAlpha(0.6f, 0.4f, 3.0f); // PromptController風のゆっくりした明滅
        Vector4 currentActiveColor = activeBaseColor_;
        currentActiveColor.w *= animAlpha;

        for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
            Vector4 applyColor = (i == selectedIndex_) ? currentActiveColor : inactiveColor_;
            
            if (std::holds_alternative<Sprite*>(items_[i])) {
                if (auto* sprite = std::get<Sprite*>(items_[i])) {
                    sprite->SetColor(applyColor);
                }
            } else if (std::holds_alternative<StaticModelObject*>(items_[i])) {
                if (auto* obj = std::get<StaticModelObject*>(items_[i])) {
                    // StaticModelObject には SetColor があるが、Alphaだけを別枠で設定するなら SetAlpha 等を呼ぶ
                    obj->SetColor(applyColor);
                }
            }
        }
    } else {
        // --- 決定後 ---
        transitionDelayTimer_ += 1.0f / 60.0f;
        
        // 決定されたアイテムだけを高速フラッシュ
        isVisible_ = animator_.GetFlashVisibility(40.0f);
        
        for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
            if (i == selectedIndex_) {
                // 選択されたものはフラッシュ
                Vector4 applyColor = activeBaseColor_;
                applyColor.w = isVisible_ ? 1.0f : 0.0f; // フラッシュ用にアルファを切り替え
                
                if (std::holds_alternative<Sprite*>(items_[i])) {
                    if (auto* sprite = std::get<Sprite*>(items_[i])) sprite->SetColor(applyColor);
                } else if (std::holds_alternative<StaticModelObject*>(items_[i])) {
                    if (auto* obj = std::get<StaticModelObject*>(items_[i])) {
                        // isVisible_ でDraw()自体を弾く方式もあるが、StaticModelObjectに透明度を入れて見えなくする
                        obj->SetColor(applyColor);
                    }
                }
            } else {
                // 選ばれなかったものは非表示、もしくはそのまま
                Vector4 invisibleColor = inactiveColor_;
                invisibleColor.w = 0.0f;
                if (std::holds_alternative<Sprite*>(items_[i])) {
                    if (auto* sprite = std::get<Sprite*>(items_[i])) sprite->SetColor(invisibleColor);
                } else if (std::holds_alternative<StaticModelObject*>(items_[i])) {
                    if (auto* obj = std::get<StaticModelObject*>(items_[i])) obj->SetColor(invisibleColor);
                }
            }
        }
    }
}

void UISelectionGroup::Draw() {
    if (items_.empty()) return;

    for (auto& item : items_) {
        if (std::holds_alternative<Sprite*>(item)) {
            if (auto* sprite = std::get<Sprite*>(item)) {
                // 透明な場合は描画をスキップするなどの最適化も可能だが一旦そのまま呼ぶ
                if (sprite->GetColor().w > 0.0f) {
                    sprite->Draw();
                }
            }
        } else if (std::holds_alternative<StaticModelObject*>(item)) {
            if (auto* obj = std::get<StaticModelObject*>(item)) {
                if (obj->GetColor().w > 0.0f) {
                    obj->Draw();
                }
            }
        }
    }
}

bool UISelectionGroup::ShouldTransition() const {
    return isDecided_ && (transitionDelayTimer_ >= kTransitionDelayLimit);
}

