#include "StaticModelObject.h" // リネーム済み
#include <filesystem>
#include <algorithm>
#include <Windows.h>
#include "Engine/Core/Math/Math.h"
#include "Resource/Texture/TextureManager.h"
#include "Engine/Manager/DrawManager.h"
#include "Engine/Manager/DebugUI.h"
#include "Resource/Model/ModelManager.h"
#include "Engine/Graphics/DirectX/DirectXCommon.h"
#include "Engine/IrufemiEngine.h"
#include "Engine/Graphics/Camera/CameraManager.h"
#include "Resource/Model/AnimationManager.h"
#include "Engine/Graphics/Compute/IComputeTask.h"

// 静的メンバ定義


StaticModelObject::~StaticModelObject() {}

void StaticModelObject::Initialize(const std::string& filename) {

    assert(engine_ && "StaticModelObject::Initialize: Engine is not set.");
    // 描画中のリソース破棄（Use-After-Free）を防ぐため、次フレームのUpdateで切り替えるフラグと変数を設定
    nextManagedModel_ = engine_->GetObjModelManager()->GetModelAsync(filename);
    isModelChanged_ = true;
}

void StaticModelObject::InitializeResources() {
    if (!managedModel_ || !managedModel_->cpuModel) {
        return;
    }

    if (transformCbIndex_ == static_cast<uint32_t>(-1)) {
        if (engine_) {
            transformCbIndex_ = engine_->GetTransformBufferManager()->Allocate();
        }
    }

    // インスタンス固有の各メッシュ用リソースを生成
    meshResources_.clear();
    for (size_t i = 0; i < managedModel_->gpuMeshes.size(); ++i) {
        auto res = std::make_unique<Object3DResource>();
        
        // 外部の変換行列リソースを借用
        res->SetExternalTransformCbIndex(&transformCbIndex_);
        
        // メッシュ固有の View を設定
        const auto& gpuMesh = managedModel_->gpuMeshes[i];
        res->vertexBufferView_ = gpuMesh->vertexBufferView;
        res->indexBufferView_ = gpuMesh->indexBufferView;
        res->indexCount_ = gpuMesh->indexCount;
        
        // マテリアルリソース等の生成
        res->CreateResource();
        
        // 初期テクスチャハンドルを共有データからコピー
        const auto& gpuMaterial = (i < managedModel_->gpuMaterials.size()) ? managedModel_->gpuMaterials[i] : nullptr;
        if (gpuMaterial) {
            res->textureHandle_ = gpuMaterial->textureHandle;
        }

        meshResources_.push_back(std::move(res));
    }

    // Skeleton と SkinCluster の初期化 (スキンがある場合)
    if (managedModel_ && managedModel_->cpuModel && !managedModel_->cpuModel->skinClusterData.empty()) {
        skeleton_ = AnimationManager::CreateSkeleton(managedModel_->cpuModel->rootNode);
        skinCluster_ = engine_->GetAnimationManager()->CreateSkinCluster(skeleton_, *managedModel_->cpuModel);
        
        // 静的モデルなのでバインドポーズのままスケルトンを1度更新しておく
        AnimationManager::SkeletonUpdate(skeleton_);
    }

    // 初回Updateを呼んでおく
    Update();
}

void StaticModelObject::Update() {
    // 描画キューにポインタが積まれた後にリソースが破棄されないよう、Updateのタイミングでモデルを切り替える
    if (isModelChanged_) {
        if (nextManagedModel_) {
            auto status = nextManagedModel_->status.load();
            if (status == ManagedModel::LoadingStatus::Loaded && nextManagedModel_->cpuModel) {
                managedModel_ = nextManagedModel_;
                isModelChanged_ = false;
                nextManagedModel_ = nullptr;
                InitializeResources();
            } else if (status == ManagedModel::LoadingStatus::Failed) {
                isModelChanged_ = false;
                nextManagedModel_ = nullptr;
            }
        } else {
            isModelChanged_ = false;
        }
    }

    if (!managedModel_ || !engine_) return;
    Camera* activeCam = engine_->GetCameraManager()->GetActiveCamera();
    if (!activeCam) return;

    // 非同期ロードが終わっていればメッシュを構築する (遅延初期化)
    if (managedModel_->status.load() == ManagedModel::LoadingStatus::Loaded && meshResources_.empty()) {
        InitializeResources();
    }

    // まだリソースが準備できていない場合はスキップ
    if (meshResources_.empty()) return;

    // オブジェクト全体のワールド行列を計算
    transformationMatrix_.world = Math::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);

    // rootNodeの行列を適用(モデルデータに階層情報があれば)
    if (managedModel_->cpuModel) {
        if (!managedModel_->cpuModel->skinClusterData.empty()) {
            // スキニングモデルの場合、rootNodeの行列はSkeleton内で処理されるため
            // World行列には適用しない（二重適用を防ぐ）
        } else {
            transformationMatrix_.world = managedModel_->cpuModel->rootNode.localMatrix * transformationMatrix_.world;
        }
    }

    // 法線変換用の逆転置行列
    Matrix4x4 worldForNormal = transformationMatrix_.world;
    worldForNormal.m[3][0] = 0.0f; worldForNormal.m[3][1] = 0.0f;
    worldForNormal.m[3][2] = 0.0f; worldForNormal.m[3][3] = 1.0f;
    transformationMatrix_.WorldInverseTranspose = Math::Transpose(Math::Inverse(worldForNormal));

    // マテリアル情報をGPUへ転送
    UpdateMaterials();

    isDirty_ = false;
    lastViewMatrix_ = activeCam->GetViewMatrix();
    lastProjectionMatrix_ = activeCam->GetPerspectiveFovMatrix();

    // スキニングモデルの場合は Compute Shader の実行を予約する
    if (managedModel_->cpuModel && !managedModel_->cpuModel->skinClusterData.empty() && engine_ && engine_->GetDrawManager()) {
        engine_->GetDrawManager()->RegisterComputeTask(this);
    }
}

void StaticModelObject::SyncBeforeDraw() {
    uint32_t frameIndex = engine_->GetDrawManager()->GetDxCommon()->GetFrameIndex();
    
    if (CheckAndClearDirty(frameIndex)) {
        // 変換行列の更新 (全メッシュで共有のバッファ)
        if (engine_) {
            if (transformCbIndex_ != static_cast<uint32_t>(-1)) {
                engine_->GetTransformBufferManager()->Update(transformCbIndex_, transformationMatrix_, frameIndex);
            }
        }

    }
    
    // 各メッシュのマテリアル等の更新
    for (auto& res : meshResources_) {
        res->SyncBeforeDraw();
    }

    // --- SkinCluster のマルチバッファ同期 ---
    if (managedModel_ && managedModel_->cpuModel && !managedModel_->cpuModel->skinClusterData.empty()) {
        AnimationManager::SkinClusterUpdate(skinCluster_, skeleton_, frameIndex);
    }
}

#include "../../../Engine/Core/Math/Geometry/Collision.h"
#include "../../../Engine/Core/Shape/Sphere.h"

void StaticModelObject::Draw() {
    if (!managedModel_ || !engine_ || !engine_->GetDrawManager()) {
        return;
    }
    Camera* activeCam = engine_->GetCameraManager()->GetActiveCamera();
    if (!activeCam) return;

    // カメラの行列が変更されたか、オブジェクト自体が変更されたかチェック
    bool cameraChanged = (std::memcmp(&lastViewMatrix_, &activeCam->GetViewMatrix(), sizeof(Matrix4x4)) != 0 ||
                          std::memcmp(&lastProjectionMatrix_, &activeCam->GetPerspectiveFovMatrix(), sizeof(Matrix4x4)) != 0);

    if (isDirtyBuffer_[BaseResource::GetDirectXCommon()->GetFrameIndex()] || cameraChanged) {
        Update();
    }
    
    // --- 【追加】描画直前のバッファ同期 ---
    SyncBeforeDraw();

    // 視錐台カリング
    if (isCullingEnabled_ && managedModel_->cpuModel) {
        const Sphere& modelSphere = managedModel_->cpuModel->boundingSphere;

        // ワールド空間の境界球を計算
        Sphere worldSphere;
        worldSphere.center = Math::Transform(modelSphere.center, transformationMatrix_.world);

        // スケールの最大値を適用して半径を変換
        float maxScale = (std::max)({ transform_.scale.x, transform_.scale.y, transform_.scale.z });
        worldSphere.radius = modelSphere.radius * maxScale * 1.1f; // 10% マージン

        // 判定
        if (!Collision::IsCollision(activeCam->GetFrustum(), worldSphere)) {
            return; // 描画スキップ
        }
    }

    // モデル内の全メッシュを描画
    for (auto& res : meshResources_) {
        if (managedModel_->cpuModel && !managedModel_->cpuModel->skinClusterData.empty()) {
            engine_->GetDrawManager()->SubmitStandard3D(res.get(), &skinCluster_.skinnedVertexBufferView[lastSkinnedFrameIndex_], castShadows_);
        } else {
            engine_->GetDrawManager()->SubmitStandard3D(res.get(), nullptr, castShadows_);
        }
    }
}

void StaticModelObject::DrawOutlineMask() {
    if (!managedModel_ || !engine_ || !engine_->GetDrawManager() || meshResources_.empty()) return;
    for (auto& res : meshResources_) {
        engine_->GetDrawManager()->SubmitOutlineMask(res.get(), nullptr);
    }
}
void StaticModelObject::DispatchCompute() {
    if (!managedModel_ || !managedModel_->cpuModel || managedModel_->cpuModel->skinClusterData.empty() || !engine_) return;
    Camera* activeCam = engine_->GetCameraManager()->GetActiveCamera();
    if (!activeCam) return;

    if (isCullingEnabled_) {
        float maxScale = (std::max)({ transform_.scale.x, transform_.scale.y, transform_.scale.z });
        Sphere boundingSphere;
        boundingSphere.center = transform_.translate;
        boundingSphere.radius = managedModel_->cpuModel->boundingSphere.radius * maxScale * 1.5f;
        if (!Collision::IsCollision(activeCam->GetFrustum(), boundingSphere)) {
            return; // 視錐台カリングされている場合はComputeもスキップ
        }
    }

    engine_->GetDrawManager()->DispatchSkinning(skinCluster_, managedModel_.get(), skinCluster_.mappedSkinningInformation->numVertices);
    lastSkinnedFrameIndex_ = engine_->GetDrawManager()->GetDxCommon()->GetFrameIndex();
}

void StaticModelObject::Debug([[maybe_unused]] const char* objName) {
#if defined USE_IMGUI
    std::string name = std::string("Obj: ") + objName;
    ImGui::Begin(name.c_str());
    DebugTab();
    ImGui::End();
#endif
}

void StaticModelObject::DebugTab() {
#if defined USE_IMGUI
    if (engine_) {
        auto ui_ = engine_->GetDebugUI();
        ImGui::Checkbox("Frustum Culling", &isCullingEnabled_);
        ui_->DebugTransform(transform_);
        ImGui::ColorEdit4("Color", &color_.x); // インスタンスカラーを編集
        ui_->DebugMaterialOverrides(&environmentCoefficient_, &lightingModeOverride_, &useClampSamplerOverride_, &enableLightingOverride_, "##OcOverrides");

        // ImGuiでマテリアルを編集
        if (managedModel_ && managedModel_->cpuModel) {
            for (size_t i = 0; i < managedModel_->cpuModel->meshes.size(); ++i) {
                std::string materialLabel = "Mesh " + std::to_string(i) + " Material";
                if (ImGui::TreeNode(materialLabel.c_str())) {
                    ObjMaterial* mat = GetMaterial(i);
                    if (mat) {
                        // unique_id を渡してコントロールIDの衝突を避ける
                        std::string unique_id = "##" + std::to_string(i);
                        ui_->DebugObjMaterial(mat, unique_id.c_str());

                        // テクスチャ選択
                        // 注意：この部分はStaticModelObjectがテクスチャのインデックスを保持する仕組みがないと完全には機能しません。
                        // 今はUIのみ表示します。
                        int tempIndex = 0; // ダミー
                        // ui_->DebugTexture(...)
                    }
                    ImGui::TreePop();
                }
            }
        }
    }
#endif
}


