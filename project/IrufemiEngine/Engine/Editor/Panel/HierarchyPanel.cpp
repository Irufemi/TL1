#include "HierarchyPanel.h"

#ifdef EditorMode
#include "imgui/imgui.h"
#include "Engine/Manager/EditorManager.h"
#include "Engine/IrufemiEngine.h"
#include "Framework/SceneManager.h"
#include "Framework/BaseScene.h"
#include "Framework/GameObject.h"
#include "../Core/EditorActionManager.h"
#include "../Core/EditorDragDrop.h"
#include "Framework/SceneSerializer.h"

#include <functional>
#include <algorithm>

void HierarchyPanel::Initialize(EditorManager* editorManager) {
    editorManager_ = editorManager;
}

void HierarchyPanel::Draw() {
    if (!editorManager_) return;

    ImGui::Begin("Hierarchy");

    auto* engine = editorManager_->GetEngine();
    if (engine && engine->GetSceneManager()) {
        auto* currentScene = engine->GetSceneManager()->GetCurrentScene();
        auto* baseScene = dynamic_cast<BaseScene*>(currentScene);

        if (baseScene) {
            // 背景クリックなどで選択解除する機能
            if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered()) {
                editorManager_->ClearSelectedObject();
            }

            // 循環参照チェック用ラムダ
            auto IsDescendant = [](std::shared_ptr<GameObject> potentialDescendant, GameObject* ancestor) {
                if (!potentialDescendant || !ancestor) return false;
                auto current = potentialDescendant->GetParent();
                while (current) {
                    if (current.get() == ancestor) return true;
                    current = current->GetParent();
                }
                return false;
            };

            // 再帰描画用ラムダ関数
            std::function<void(std::shared_ptr<GameObject>)> DrawNode = [&](std::shared_ptr<GameObject> obj) {
                if (!obj) return;

                bool isSelected = false;
                if (auto selected = editorManager_->GetSelectedObject()) {
                    isSelected = (selected == obj);
                }

                // 描画開始時点での子の有無を保存（途中で子が追加されても不整合を起こさないため）
                bool hasChildrenAtStart = !obj->GetChildren().empty();

                // 子がいればツリーノード、いなければリーフ
                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
                if (!hasChildrenAtStart) {
                    flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                }
                if (isSelected) {
                    flags |= ImGuiTreeNodeFlags_Selected;
                }

                // IDスタックでチェックボックス名の競合を防ぐ
                ImGui::PushID(obj.get());

                // 識別用にポインタアドレスを使う
                bool isOpen = ImGui::TreeNodeEx((void*)obj.get(), flags, "%s", obj->GetName().c_str());

                // クリックで選択 (TreeNodeExがクリックされたかを判定)
                if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                    editorManager_->SetSelectedObject(obj);
                }

                // 右端にActive切り替えのチェックボックスを配置
                ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 30.0f);
                bool isActive = obj->GetIsActive();
                if (ImGui::Checkbox("##Active", &isActive)) {
                    obj->SetIsActive(isActive);
                }
                
                ImGui::PopID();

                // --- Drag and Drop Source ---
                if (ImGui::BeginDragDropSource()) {
                    GameObject* ptr = obj.get();
                    ImGui::SetDragDropPayload(EditorDragDrop::PayloadGameObject, &ptr, sizeof(GameObject*));
                    ImGui::Text("Move %s", obj->GetName().c_str());
                    ImGui::EndDragDropSource();
                }

                // --- Drag and Drop Target ---
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(EditorDragDrop::PayloadGameObject)) {
                        GameObject* payload_ptr = *(GameObject**)payload->Data;
                        
                        // 自分自身にはDropできない
                        // また、ドロップされるオブジェクトが「今の自分の親（先祖）」であってはならない（循環参照の防止）
                        if (payload_ptr != obj.get() && !IsDescendant(obj, payload_ptr)) {
                            if (auto dropObj = baseScene->FindGameObject(payload_ptr)) {
                                dropObj->SetParent(obj);
                            }
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                // コンテキストメニュー (右クリック)
                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::Selectable("Duplicate")) {
                        if (auto am = editorManager_->GetActionManager()) {
                            am->DuplicateObject(obj);
                        }
                    }
                    if (ImGui::Selectable("Delete")) {
                        if (auto am = editorManager_->GetActionManager()) {
                            am->DeleteObject(obj);
                        }
                    }
                    ImGui::Separator();
                    if (ImGui::Selectable("Save as Prefab")) {
                        std::string path = "resources/prefabs/" + obj->GetName() + ".prefab.json";
                        SceneSerializer::SavePrefab(obj, path);
                    }
                    ImGui::EndPopup();
                }

                // 子ノードの描画
                if (isOpen && hasChildrenAtStart) {
                    // vector のコピーを回す（描画中に要素が削除・追加されても安全なように）
                    auto childrenCopy = obj->GetChildren();
                    for (auto& child : childrenCopy) {
                        DrawNode(child);
                    }
                    ImGui::TreePop();
                }
            };

            const auto& gameObjects = baseScene->GetGameObjects();
            auto gameObjectsCopy = gameObjects; // 描画中のリスト変更対策
            for (auto& obj : gameObjectsCopy) {
                // ルートオブジェクトのみを描画開始（子は再帰的に呼ばれる）
                if (obj && !obj->GetParent()) {
                    DrawNode(obj);
                }
            }

            // --- 余白でのD&D（ルートへ移動 ＆ アセット配置） ---
            ImGui::InvisibleButton("HierarchyDropZone", ImGui::GetContentRegionAvail());
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(EditorDragDrop::PayloadGameObject)) {
                    GameObject* payload_ptr = *(GameObject**)payload->Data;
                    if (auto obj = baseScene->FindGameObject(payload_ptr)) {
                        obj->SetParent(nullptr); // 親を解除してルートに
                    }
                }
                // --- アセットのドロップを受け付ける ---
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(EditorDragDrop::PayloadAssetPath)) {
                    std::string droppedPathStr = static_cast<const char*>(payload->Data);
                    if (auto am = editorManager_->GetActionManager()) {
                        am->CreateObjectFromAsset(droppedPathStr);
                    }
                }
                ImGui::EndDragDropTarget();
            }

            // --- 全体の空白での右クリック「Create」メニューを表示 ---
            if (ImGui::BeginPopupContextItem("HierarchyContextMenu", ImGuiPopupFlags_MouseButtonRight)) {
                if (auto am = editorManager_->GetActionManager()) {
                    if (ImGui::Selectable("Create Empty")) am->CreatePrimitiveObject("Empty");
                    
                    if (ImGui::BeginMenu("3D Object")) {
                        if (ImGui::Selectable("Cube")) am->CreatePrimitiveObject("Cube");
                        if (ImGui::Selectable("Sphere")) am->CreatePrimitiveObject("Sphere");
                        if (ImGui::Selectable("Cylinder")) am->CreatePrimitiveObject("Cylinder");
                        if (ImGui::Selectable("Plane")) am->CreatePrimitiveObject("Plane");
                        ImGui::Separator();
                        if (ImGui::Selectable("Model (MeshRenderer)")) am->CreatePrimitiveObject("Model");
                        ImGui::EndMenu();
                    }
                    
                    if (ImGui::BeginMenu("2D Object")) {
                        if (ImGui::Selectable("Sprite")) am->CreatePrimitiveObject("Sprite");
                        ImGui::EndMenu();
                    }
                }
                ImGui::EndPopup();
            }
            // --- Deleteキーでの削除ロジックは EditorShortcutManager に移譲したため削除 ---
        }
    }

    ImGui::End();
}
#endif // EditorMode
