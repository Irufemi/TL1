#pragma once
#include <string>
#include <vector>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "Component/Component.h"

class BaseScene;

/**
 * @class GameObject
 * @brief コンポーネントをアタッチできるエンティティの基底クラス
 */
class GameObject : public std::enable_shared_from_this<GameObject> {
public:
    GameObject() = default;
    GameObject(const std::string& name) : name_(name) {}
    ~GameObject() = default;

    void Initialize();
    void Update(bool isPlayMode = true);
    void Draw();
    void DrawOutlineMask();

    nlohmann::json Serialize() const;
    void Deserialize(const nlohmann::json& j);

    /**
     * @brief 自分自身の完全なコピー(クローン)を生成する
     */
    std::shared_ptr<GameObject> Clone();

    /**
     * @brief 新しいコンポーネントを追加する
     * @return 追加されたコンポーネントの共有ポインタ
     */
    template<typename T, typename... Args>
    std::shared_ptr<T> AddComponent(Args&&... args) {
        auto component = std::make_shared<T>(std::forward<Args>(args)...);
        component->SetGameObject(this);
        
        components_.push_back(component);
        componentMap_[typeid(T)].push_back(component.get());
        
        component->OnRegisterProperties();
        component->Initialize();
        return component;
    }

    /**
     * @brief 既存のコンポーネントをアタッチする (Undo用)
     */
    void AddComponent(std::shared_ptr<Component> component);

    /**
     * @brief 指定した型のコンポーネントを取得する
     */
    template<typename T>
    T* GetComponent() {
        auto it = componentMap_.find(typeid(T));
        if (it != componentMap_.end() && !it->second.empty()) {
            return static_cast<T*>(it->second.front());
        }
        return nullptr;
    }

    /**
     * @brief コンポーネントを削除する
     */
    void RemoveComponent(Component* component);

    /**
     * @brief アタッチされているすべてのコンポーネントのリストを取得する
     */
    const std::vector<std::shared_ptr<Component>>& GetComponents() const { return components_; }

    // --- アクセッサ ---
    const std::string& GetName() const { return name_; }
    void SetName(const std::string& name) { name_ = name; }
    void SetIsActive(bool isActive) { isActive_ = isActive; }
    bool GetIsActive() const { return isActive_; }

    void SetDontSave(bool dontSave) { dontSave_ = dontSave; }
    bool GetDontSave() const { return dontSave_; }

    void SetScene(BaseScene* scene);
    BaseScene* GetScene() const { return scene_; }

    // --- 親子関係 ---
    void AddChild(std::shared_ptr<GameObject> child);
    void InsertChild(std::shared_ptr<GameObject> child, size_t index);
    void RemoveChild(std::shared_ptr<GameObject> child);
    std::shared_ptr<GameObject> GetParent() const { return parent_.lock(); }
    const std::vector<std::shared_ptr<GameObject>>& GetChildren() const { return children_; }
    void SetParent(std::shared_ptr<GameObject> parent);
    size_t GetChildIndex(std::shared_ptr<GameObject> child) const;

    // --- ライフサイクル ---
    /**
     * @brief オブジェクトを破棄状態にする（現在のフレームの終わりに削除される）
     */
    void Destroy() { isDestroyed_ = true; }
    bool IsDestroyed() const { return isDestroyed_; }

    // --- イベント伝達 ---
    void SendCollisionEnter(GameObject* hitObject);
    void SendCollisionStay(GameObject* hitObject);
    void SendCollisionExit(GameObject* hitObject);

    // --- 動的生成 ---
    /**
     * @brief 所属するシーンにプレハブから新しい GameObject を生成して追加する
     */
    std::shared_ptr<GameObject> Instantiate(const std::string& prefabPath, const Vector3& position = {0,0,0});

private:
    std::string name_ = "GameObject";
    bool isActive_ = true;
    bool isDestroyed_ = false;
    bool dontSave_ = false;
    BaseScene* scene_ = nullptr;
    
    std::weak_ptr<GameObject> parent_;
    std::vector<std::shared_ptr<GameObject>> children_;

    std::vector<std::shared_ptr<Component>> components_;
    std::unordered_map<std::type_index, std::vector<Component*>> componentMap_;
};
