#include "GameObject.h"
#include "BaseScene.h"

#include "Component/Component.h"
#include "Component/ComponentFactory.h"
#include "Component/TransformComponent.h"
#include "Component/Renderer/MeshRendererComponent.h"
#include "Component/Renderer/PrimitiveRendererComponent.h"
#include "Component/Renderer/SpriteRendererComponent.h"
#include "Component/Collider/AABBColliderComponent.h"
#include "Component/Collider/SphereColliderComponent.h"
#include "Component/Collider/OBBColliderComponent.h"
#include "Component/Collider/RaycastComponent.h"
#include "Component/Script/RotatorComponent.h"
void GameObject::Initialize() {
    for (auto& comp : components_) {
        comp->Initialize();
    }
    for (auto& child : children_) {
        child->Initialize();
    }
}

void GameObject::SetScene(BaseScene* scene) {
    scene_ = scene;
    for (auto& child : children_) {
        if (child) {
            child->SetScene(scene);
        }
    }
}

void GameObject::Update(bool isPlayMode) {
    if (!isActive_) return;
    for (auto& comp : components_) {
        // PlayModeでない場合は、エディタで更新可能なコンポーネントのみ更新する
        if (!isPlayMode && !comp->CanUpdateInEditMode()) {
            continue;
        }
        comp->Update();
    }
    for (auto& child : children_) {
        child->Update(isPlayMode);
    }
}

void GameObject::Draw() {
    if (!isActive_) return;
    for (auto& comp : components_) {
        comp->Draw();
    }
    for (auto& child : children_) {
        child->Draw();
    }
}

void GameObject::DrawOutlineMask() {
    if (!isActive_) return;
    for (auto& comp : components_) {
        comp->DrawOutlineMask();
    }
    for (auto& child : children_) {
        child->DrawOutlineMask();
    }
}

void GameObject::AddChild(std::shared_ptr<GameObject> child) {
    if (!child) return;
    
    // 既に親がいる場合は外す
    if (auto currentParent = child->GetParent()) {
        currentParent->RemoveChild(child);
    }
    
    child->parent_ = shared_from_this();
    children_.push_back(child);
}

void GameObject::InsertChild(std::shared_ptr<GameObject> child, size_t index) {
    if (!child) return;

    if (auto currentParent = child->GetParent()) {
        currentParent->RemoveChild(child);
    }

    child->parent_ = shared_from_this();
    if (index >= children_.size()) {
        children_.push_back(child);
    } else {
        children_.insert(children_.begin() + index, child);
    }
}

void GameObject::RemoveChild(std::shared_ptr<GameObject> child) {
    auto it = std::find(children_.begin(), children_.end(), child);
    if (it != children_.end()) {
        (*it)->parent_.reset();
        children_.erase(it);
    }
}

size_t GameObject::GetChildIndex(std::shared_ptr<GameObject> child) const {
    auto it = std::find(children_.begin(), children_.end(), child);
    if (it != children_.end()) {
        return std::distance(children_.begin(), it);
    }
    return (size_t)-1;
}

void GameObject::SetParent(std::shared_ptr<GameObject> parent) {
    if (parent) {
        parent->AddChild(shared_from_this());
    } else {
        if (auto currentParent = parent_.lock()) {
            currentParent->RemoveChild(shared_from_this());
        }
    }
}

void GameObject::AddComponent(std::shared_ptr<Component> component) {
    if (!component) return;
    component->SetGameObject(this);
    components_.push_back(component);
    componentMap_[typeid(*component)].push_back(component.get());
    component->OnRegisterProperties();
    component->Initialize();
}

void GameObject::RemoveComponent(Component* component) {
    if (!component) return;

    // TransformComponentは基本として削除不可とする
    if (component->GetComponentName() == "TransformComponent") return;

    // componentMap_からの削除
    auto typeIt = componentMap_.find(typeid(*component));
    if (typeIt != componentMap_.end()) {
        auto& vec = typeIt->second;
        vec.erase(std::remove(vec.begin(), vec.end(), component), vec.end());
    }

    // components_からの削除
    components_.erase(std::remove_if(components_.begin(), components_.end(),
        [component](const std::shared_ptr<Component>& ptr) {
            return ptr.get() == component;
        }), components_.end());
}



nlohmann::json GameObject::Serialize() const {
    nlohmann::json j;
    j["name"] = name_;
    j["isActive"] = isActive_;
    
    nlohmann::json comps = nlohmann::json::array();
    for (const auto& comp : components_) {
        nlohmann::json cj;
        cj["type"] = comp->GetComponentName();
        cj["data"] = comp->Serialize();
        comps.push_back(cj);
    }
    j["components"] = comps;
    
    nlohmann::json childrenJson = nlohmann::json::array();
    for (const auto& child : children_) {
        childrenJson.push_back(child->Serialize());
    }
    j["children"] = childrenJson;
    
    return j;
}

void GameObject::Deserialize(const nlohmann::json& j) {
    if (j.contains("name")) name_ = j["name"];
    if (j.contains("isActive")) isActive_ = j["isActive"];
    
    if (j.contains("components")) {
        for (const auto& cj : j["components"]) {
            std::string type = cj["type"];
            std::shared_ptr<Component> newComp = ComponentFactory::Create(type);
            
            if (newComp) {
                // AddComponentと同等の登録処理をInitializeの前に行う
                newComp->SetGameObject(this);
                components_.push_back(newComp);
                componentMap_[typeid(*newComp)].push_back(newComp.get());
                newComp->OnRegisterProperties();
                
                // Initialize前にパラメータを復元する
                if (cj.contains("data")) {
                    newComp->Deserialize(cj["data"]);
                }
                
                // 復元されたデータを使って初期化
                newComp->Initialize();
            }
        }
    }
    
    if (j.contains("children") && j["children"].is_array()) {
        for (const auto& cj : j["children"]) {
            auto child = std::make_shared<GameObject>();
            child->Deserialize(cj);
            AddChild(child);
        }
    }
}

std::shared_ptr<GameObject> GameObject::Clone() {
    auto clone = std::make_shared<GameObject>();
    clone->Deserialize(this->Serialize());
    // クローン時は必要に応じて "(Clone)" などを付与
    clone->SetName(this->GetName() + " (Clone)");
    return clone;
}

void GameObject::SendCollisionEnter(GameObject* hitObject) {
    for (auto& comp : components_) {
        comp->OnCollisionEnter(hitObject);
    }
}

void GameObject::SendCollisionStay(GameObject* hitObject) {
    for (auto& comp : components_) {
        comp->OnCollisionStay(hitObject);
    }
}

void GameObject::SendCollisionExit(GameObject* hitObject) {
    for (auto& comp : components_) {
        comp->OnCollisionExit(hitObject);
    }
}

std::shared_ptr<GameObject> GameObject::Instantiate(const std::string& prefabPath, const Vector3& position) {
    if (scene_) {
        return scene_->InstantiatePrefab(prefabPath, position);
    }
    return nullptr;
}

