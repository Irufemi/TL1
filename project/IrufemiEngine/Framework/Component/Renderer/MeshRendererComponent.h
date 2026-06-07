#pragma once
#include "../Component.h"
#include <memory>
#include <string>
#include "Engine/Core/Shape/Sphere.h"

// 前方宣言
class StaticModelObject;
class TransformComponent;

class MeshRendererComponent : public Component {
public:
    MeshRendererComponent();
    ~MeshRendererComponent() override;

    // 初期化時にモデルファイル名を指定
    void LoadModel(const std::string& filename);

    void Initialize() override;
    void Update() override;
    void Draw() override;
    
    bool CanUpdateInEditMode() const override { return true; }

    IRenderable* GetRenderable() override { return reinterpret_cast<IRenderable*>(obj_.get()); }
    
    // エディタのRaycast用
    Sphere GetWorldSphere() const;
    bool Raycast(const Ray& ray, float& outDistance) const override;

    std::string GetComponentName() const override { return "MeshRendererComponent"; }
    nlohmann::json Serialize() override;
    void Deserialize(const nlohmann::json& j) override;

#ifdef EditorMode
    friend class MeshRendererComponentEditor;
#endif

private:
    std::unique_ptr<StaticModelObject> obj_;                 ///< 実際の描画を担う既存クラス
    TransformComponent* transform_ = nullptr;       ///< 親のTransform情報（キャッシュ）
    std::string modelName_ = "plane.obj";           ///< 読み込むモデル名
};
