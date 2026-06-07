#pragma once

#include "BaseRegion.h"
#include "Resource/Model/Data/ObjModel.h"

struct ManagedModel;
struct GpuMesh;
class ModelManager;

/**
 * @class ModelRegion
 * @brief 外部モデルデータ（.obj, .gltf）を描画するための領域クラス
 */
class ModelRegion : public BaseRegion {
public:
    ModelRegion() = default;
    ~ModelRegion() override = default;

    static void SetModelManager(ModelManager* mm) { modelManager_ = mm; }

    void Initialize(const std::string& objFilename);
    
    void Draw() override;
    void Draw(bool isUI);

    const GpuMesh* GetGpuMesh() const; // 共有メッシュ取得

protected:
    float GetBoundingSphereRadius() const override;

private:
    void InitializeResources();
    void CreateMaterialResources(const ObjMesh& mesh);
    void EnsureSharedTexture(const ObjMesh& mesh);

private:
    static ModelManager* modelManager_;

    // 共有モデルデータ(CPU/GPU)
    std::shared_ptr<ManagedModel> managedModel_{};
    bool isResourcesInitialized_ = false;
};
