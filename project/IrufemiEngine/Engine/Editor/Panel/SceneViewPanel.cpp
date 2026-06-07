#include "SceneViewPanel.h"

#ifdef EditorMode
#include "imgui/imgui.h"
#include "Engine/Manager/EditorManager.h"
#include "Engine/IrufemiEngine.h"
#include "Engine/Graphics/DirectX/RenderTexture.h"
#include "Engine/Manager/CollisionManager.h"
#include "../Core/EditorActionManager.h"
#include "../Core/EditorDragDrop.h"
#include "Engine/Core/Math/MathFunction.h"
#include "Framework/GameObject.h"
#include "Framework/IScene.h"
#include "Framework/Component/Renderer/MeshRendererComponent.h"
#include "Framework/Component/Renderer/PrimitiveRendererComponent.h"
#include "Framework/Component/Renderer/SpriteRendererComponent.h"
#include "Framework/Component/Renderer/TextRendererComponent.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Collider/AABBColliderComponent.h"
#include "Framework/Component/Collider/OBBColliderComponent.h"
#include "Framework/Component/Collider/SphereColliderComponent.h"
#include "Engine/Core/Math/Geometry/Collision.h"
#include "Engine/Platform/Input/InputManager.h"
#include "Engine/Platform/Input/Mouse.h"
#include "Engine/Platform/Input/Keyboard.h"
#include <algorithm>

void SceneViewPanel::Initialize(EditorManager* editorManager) {
    editorManager_ = editorManager;
}

void SceneViewPanel::Draw() {
    if (!editorManager_) return;

    ImGui::Begin("Scene");

    // デバッグ線の描画ON/OFF
    bool* drawCollider = CollisionManager::GetInstance().GetIsDrawDebugLinePtr();
    if (drawCollider) {
        ImGui::Checkbox("Draw Colliders", drawCollider);
    }
    
    ImGui::SameLine();
    if (ImGui::RadioButton("Translate", currentGizmoOperation_ == ImGuizmo::TRANSLATE)) currentGizmoOperation_ = ImGuizmo::TRANSLATE;
    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate", currentGizmoOperation_ == ImGuizmo::ROTATE)) currentGizmoOperation_ = ImGuizmo::ROTATE;
    ImGui::SameLine();
    if (ImGui::RadioButton("Scale", currentGizmoOperation_ == ImGuizmo::SCALE)) currentGizmoOperation_ = ImGuizmo::SCALE;
    ImGui::SameLine();
    if (ImGui::RadioButton("Bounds", currentGizmoOperation_ == ImGuizmo::BOUNDS)) currentGizmoOperation_ = ImGuizmo::BOUNDS;
    
    ImGui::SameLine();
    ImGui::Text("|");
    ImGui::SameLine();
    
    if (ImGui::RadioButton("Local", currentGizmoMode_ == ImGuizmo::LOCAL)) currentGizmoMode_ = ImGuizmo::LOCAL;
    ImGui::SameLine();
    if (ImGui::RadioButton("World", currentGizmoMode_ == ImGuizmo::WORLD)) currentGizmoMode_ = ImGuizmo::WORLD;

    auto* engine = editorManager_->GetEngine();
    if (engine && engine->GetMainRenderTexture()) {
        auto mainTexture = engine->GetMainRenderTexture();
        
        // パネルの大きさを取得して画像をフィットさせる（16:9を維持する）
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float aspect = 1280.0f / 720.0f;
        ImVec2 size;
        if (avail.x / avail.y > aspect) {
            size.y = avail.y;
            size.x = size.y * aspect;
        } else {
            size.x = avail.x;
            size.y = size.x / aspect;
        }

        // 中央揃えにするためのカーソル位置調整
        ImVec2 cursor = ImGui::GetCursorPos();
        cursor.x += (avail.x - size.x) * 0.5f;
        cursor.y += (avail.y - size.y) * 0.5f;
        ImGui::SetCursorPos(cursor);

        ImGui::Image((ImTextureID)mainTexture->GetSrvHandleGPU().ptr, size);
        
        ImVec2 minPos = ImGui::GetItemRectMin(); // ImGui::Image() の左上
        ImVec2 maxPos = ImGui::GetItemRectMax(); // ImGui::Image() の右下

        // --- 選択中のSpriteに対するアウトライン（強調枠）描画 ---
        if (auto selectedObj = editorManager_->GetSelectedObject()) {
            if (auto spriteComp = selectedObj->GetComponent<SpriteRendererComponent>()) {
                if (auto transform = selectedObj->GetComponent<TransformComponent>()) {
                    auto sprite = spriteComp->GetSprite();
                    if (sprite) {
                        Vector2 sizeScaled = sprite->GetSize();
                        Vector2 anchor = sprite->GetAnchor();
                        Vector3 pos = transform->worldPosition_;
                        
                        float left = pos.x - sizeScaled.x * anchor.x;
                        float top = pos.y - sizeScaled.y * anchor.y;
                        float right = pos.x + sizeScaled.x * (1.0f - anchor.x);
                        float bottom = pos.y + sizeScaled.y * (1.0f - anchor.y);
                        
                        float scaleX = size.x / 1280.0f;
                        float scaleY = size.y / 720.0f;
                        
                        ImVec2 pMin = ImVec2(minPos.x + left * scaleX, minPos.y + top * scaleY);
                        ImVec2 pMax = ImVec2(minPos.x + right * scaleX, minPos.y + bottom * scaleY);
                        
                        ImGui::GetWindowDrawList()->AddRect(pMin, pMax, IM_COL32(255, 165, 0, 255), 0.0f, 0, 2.0f);
                    }
                }
            } else if (auto textComp = selectedObj->GetComponent<TextRendererComponent>()) {
                if (auto transform = selectedObj->GetComponent<TransformComponent>()) {
                    Vector3 pos = transform->worldPosition_;
                    Vector2 minBounds = textComp->GetLocalBoundsMin();
                    Vector2 maxBounds = textComp->GetLocalBoundsMax();
                    
                    float left = pos.x + minBounds.x * transform->worldScale_.x;
                    float right = pos.x + maxBounds.x * transform->worldScale_.x;
                    float top = pos.y + minBounds.y * transform->worldScale_.y;
                    float bottom = pos.y + maxBounds.y * transform->worldScale_.y;
                    
                    float scaleX = size.x / 1280.0f;
                    float scaleY = size.y / 720.0f;
                    
                    ImVec2 pMin = ImVec2(minPos.x + left * scaleX, minPos.y + top * scaleY);
                    ImVec2 pMax = ImVec2(minPos.x + right * scaleX, minPos.y + bottom * scaleY);
                    
                    ImGui::GetWindowDrawList()->AddRect(pMin, pMax, IM_COL32(0, 255, 255, 255), 0.0f, 0, 2.0f);
                }
            }
        }

        DrawImGuizmo(minPos, size);
        HandleDragAndDrop();

        // --- UI用の仮想マウス座標更新 & クリックによる3Dピッキング ---
        if (ImGui::IsWindowHovered()) {
            ImVec2 mousePos = ImGui::GetMousePos(); // 画面全体の座標

            if (mousePos.x >= minPos.x && mousePos.x <= maxPos.x &&
                mousePos.y >= minPos.y && mousePos.y <= maxPos.y) {
                
                Vector2 localMousePos = { mousePos.x - minPos.x, mousePos.y - minPos.y };
                float scaleX = 1280.0f / size.x;
                float scaleY = 720.0f / size.y;
                Vector2 scaledVirtualPos = { localMousePos.x * scaleX, localMousePos.y * scaleY };
                
                engine->GetInputManager()->SetVirtualMousePosition(scaledVirtualPos, true);
                
                if (auto camera = engine->GetCameraManager()->GetActiveCamera()) {
                    cameraController_.UpdateCameraInput(camera, engine->GetInputManager());
                }

                HandlePicking(mousePos, minPos, maxPos, size);
            } else {
                engine->GetInputManager()->SetVirtualMousePosition({0.0f, 0.0f}, false);
            }
        } else {
            if (engine && engine->GetInputManager()) {
                engine->GetInputManager()->SetVirtualMousePosition({0.0f, 0.0f}, false);
            }
        }
    }

    ImGui::End();
}

void SceneViewPanel::DrawImGuizmo(ImVec2 minPos, ImVec2 size) {
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(minPos.x, minPos.y, size.x, size.y);

    auto* engine = editorManager_->GetEngine();
    if (auto selectedObj = editorManager_->GetSelectedObject()) {
        if (auto camera = engine->GetCameraManager()->GetActiveCamera()) {
            Matrix4x4 view = camera->GetViewMatrix();
            Matrix4x4 proj = camera->GetPerspectiveFovMatrix();
            
            if (auto transform = selectedObj->GetComponent<TransformComponent>()) {
                Matrix4x4 world = transform->GetWorldMatrix();
                bool manipulated = false;
                
                if (currentGizmoOperation_ == ImGuizmo::BOUNDS) {
                    // コライダーのリサイズ操作
                    if (auto aabbCol = selectedObj->GetComponent<AABBColliderComponent>()) {
                        Vector3 offset = aabbCol->GetLocalOffset();
                        Vector3 csize = aabbCol->GetLocalSize();
                        float bounds[6] = {
                            offset.x - csize.x, offset.y - csize.y, offset.z - csize.z,
                            offset.x + csize.x, offset.y + csize.y, offset.z + csize.z
                        };
                        if (ImGuizmo::Manipulate(&view.m[0][0], &proj.m[0][0], currentGizmoOperation_, currentGizmoMode_, &world.m[0][0], nullptr, nullptr, bounds)) {
                            aabbCol->SetLocalOffset({
                                (bounds[0] + bounds[3]) * 0.5f,
                                (bounds[1] + bounds[4]) * 0.5f,
                                (bounds[2] + bounds[5]) * 0.5f
                            });
                            aabbCol->SetLocalSize({
                                (bounds[3] - bounds[0]) * 0.5f,
                                (bounds[4] - bounds[1]) * 0.5f,
                                (bounds[5] - bounds[2]) * 0.5f
                            });
                        }
                    } else if (auto obbCol = selectedObj->GetComponent<OBBColliderComponent>()) {
                        Vector3 offset = obbCol->GetLocalOffset();
                        Vector3 csize = obbCol->GetLocalSize();
                        float bounds[6] = {
                            offset.x - csize.x, offset.y - csize.y, offset.z - csize.z,
                            offset.x + csize.x, offset.y + csize.y, offset.z + csize.z
                        };
                        if (ImGuizmo::Manipulate(&view.m[0][0], &proj.m[0][0], currentGizmoOperation_, currentGizmoMode_, &world.m[0][0], nullptr, nullptr, bounds)) {
                            obbCol->SetLocalOffset({
                                (bounds[0] + bounds[3]) * 0.5f,
                                (bounds[1] + bounds[4]) * 0.5f,
                                (bounds[2] + bounds[5]) * 0.5f
                            });
                            obbCol->SetLocalSize({
                                (bounds[3] - bounds[0]) * 0.5f,
                                (bounds[4] - bounds[1]) * 0.5f,
                                (bounds[5] - bounds[2]) * 0.5f
                            });
                        }
                    } else if (auto sphereCol = selectedObj->GetComponent<SphereColliderComponent>()) {
                        ImGuizmo::OPERATION op = ImGuizmo::SCALE;
                        if (ImGuizmo::Manipulate(&view.m[0][0], &proj.m[0][0], op, currentGizmoMode_, &world.m[0][0])) {
                            manipulated = true;
                        }
                    } else {
                        ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
                        if (ImGuizmo::Manipulate(&view.m[0][0], &proj.m[0][0], op, currentGizmoMode_, &world.m[0][0])) {
                            manipulated = true;
                        }
                    }
                } else {
                    if (ImGuizmo::Manipulate(&view.m[0][0], &proj.m[0][0], currentGizmoOperation_, currentGizmoMode_, &world.m[0][0])) {
                        manipulated = true;
                    }
                }

                if (manipulated) {
                    Vector3 pos, rot, mscale;
                    ImGuizmo::DecomposeMatrixToComponents(&world.m[0][0], &pos.x, &rot.x, &mscale.x);
                    
                    rot.x = rot.x * Math::PI / 180.0f;
                    rot.y = rot.y * Math::PI / 180.0f;
                    rot.z = rot.z * Math::PI / 180.0f;
                    
                    transform->position_ = pos;
                    transform->rotation_ = rot;
                    transform->scale_ = mscale;
                }
            }
        }
    }
}

void SceneViewPanel::HandleDragAndDrop() {
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(EditorDragDrop::PayloadAssetPath)) {
            std::string droppedPathStr = static_cast<const char*>(payload->Data);
            if (auto am = editorManager_->GetActionManager()) {
                am->CreateObjectFromAsset(droppedPathStr);
            }
        }
        ImGui::EndDragDropTarget();
    }
}

void SceneViewPanel::HandlePicking(ImVec2 mousePos, ImVec2 minPos, ImVec2 maxPos, ImVec2 size) {
    // プレイモード中（ゲーム進行中）はインゲームのクリック操作（射撃など）と競合するためピッキングを無効にする
    if (editorManager_->IsPlayMode()) {
        return;
    }

    bool isGizmoUsing = ImGuizmo::IsUsing() || ImGuizmo::IsOver();
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !isGizmoUsing) {
        auto* engine = editorManager_->GetEngine();
        bool isHit = false;
        GameObject* closestObj = nullptr;
        float closestDist = 1000.0f;
        
        Vector2 localMousePos = { mousePos.x - minPos.x, mousePos.y - minPos.y };
        float scaleX = 1280.0f / size.x;
        float scaleY = 720.0f / size.y;
        Vector2 scaledVirtualPos = { localMousePos.x * scaleX, localMousePos.y * scaleY };

        // --- 1. まず 2D (Sprite) のピッキング判定を行う ---
        if (auto scene = engine->GetSceneManager()->GetCurrentScene()) {
            auto gameObjects = scene->GetGameObjects();
            for (auto it = gameObjects.rbegin(); it != gameObjects.rend(); ++it) {
                auto& obj = *it;
                if (!obj || obj->IsDestroyed() || !obj->GetIsActive()) continue;
                
                if (auto spriteComp = obj->GetComponent<SpriteRendererComponent>()) {
                    if (auto transform = obj->GetComponent<TransformComponent>()) {
                        auto sprite = spriteComp->GetSprite();
                        if (sprite) {
                            Vector2 sizeScaled = sprite->GetSize();
                            Vector2 anchor = sprite->GetAnchor();
                            Vector3 pos = transform->worldPosition_;
                            
                            float left = pos.x - sizeScaled.x * anchor.x;
                            float top = pos.y - sizeScaled.y * anchor.y;
                            float right = pos.x + sizeScaled.x * (1.0f - anchor.x);
                            float bottom = pos.y + sizeScaled.y * (1.0f - anchor.y);
                            
                            if (scaledVirtualPos.x >= left && scaledVirtualPos.x <= right &&
                                scaledVirtualPos.y >= top && scaledVirtualPos.y <= bottom) {
                                closestObj = obj.get();
                                isHit = true;
                                break;
                            }
                        }
                    }
                }
                
                if (!isHit) {
                    if (auto textComp = obj->GetComponent<TextRendererComponent>()) {
                        if (auto transform = obj->GetComponent<TransformComponent>()) {
                            Vector3 pos = transform->worldPosition_;
                            Vector2 minBounds = textComp->GetLocalBoundsMin();
                            Vector2 maxBounds = textComp->GetLocalBoundsMax();
                            
                            float left = pos.x + minBounds.x * transform->worldScale_.x;
                            float right = pos.x + maxBounds.x * transform->worldScale_.x;
                            float top = pos.y + minBounds.y * transform->worldScale_.y;
                            float bottom = pos.y + maxBounds.y * transform->worldScale_.y;
                            
                            if (scaledVirtualPos.x >= left && scaledVirtualPos.x <= right &&
                                scaledVirtualPos.y >= top && scaledVirtualPos.y <= bottom) {
                                closestObj = obj.get();
                                isHit = true;
                                break;
                            }
                        }
                    }
                }
            }
        }

        // --- 2. Sprite に当たらなかった場合のみ 3D のピッキングを行う ---
        if (!isHit) {
            if (auto camera = engine->GetCameraManager()->GetActiveCamera()) {
                Matrix4x4 viewProj = camera->GetViewProjectionMatrix3D();
                Matrix4x4 viewProjInverse = Math::Inverse(viewProj);
                Ray ray = Math::ScreenPointToRay(localMousePos, size.x, size.y, viewProjInverse);

                RaycastHit hit;
                if (CollisionManager::GetInstance().Raycast(ray, hit, 1000.0f)) {
                    closestDist = hit.distance;
                    closestObj = hit.hitObject;
                    isHit = true;
                }
                
                if (auto scene = engine->GetSceneManager()->GetCurrentScene()) {
                    for (auto& obj : scene->GetGameObjects()) {
                        if (!obj || obj.get() == closestObj) continue;
                        
                        float dist = 0.0f;
                        for (auto& comp : obj->GetComponents()) {
                            if (comp->Raycast(ray, dist)) {
                                if (dist < closestDist) {
                                    closestDist = dist;
                                    closestObj = obj.get();
                                    isHit = true;
                                }
                            }
                        }
                    }
                }
            }
        }

        if (isHit && closestObj) {
            editorManager_->SetSelectedObject(closestObj->shared_from_this());
        } else {
            editorManager_->ClearSelectedObject();
        }
    }
}
#endif // EditorMode
