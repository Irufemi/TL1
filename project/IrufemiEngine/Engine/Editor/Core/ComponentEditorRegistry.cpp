#include "ComponentEditorRegistry.h"

#ifdef EditorMode
#include <imgui/imgui.h>
#include <filesystem>
#include <algorithm>
#include <string>

// Engine/Framework
#include "Engine/Manager/EditorManager.h"
#include "Engine/Manager/CollisionManager.h"
#include "Engine/IrufemiEngine.h"
#include "Renderer/Object3D/StaticModelObject/StaticModelObject.h"
#include "Resource/Model/ModelManager.h"
#include "Resource/Texture/TextureManager.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/GameObject.h"
#include "Framework/Component/Renderer/MeshRendererComponent.h"
#include "Framework/Component/Renderer/PrimitiveRendererComponent.h"
#include "Framework/Component/Renderer/SpriteRendererComponent.h"
#include "Framework/Component/Renderer/TextRendererComponent.h"
#include "Framework/Component/Collider/AABBColliderComponent.h"
#include "Framework/Component/Collider/OBBColliderComponent.h"
#include "Framework/Component/Collider/SphereColliderComponent.h"
#include "Framework/Component/Collider/RaycastComponent.h"
#include "Framework/Component/Renderer/ParticleEmitterComponent.h"
#include "Renderer/ParticleGPU/ParticleObject.h"
#include "Resource/Texture/TextureManager.h"
#include "Framework/Component/Script/RotatorComponent.h"
#include "Engine/Core/Utility/StringUtility.h"

// Core
#include "IComponentEditor.h"
#include "EditorCommands.h"
#include "EditorActionManager.h"

// --- Undo/Redo Helpers ---
template <typename T>
static void CheckUndoRedoDrag(EditorActionManager* actionManager, T* valuePtr) {
    static T startValue;
    if (ImGui::IsItemActivated()) {
        startValue = *valuePtr;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        T endValue = *valuePtr;
        actionManager->PushAndExecute(std::make_unique<ChangeValueCommand<T>>(
            startValue, endValue, [valuePtr](const T& v) { *valuePtr = v; }));
    }
}

template <typename T>
static void CheckUndoRedoDrag(EditorActionManager* actionManager, T* valuePtr, std::function<void(const T&)> setter) {
    static T startValue;
    if (ImGui::IsItemActivated()) {
        startValue = *valuePtr;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        T endValue = *valuePtr;
        actionManager->PushAndExecute(std::make_unique<ChangeValueCommand<T>>(
            startValue, endValue, setter));
    }
}

template <typename T>
static void PushInstantUndo(EditorActionManager* actionManager, const T& oldVal, const T& newVal, T* valuePtr) {
    actionManager->PushAndExecute(std::make_unique<ChangeValueCommand<T>>(
        oldVal, newVal, [valuePtr](const T& v) { *valuePtr = v; }));
}

template <typename T>
static void PushInstantUndo(EditorActionManager* actionManager, const T& oldVal, const T& newVal, std::function<void(const T&)> setter) {
    actionManager->PushAndExecute(std::make_unique<ChangeValueCommand<T>>(
        oldVal, newVal, setter));
}

// --- Helper Functions ---
static void DrawCollisionLayerGUI(EditorActionManager* actionManager, uint32_t& layer, uint32_t& mask) {
    ImGui::Separator();
    ImGui::Text("Collision Settings");

    auto& cm = CollisionManager::GetInstance();
    const auto& layerNames = cm.GetLayerNames();

    if (layerNames.empty()) return;

    int currentLayerIndex = 0;
    for (int i = 0; i < layerNames.size(); ++i) {
        if (layer == (1u << i)) {
            currentLayerIndex = i;
            break;
        }
    }

    if (ImGui::BeginCombo("Layer", layerNames[currentLayerIndex].c_str())) {
        for (int i = 0; i < layerNames.size(); ++i) {
            bool isSelected = (currentLayerIndex == i);
            if (ImGui::Selectable(layerNames[i].c_str(), isSelected)) {
                uint32_t newLayer = (1u << i);
                PushInstantUndo(actionManager, layer, newLayer, &layer);
            }
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (ImGui::TreeNode("Collision Mask")) {
        if (ImGui::Button("All")) {
            PushInstantUndo(actionManager, mask, 0xFFFFFFFF, &mask);
        }
        ImGui::SameLine();
        if (ImGui::Button("None")) {
            PushInstantUndo(actionManager, mask, 0u, &mask);
        }

        for (int i = 0; i < layerNames.size(); ++i) {
            bool isMasked = (mask & (1u << i)) != 0;
            if (ImGui::Checkbox(layerNames[i].c_str(), &isMasked)) {
                uint32_t newMask = mask;
                if (isMasked) newMask |= (1u << i);
                else          newMask &= ~(1u << i);
                PushInstantUndo(actionManager, mask, newMask, &mask);
            }
        }
        ImGui::TreePop();
    }

    if (ImGui::Button("Edit Layers...")) {
        ImGui::OpenPopup("Edit Layers");
    }

    if (ImGui::BeginPopupModal("Edit Layers", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Manage Collision Layers");
        ImGui::Separator();

        for (int i = 0; i < layerNames.size(); ++i) {
            ImGui::PushID(i);
            ImGui::Text("%2d: ", i);
            ImGui::SameLine();
            
            if (i == 0) {
                ImGui::TextDisabled("%s (Fixed)", layerNames[i].c_str());
            } else {
                char nameBuffer[64];
                strncpy_s(nameBuffer, sizeof(nameBuffer), layerNames[i].c_str(), _TRUNCATE);
                
                ImGui::SetNextItemWidth(150);
                if (ImGui::InputText("##Name", nameBuffer, sizeof(nameBuffer))) {
                    cm.RenameLayer(i, nameBuffer);
                }
                ImGui::SameLine();
                if (ImGui::Button("X")) {
                    cm.RemoveLayer(i);
                    ImGui::PopID();
                    break; 
                }
            }
            ImGui::PopID();
        }

        if (layerNames.size() < 32) {
            if (ImGui::Button("+ Add New Layer")) {
                cm.AddLayer("New Layer");
            }
        } else {
            ImGui::TextDisabled("Max layers reached (32).");
        }

        ImGui::Separator();
        if (ImGui::Button("Close", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

template<typename T>
static void DrawColliderCommonProperties(T* comp, EditorActionManager* actionManager) {
    Vector3 offset = comp->GetLocalOffset();
    if (ImGui::DragFloat3("Offset", &offset.x, 0.1f)) {
        comp->SetLocalOffset(offset);
    }
    CheckUndoRedoDrag(actionManager, &offset, std::function<void(const Vector3&)>([comp](const Vector3& v){ comp->SetLocalOffset(v); }));
    
    if constexpr (std::is_same_v<T, SphereColliderComponent>) {
        float radius = comp->GetLocalRadius();
        if (ImGui::DragFloat("Radius", &radius, 0.1f, 0.0f, 1000.0f)) {
            comp->SetLocalRadius(radius);
        }
        CheckUndoRedoDrag(actionManager, &radius, std::function<void(const float&)>([comp](const float& v){ comp->SetLocalRadius(v); }));
    } else {
        Vector3 size = comp->GetLocalSize();
        if (ImGui::DragFloat3("Size (Extents)", &size.x, 0.1f, 0.0f, 1000.0f)) {
            comp->SetLocalSize(size);
        }
        CheckUndoRedoDrag(actionManager, &size, std::function<void(const Vector3&)>([comp](const Vector3& v){ comp->SetLocalSize(v); }));
    }
    
    bool isTrigger = comp->isTrigger_;
    if (ImGui::Checkbox("Is Trigger", &isTrigger)) {
        PushInstantUndo(actionManager, comp->isTrigger_, isTrigger, &comp->isTrigger_);
    }
    DrawCollisionLayerGUI(actionManager, comp->layer_, comp->mask_);
}

static void DrawFallbackPropertiesGUI(Component* component, EditorActionManager* actionManager) {
    const auto& props = component->GetProperties();
    if (props.empty()) return;
    
    ImGui::PushID(component);
    if (ImGui::CollapsingHeader(component->GetComponentName().c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const auto& prop : props) {
            switch (prop.type) {
                case ComponentPropertyType::Header: {
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "%s", prop.name.c_str());
                    break;
                }
                case ComponentPropertyType::Separator: {
                    ImGui::Separator();
                    break;
                }
                case ComponentPropertyType::Float: {
                    float* ptr = static_cast<float*>(prop.data);
                    ImGui::DragFloat(prop.name.c_str(), ptr, 0.1f);
                    CheckUndoRedoDrag(actionManager, ptr);
                    break;
                }
                case ComponentPropertyType::Int: {
                    int* ptr = static_cast<int*>(prop.data);
                    if (prop.name.find("Particle Type") != std::string::npos) {
                        const char* items[] = { "0: Custom", "1: Explosion", "2: Spark", "3: Smoke" };
                        int oldVal = *ptr;
                        if (ImGui::Combo("Particle Type", ptr, items, IM_ARRAYSIZE(items))) {
                            PushInstantUndo(actionManager, oldVal, *ptr, ptr);
                        }
                    } else if (prop.name.find("Emit Type") != std::string::npos) {
                        const char* items[] = { "0: Sphere", "1: Beam", "2: Box", "3: Cylinder" };
                        int oldVal = *ptr;
                        if (ImGui::Combo("Emit Type", ptr, items, IM_ARRAYSIZE(items))) {
                            PushInstantUndo(actionManager, oldVal, *ptr, ptr);
                        }
                    } else {
                        ImGui::DragInt(prop.name.c_str(), ptr, 1);
                        CheckUndoRedoDrag(actionManager, ptr);
                    }
                    break;
                }
                case ComponentPropertyType::Bool: {
                    bool* ptr = static_cast<bool*>(prop.data);
                    bool oldVal = *ptr;
                    if (ImGui::Checkbox(prop.name.c_str(), ptr)) {
                        PushInstantUndo(actionManager, oldVal, *ptr, ptr);
                    }
                    break;
                }
                case ComponentPropertyType::Float2: {
                    Vector2* ptr = reinterpret_cast<Vector2*>(prop.data);
                    ImGui::DragFloat2(prop.name.c_str(), &ptr->x, 0.1f);
                    CheckUndoRedoDrag(actionManager, ptr);
                    break;
                }
                case ComponentPropertyType::Float3: {
                    Vector3* ptr = reinterpret_cast<Vector3*>(prop.data);
                    ImGui::DragFloat3(prop.name.c_str(), &ptr->x, 0.1f);
                    CheckUndoRedoDrag(actionManager, ptr);
                    break;
                }
                case ComponentPropertyType::Float4: {
                    Vector4* ptr = reinterpret_cast<Vector4*>(prop.data);
                    if (prop.name.find("Color") != std::string::npos || prop.name.find("color") != std::string::npos) {
                        ImGui::ColorEdit4(prop.name.c_str(), &ptr->x);
                    } else {
                        ImGui::DragFloat4(prop.name.c_str(), &ptr->x, 0.1f);
                    }
                    CheckUndoRedoDrag(actionManager, ptr);
                    break;
                }
                case ComponentPropertyType::String: {
                    auto* str = static_cast<std::string*>(prop.data);
                    char buffer[256];
                    strncpy_s(buffer, sizeof(buffer), str->c_str(), _TRUNCATE);
                    
                    static std::string startStr;
                    if (ImGui::InputText(prop.name.c_str(), buffer, sizeof(buffer))) {
                        *str = buffer;
                    }
                    if (ImGui::IsItemActivated()) {
                        startStr = *str;
                    }
                    if (ImGui::IsItemDeactivatedAfterEdit()) {
                        std::string endStr = *str;
                        actionManager->PushAndExecute(std::make_unique<ChangeValueCommand<std::string>>(
                            startStr, endStr, [str](const std::string& v) { *str = v; }));
                    }
                    break;
                }
                case ComponentPropertyType::Float3Array: {
                    auto* arr = static_cast<std::vector<Vector3>*>(prop.data);
                    if (ImGui::TreeNode(prop.name.c_str())) {
                        int size = static_cast<int>(arr->size());
                        int oldSize = size;
                        if (ImGui::InputInt("Size", &size)) {
                            if (size >= 0) {
                                std::vector<Vector3> oldArr = *arr;
                                arr->resize(size);
                                std::vector<Vector3> newArr = *arr;
                                PushInstantUndo(actionManager, oldArr, newArr, arr);
                            }
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("+")) {
                            std::vector<Vector3> oldArr = *arr;
                            arr->push_back(Vector3{0, 0, 0});
                            std::vector<Vector3> newArr = *arr;
                            PushInstantUndo(actionManager, oldArr, newArr, arr);
                        }
                        for (size_t i = 0; i < arr->size(); ++i) {
                            ImGui::PushID(static_cast<int>(i));
                            ImGui::DragFloat3("##Element", &(*arr)[i].x, 0.1f);
                            CheckUndoRedoDrag(actionManager, &(*arr)[i]);
                            
                            ImGui::SameLine();
                            if (ImGui::Button("-")) {
                                std::vector<Vector3> oldArr = *arr;
                                arr->erase(arr->begin() + i);
                                std::vector<Vector3> newArr = *arr;
                                PushInstantUndo(actionManager, oldArr, newArr, arr);
                                ImGui::PopID();
                                break; // ループを抜けて次フレームで再描画
                            }
                            ImGui::PopID();
                        }
                        ImGui::TreePop();
                    }
                    break;
                }
            }
        }
    }
    ImGui::PopID();
}

// =======================================================================
// Custom Editors
// =======================================================================

class TransformComponentEditor : public IComponentEditor {
public:
    void Draw(Component* component, EditorActionManager* actionManager) override {
        auto* comp = static_cast<TransformComponent*>(component);
        if (ImGui::TreeNodeEx("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            
            // Position
            static Vector3 startPos;
            if (ImGui::DragFloat3("Position", &comp->position_.x, 0.1f)) {
                // ドラッグ中も値は更新されるがコマンドは積まない
            }
            if (ImGui::IsItemActivated()) startPos = comp->position_;
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                Vector3 endPos = comp->position_;
                actionManager->PushAndExecute(std::make_unique<ChangeValueCommand<Vector3>>(
                    startPos, endPos, [comp](const Vector3& v) { comp->position_ = v; }));
            }
            
            // Rotation
            static Vector3 startRot;
            Vector3 rotDegrees = {
                comp->rotation_.x * (180.0f / 3.14159265f),
                comp->rotation_.y * (180.0f / 3.14159265f),
                comp->rotation_.z * (180.0f / 3.14159265f)
            };
            if (ImGui::DragFloat3("Rotation", &rotDegrees.x, 1.0f)) {
                comp->rotation_.x = rotDegrees.x * (3.14159265f / 180.0f);
                comp->rotation_.y = rotDegrees.y * (3.14159265f / 180.0f);
                comp->rotation_.z = rotDegrees.z * (3.14159265f / 180.0f);
            }
            if (ImGui::IsItemActivated()) startRot = comp->rotation_;
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                Vector3 endRot = comp->rotation_;
                actionManager->PushAndExecute(std::make_unique<ChangeValueCommand<Vector3>>(
                    startRot, endRot, [comp](const Vector3& v) { comp->rotation_ = v; }));
            }

            // Scale
            static Vector3 startScale;
            if (ImGui::DragFloat3("Scale", &comp->scale_.x, 0.1f)) {
            }
            if (ImGui::IsItemActivated()) startScale = comp->scale_;
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                Vector3 endScale = comp->scale_;
                actionManager->PushAndExecute(std::make_unique<ChangeValueCommand<Vector3>>(
                    startScale, endScale, [comp](const Vector3& v) { comp->scale_ = v; }));
            }

            ImGui::TreePop();
        }
    }
};

class MeshRendererComponentEditor : public IComponentEditor {
public:
    void Draw(Component* component, EditorActionManager* actionManager) override {
        auto* comp = static_cast<MeshRendererComponent*>(component);
        if (ImGui::TreeNodeEx("MeshRenderer", ImGuiTreeNodeFlags_DefaultOpen)) {
            IrufemiEngine* engine = BaseModel::GetIrufemiEngine();
            if (engine && engine->GetObjModelManager()) {
                ModelManager* modelManager = engine->GetObjModelManager();
                std::vector<std::string> availableModels = modelManager->GetAvailableModels();
                
                if (std::find(availableModels.begin(), availableModels.end(), comp->modelName_) == availableModels.end()) {
                    availableModels.push_back(comp->modelName_);
                }
                
                ImGui::Text("Model");
                ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 60.0f);
                if (ImGui::Button("Refresh")) {
                    modelManager->RefreshAvailableModels();
                    availableModels = modelManager->GetAvailableModels();
                    if (std::find(availableModels.begin(), availableModels.end(), comp->modelName_) == availableModels.end()) {
                        availableModels.push_back(comp->modelName_);
                    }
                }

                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                if (ImGui::BeginCombo("##ModelCombo", comp->modelName_.c_str())) {
                    for (const auto& key : availableModels) {
                        bool isSelected = (comp->modelName_ == key);
                        if (ImGui::Selectable(key.c_str(), isSelected)) {
                            std::string oldModel = comp->modelName_;
                            std::string newModel = key;
                            PushInstantUndo(actionManager, oldModel, newModel, std::function<void(const std::string&)>([comp](const std::string& v){ comp->LoadModel(v); }));
                        }
                        if (isSelected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            } else {
                ImGui::Text("Model: %s", comp->modelName_.c_str());
            }

            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_ASSET_PATH")) {
                    std::string droppedPathStr = static_cast<const char*>(payload->Data);
                    std::filesystem::path droppedPath(reinterpret_cast<const char8_t*>(droppedPathStr.c_str()));
                    std::string ext = droppedPath.extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    
                    if (ext == ".obj" || ext == ".gltf" || ext == ".fbx" || ext == ".glb") {
                        std::string newModelName = droppedPathStr;
                        std::replace(newModelName.begin(), newModelName.end(), '\\', '/');
                        std::string lowerPath = newModelName;
                        std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);
                        if (lowerPath.find("resources/model/") == 0) {
                            newModelName = newModelName.substr(16);
                        }
                        std::string oldModel = comp->modelName_;
                        PushInstantUndo(actionManager, oldModel, newModelName, std::function<void(const std::string&)>([comp](const std::string& v){ comp->LoadModel(v); }));
                    }
                }
                ImGui::EndDragDropTarget();
            }
            ImGui::TextDisabled("(?) Drag & Drop model file from Project Browser");
            ImGui::TreePop();
        }
    }
};

class PrimitiveRendererComponentEditor : public IComponentEditor {
public:
    void Draw(Component* component, EditorActionManager* actionManager) override {
        auto* comp = static_cast<PrimitiveRendererComponent*>(component);
        if (ImGui::TreeNodeEx("PrimitiveRenderer", ImGuiTreeNodeFlags_DefaultOpen)) {
            const char* typeNames[] = {
                "Triangle", "Plane", "Cube", "Cylinder", "Sphere", 
                "Tetra", "Circle", "Ring", "Skybox", "Cone", 
                "Torus", "IcoSphere", "Grid"
            };
            
            bool needRebuild = false;
            int typeIndex = comp->currentTypeIndex_;
            int oldTypeIndex = typeIndex;
            if (ImGui::Combo("Shape Type", &typeIndex, typeNames, IM_ARRAYSIZE(typeNames))) {
                PushInstantUndo(actionManager, oldTypeIndex, typeIndex, std::function<void(const int&)>([comp](const int& v) { comp->SetShape(static_cast<PrimitiveType>(v)); comp->RebuildMesh(); }));
            }

            PrimitiveType type = static_cast<PrimitiveType>(comp->currentTypeIndex_);
            switch (type) {
                case PrimitiveType::Sphere:
                case PrimitiveType::IcoSphere:
                case PrimitiveType::Circle:
                    if (ImGui::DragFloat("Radius", &comp->radius_, 0.1f, 0.1f, 100.0f)) comp->RebuildMesh();
                    CheckUndoRedoDrag(actionManager, &comp->radius_, std::function<void(const float&)>([comp](const float& v){ comp->radius_ = v; comp->RebuildMesh(); }));
                    if (ImGui::SliderInt("Subdivisions", &comp->subdivisions_, 3, 64)) comp->RebuildMesh();
                    CheckUndoRedoDrag(actionManager, &comp->subdivisions_, std::function<void(const int&)>([comp](const int& v){ comp->subdivisions_ = v; comp->RebuildMesh(); }));
                    break;
                case PrimitiveType::Cylinder:
                    if (ImGui::DragFloat("Top Radius", &comp->topRadius_, 0.1f, 0.0f, 100.0f)) comp->RebuildMesh();
                    CheckUndoRedoDrag(actionManager, &comp->topRadius_, std::function<void(const float&)>([comp](const float& v){ comp->topRadius_ = v; comp->RebuildMesh(); }));
                    if (ImGui::DragFloat("Bottom Radius", &comp->bottomRadius_, 0.1f, 0.0f, 100.0f)) comp->RebuildMesh();
                    CheckUndoRedoDrag(actionManager, &comp->bottomRadius_, std::function<void(const float&)>([comp](const float& v){ comp->bottomRadius_ = v; comp->RebuildMesh(); }));
                    if (ImGui::DragFloat("Height", &comp->height_, 0.1f, 0.1f, 100.0f)) comp->RebuildMesh();
                    CheckUndoRedoDrag(actionManager, &comp->height_, std::function<void(const float&)>([comp](const float& v){ comp->height_ = v; comp->RebuildMesh(); }));
                    if (ImGui::SliderInt("Segments", &comp->subdivisions_, 3, 64)) comp->RebuildMesh();
                    CheckUndoRedoDrag(actionManager, &comp->subdivisions_, std::function<void(const int&)>([comp](const int& v){ comp->subdivisions_ = v; comp->RebuildMesh(); }));
                    
                    {
                        bool hasTop = comp->hasTop_;
                        if (ImGui::Checkbox("Has Top", &hasTop)) {
                            PushInstantUndo(actionManager, comp->hasTop_, hasTop, std::function<void(const bool&)>([comp](const bool& v){ comp->hasTop_ = v; comp->RebuildMesh(); }));
                        }
                        bool hasBottom = comp->hasBottom_;
                        if (ImGui::Checkbox("Has Bottom", &hasBottom)) {
                            PushInstantUndo(actionManager, comp->hasBottom_, hasBottom, std::function<void(const bool&)>([comp](const bool& v){ comp->hasBottom_ = v; comp->RebuildMesh(); }));
                        }
                    }
                    break;
                case PrimitiveType::Cone:
                    if (ImGui::DragFloat("Radius", &comp->radius_, 0.1f, 0.1f, 100.0f)) comp->RebuildMesh();
                    CheckUndoRedoDrag(actionManager, &comp->radius_, std::function<void(const float&)>([comp](const float& v){ comp->radius_ = v; comp->RebuildMesh(); }));
                    if (ImGui::DragFloat("Height", &comp->height_, 0.1f, 0.1f, 100.0f)) comp->RebuildMesh();
                    CheckUndoRedoDrag(actionManager, &comp->height_, std::function<void(const float&)>([comp](const float& v){ comp->height_ = v; comp->RebuildMesh(); }));
                    if (ImGui::SliderInt("Segments", &comp->subdivisions_, 3, 64)) comp->RebuildMesh();
                    CheckUndoRedoDrag(actionManager, &comp->subdivisions_, std::function<void(const int&)>([comp](const int& v){ comp->subdivisions_ = v; comp->RebuildMesh(); }));
                    break;
                case PrimitiveType::Torus:
                    if (ImGui::DragFloat("Major Radius", &comp->torusMajorRadius_, 0.1f, 0.1f, 100.0f)) comp->RebuildMesh();
                    CheckUndoRedoDrag(actionManager, &comp->torusMajorRadius_, std::function<void(const float&)>([comp](const float& v){ comp->torusMajorRadius_ = v; comp->RebuildMesh(); }));
                    if (ImGui::DragFloat("Minor Radius", &comp->torusMinorRadius_, 0.05f, 0.01f, 100.0f)) comp->RebuildMesh();
                    CheckUndoRedoDrag(actionManager, &comp->torusMinorRadius_, std::function<void(const float&)>([comp](const float& v){ comp->torusMinorRadius_ = v; comp->RebuildMesh(); }));
                    if (ImGui::SliderInt("Major Segments", &comp->torusMajorSegments_, 3, 64)) comp->RebuildMesh();
                    CheckUndoRedoDrag(actionManager, &comp->torusMajorSegments_, std::function<void(const int&)>([comp](const int& v){ comp->torusMajorSegments_ = v; comp->RebuildMesh(); }));
                    if (ImGui::SliderInt("Minor Segments", &comp->torusMinorSegments_, 3, 64)) comp->RebuildMesh();
                    CheckUndoRedoDrag(actionManager, &comp->torusMinorSegments_, std::function<void(const int&)>([comp](const int& v){ comp->torusMinorSegments_ = v; comp->RebuildMesh(); }));
                    break;
            }
            ImGui::TreePop();
        }
    }
};

class SpriteRendererComponentEditor : public IComponentEditor {
public:
    void Draw(Component* component, EditorActionManager* actionManager) override {
        auto* comp = static_cast<SpriteRendererComponent*>(component);
        if (ImGui::TreeNodeEx("SpriteRenderer", ImGuiTreeNodeFlags_DefaultOpen)) {
            bool needUpdate = false;
            TextureManager* tm = Sprite::GetTextureManager();
            if (tm) {
                auto names = tm->GetTextureNamesForDebug();
                int currentIndex = 0;
                for (int i = 0; i < (int)names.size(); ++i) {
                    if (names[i] == comp->texturePath_) {
                        currentIndex = i;
                        break;
                    }
                }
                const char* currentPreview = names.empty() ? "" : names[currentIndex].c_str();
                if (ImGui::BeginCombo("Texture", currentPreview)) {
                    for (int i = 0; i < names.size(); ++i) {
                        bool isSelected = (currentIndex == i);
                        if (ImGui::Selectable(names[i].c_str(), isSelected)) {
                            std::string oldTex = comp->texturePath_;
                            std::string newTex = names[i];
                            PushInstantUndo(actionManager, oldTex, newTex, std::function<void(const std::string&)>([comp](const std::string& v){ comp->SetTexture(v); }));
                        }
                        if (isSelected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            } else {
                char buffer[256];
                strncpy_s(buffer, sizeof(buffer), comp->texturePath_.c_str(), _TRUNCATE);
                static std::string startTex;
                if (ImGui::InputText("TexturePath", buffer, sizeof(buffer))) {
                    comp->SetTexture(buffer);
                }
                if (ImGui::IsItemActivated()) startTex = comp->texturePath_;
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    std::string endTex = buffer;
                    actionManager->PushAndExecute(std::make_unique<ChangeValueCommand<std::string>>(
                        startTex, endTex, [comp](const std::string& v){ comp->SetTexture(v); }));
                }
            }
            
            bool isTopMost = comp->isTopMost_;
            if (ImGui::Checkbox("TopMost (Draw over 3D)", &isTopMost)) {
                PushInstantUndo(actionManager, comp->isTopMost_, isTopMost, std::function<void(const bool&)>([comp](const bool& v){ comp->isTopMost_ = v; if (comp->GetSprite()) comp->GetSprite()->SetTopMost(v); }));
            }
            bool isFlipX = comp->isFlipX_;
            if (ImGui::Checkbox("Flip X", &isFlipX)) {
                PushInstantUndo(actionManager, comp->isFlipX_, isFlipX, std::function<void(const bool&)>([comp](const bool& v){ comp->isFlipX_ = v; if (comp->GetSprite()) comp->GetSprite()->SetFlip(comp->isFlipX_, comp->isFlipY_); }));
            }
            ImGui::SameLine();
            bool isFlipY = comp->isFlipY_;
            if (ImGui::Checkbox("Flip Y", &isFlipY)) {
                PushInstantUndo(actionManager, comp->isFlipY_, isFlipY, std::function<void(const bool&)>([comp](const bool& v){ comp->isFlipY_ = v; if (comp->GetSprite()) comp->GetSprite()->SetFlip(comp->isFlipX_, comp->isFlipY_); }));
            }

            if (ImGui::SliderFloat2("Anchor", comp->anchor_, 0.0f, 1.0f)) {
                if (comp->GetSprite()) comp->GetSprite()->SetAnchor(comp->anchor_[0], comp->anchor_[1]);
            }
            CheckUndoRedoDrag(actionManager, reinterpret_cast<Vector2*>(comp->anchor_), std::function<void(const Vector2&)>([comp](const Vector2& v){ 
                comp->anchor_[0] = v.x; comp->anchor_[1] = v.y; 
                if (comp->GetSprite()) comp->GetSprite()->SetAnchor(v.x, v.y); 
            }));
            
            ImGui::DragFloat2("Base Size", comp->size_, 1.0f, 1.0f, 8192.0f);
            CheckUndoRedoDrag(actionManager, reinterpret_cast<Vector2*>(comp->size_), std::function<void(const Vector2&)>([comp](const Vector2& v){
                comp->size_[0] = v.x; comp->size_[1] = v.y;
            }));
            
            if (ImGui::ColorEdit4("Color", &comp->color_.x)) {
                if (comp->GetSprite()) comp->GetSprite()->SetColor(comp->color_);
            }
            CheckUndoRedoDrag(actionManager, &comp->color_, std::function<void(const Vector4&)>([comp](const Vector4& v){
                comp->color_ = v;
                if (comp->GetSprite()) comp->GetSprite()->SetColor(v);
            }));
            ImGui::TreePop();
        }
    }
};

class TextRendererComponentEditor : public IComponentEditor {
public:
    void Draw(Component* component, EditorActionManager* actionManager) override {
        auto* comp = static_cast<TextRendererComponent*>(component);
        if (ImGui::TreeNodeEx("TextRenderer", ImGuiTreeNodeFlags_DefaultOpen)) {
            // Text (UTF-8 変換)
            std::string utf8Text = ConvertString(comp->GetText());
            char textBuffer[256];
            strncpy_s(textBuffer, sizeof(textBuffer), utf8Text.c_str(), _TRUNCATE);
            static std::string startText;
            if (ImGui::InputText("Text", textBuffer, sizeof(textBuffer))) {
                comp->SetText(ConvertString(std::string(textBuffer)));
            }
            if (ImGui::IsItemActivated()) startText = utf8Text;
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                std::string endText = textBuffer;
                actionManager->PushAndExecute(std::make_unique<ChangeValueCommand<std::string>>(
                    startText, endText, [comp](const std::string& v){ comp->SetText(ConvertString(v)); }));
            }

            // Font ID
            std::string fontId = comp->GetFontId();
            FontManager* fm = Text::GetFontManager();
            if (fm) {
                auto fontIds = fm->GetLoadedFontIds();
                int currentIndex = 0;
                for (int i = 0; i < (int)fontIds.size(); ++i) {
                    if (fontIds[i] == fontId) {
                        currentIndex = i;
                        break;
                    }
                }
                const char* currentPreview = fontIds.empty() ? "" : fontIds[currentIndex].c_str();
                if (ImGui::BeginCombo("Font ID", currentPreview)) {
                    for (int i = 0; i < fontIds.size(); ++i) {
                        bool isSelected = (currentIndex == i);
                        if (ImGui::Selectable(fontIds[i].c_str(), isSelected)) {
                            std::string oldFontId = comp->GetFontId();
                            std::string newFontId = fontIds[i];
                            PushInstantUndo(actionManager, oldFontId, newFontId, std::function<void(const std::string&)>([comp](const std::string& v){ comp->SetFontId(v); }));
                        }
                        if (isSelected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            } else {
                char fontBuffer[128];
                strncpy_s(fontBuffer, sizeof(fontBuffer), fontId.c_str(), _TRUNCATE);
                static std::string startFont;
                if (ImGui::InputText("Font ID", fontBuffer, sizeof(fontBuffer))) {
                    comp->SetFontId(fontBuffer);
                }
                if (ImGui::IsItemActivated()) startFont = fontId;
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    std::string endFont = fontBuffer;
                    actionManager->PushAndExecute(std::make_unique<ChangeValueCommand<std::string>>(
                        startFont, endFont, [comp](const std::string& v){ comp->SetFontId(v); }));
                }
            }
            
            // Alignment
            TextAlignment align = comp->GetAlignment();
            const char* alignments[] = { "Left", "Center", "Right" };
            if (ImGui::BeginCombo("Alignment", alignments[static_cast<int>(align)])) {
                for (int i = 0; i < 3; ++i) {
                    bool isSelected = (static_cast<int>(align) == i);
                    if (ImGui::Selectable(alignments[i], isSelected)) {
                        TextAlignment oldAlign = comp->GetAlignment();
                        TextAlignment newAlign = static_cast<TextAlignment>(i);
                        PushInstantUndo(actionManager, oldAlign, newAlign, std::function<void(const TextAlignment&)>([comp](const TextAlignment& v){ comp->SetAlignment(v); }));
                    }
                    if (isSelected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            // Base Scale
            float scale = comp->GetBaseScale();
            if (ImGui::DragFloat("Base Scale", &scale, 0.1f, 0.1f, 1000.0f)) {
                comp->SetBaseScale(scale);
            }
            CheckUndoRedoDrag(actionManager, &scale, std::function<void(const float&)>([comp](const float& v){ comp->SetBaseScale(v); }));

            // Color
            Vector4 color = comp->GetColor();
            if (ImGui::ColorEdit4("Color", &color.x)) {
                comp->SetColor(color);
            }
            CheckUndoRedoDrag(actionManager, &color, std::function<void(const Vector4&)>([comp](const Vector4& v){ comp->SetColor(v); }));

            // TopMost
            bool isTopMost = comp->IsTopMost();
            if (ImGui::Checkbox("TopMost", &isTopMost)) {
                PushInstantUndo(actionManager, comp->IsTopMost(), isTopMost, std::function<void(const bool&)>([comp](const bool& v){ comp->SetTopMost(v); }));
            }

            ImGui::TreePop();
        }
    }
};

class AABBColliderComponentEditor : public IComponentEditor {
public:
    void Draw(Component* component, EditorActionManager* actionManager) override {
        auto* comp = static_cast<AABBColliderComponent*>(component);
        ImGui::PushID(comp);
        if (ImGui::CollapsingHeader("AABB Collider", ImGuiTreeNodeFlags_DefaultOpen)) {
            DrawColliderCommonProperties(comp, actionManager);
        }
        ImGui::PopID();
    }
};

class OBBColliderComponentEditor : public IComponentEditor {
public:
    void Draw(Component* component, EditorActionManager* actionManager) override {
        auto* comp = static_cast<OBBColliderComponent*>(component);
        ImGui::PushID(comp);
        if (ImGui::CollapsingHeader("OBB Collider", ImGuiTreeNodeFlags_DefaultOpen)) {
            DrawColliderCommonProperties(comp, actionManager);
        }
        ImGui::PopID();
    }
};

class SphereColliderComponentEditor : public IComponentEditor {
public:
    void Draw(Component* component, EditorActionManager* actionManager) override {
        auto* comp = static_cast<SphereColliderComponent*>(component);
        ImGui::PushID(comp);
        if (ImGui::CollapsingHeader("Sphere Collider", ImGuiTreeNodeFlags_DefaultOpen)) {
            DrawColliderCommonProperties(comp, actionManager);
        }
        ImGui::PopID();
    }
};

class ParticleEmitterComponentEditor : public IComponentEditor {
public:
    void Draw(Component* component, EditorActionManager* actionManager) override {
        auto* compWrapper = static_cast<ParticleEmitterComponent*>(component);
        auto* comp = compWrapper->GetParticleObject();
        
        ImGui::PushID(compWrapper);
        comp->DebugUI("Particle Emitter");
        ImGui::PopID();
    }
};

class RaycastComponentEditor : public IComponentEditor {
public:
    void Draw(Component* component, EditorActionManager* actionManager) override {
        auto* comp = static_cast<RaycastComponent*>(component);
        ImGui::PushID(comp);
        if (ImGui::CollapsingHeader("Raycast", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragFloat3("Origin", &comp->localOffset_.x, 0.1f);
            CheckUndoRedoDrag(actionManager, &comp->localOffset_);
            ImGui::DragFloat3("Local Direction", &comp->localDirection_.x, 0.1f);
            CheckUndoRedoDrag(actionManager, &comp->localDirection_);
            ImGui::DragFloat("Max Distance", &comp->maxDistance_, 0.1f, 0.0f, 10000.0f);
            CheckUndoRedoDrag(actionManager, &comp->maxDistance_);
            
            // RaycastはLayerの描画なし（Maskのみ指定）
            if (ImGui::TreeNode("Collision Mask")) {
                if (ImGui::Button("All")) {
                    PushInstantUndo(actionManager, comp->mask_, 0xFFFFFFFF, &comp->mask_);
                }
                ImGui::SameLine();
                if (ImGui::Button("None")) {
                    PushInstantUndo(actionManager, comp->mask_, 0u, &comp->mask_);
                }

                const auto& layerNames = CollisionManager::GetInstance().GetLayerNames();
                for (int i = 0; i < layerNames.size(); ++i) {
                    bool isMasked = (comp->mask_ & (1u << i)) != 0;
                    if (ImGui::Checkbox(layerNames[i].c_str(), &isMasked)) {
                        uint32_t newMask = comp->mask_;
                        if (isMasked) newMask |= (1u << i);
                        else          newMask &= ~(1u << i);
                        PushInstantUndo(actionManager, comp->mask_, newMask, &comp->mask_);
                    }
                }
                ImGui::TreePop();
            }
            
            // デバッグ情報
            ImGui::Separator();
            if (comp->hitInfo_.isHit) {
                ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "Hit: %s", 
                    comp->hitInfo_.hitObject ? comp->hitInfo_.hitObject->GetName().c_str() : "Unknown");
                ImGui::Text("Distance: %.2f", comp->hitInfo_.distance);
                ImGui::Text("Point: (%.2f, %.2f, %.2f)", comp->hitInfo_.hitPoint.x, comp->hitInfo_.hitPoint.y, comp->hitInfo_.hitPoint.z);
            } else {
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "No Hit");
            }
        }
        ImGui::PopID();
    }
};

// =======================================================================
// ComponentEditorRegistry
// =======================================================================

ComponentEditorRegistry::ComponentEditorRegistry() {}
ComponentEditorRegistry::~ComponentEditorRegistry() {}

void ComponentEditorRegistry::RegisterAllEditors() {
    RegisterEditor<TransformComponent, TransformComponentEditor>();
    RegisterEditor<MeshRendererComponent, MeshRendererComponentEditor>();
    RegisterEditor<PrimitiveRendererComponent, PrimitiveRendererComponentEditor>();
    RegisterEditor<SpriteRendererComponent, SpriteRendererComponentEditor>();
    RegisterEditor<TextRendererComponent, TextRendererComponentEditor>();
    RegisterEditor<AABBColliderComponent, AABBColliderComponentEditor>();
    RegisterEditor<OBBColliderComponent, OBBColliderComponentEditor>();
    RegisterEditor<SphereColliderComponent, SphereColliderComponentEditor>();
    RegisterEditor<RaycastComponent, RaycastComponentEditor>();
    RegisterEditor<ParticleEmitterComponent, ParticleEmitterComponentEditor>();
}

void ComponentEditorRegistry::DrawComponent(Component* component, EditorActionManager* actionManager) {
    if (!component) return;
    auto it = editors_.find(typeid(*component));
    if (it != editors_.end()) {
        it->second->Draw(component, actionManager);
    } else {
        DrawFallbackPropertiesGUI(component, actionManager);
    }
}

#endif // EditorMode
