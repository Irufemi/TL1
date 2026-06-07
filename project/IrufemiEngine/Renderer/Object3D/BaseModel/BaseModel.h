#pragma once
#include "../../Core/IRenderable.h"
#include "../../Core/MultiBufferSyncState.h"

#include "Engine/Core/Math/Matrix4x4.h"
#include "Engine/Core/Math/Transform.h"
#include "Engine/Core/Math/Vector3.h"
#include "Engine/Core/Math/Vector4.h"
#include "../../../Engine/Graphics/Data/TransformationMatrix.h"
#include "Resource/Model/Data/ObjModel.h"
#include "Renderer/Object3D/Object3DResource.h"
#include "Engine/Graphics/Data/Material.h"
#include <d3d12.h>
#include <string>
#include <cstdint>
#include <memory>
#include <vector>

// 前方宣言
class Camera;
class IrufemiEngine;
struct ManagedModel;
struct ObjMaterial;

/**
 * @class BaseModel
 * @brief 3Dモデル（静的メッシュおよびアニメーションメッシュ）の共通基底クラス
 * @details Transform管理、マテリアルオーバーライド、フラストゥムカリング、
 * および基本リソース管理を提供します。
 */
class BaseModel : public IRenderable, public MultiBufferSyncState {
public: // メンバ関数

    virtual ~BaseModel();

    // 描画用の変換行列リソースのGPUアドレスを取得
    D3D12_GPU_VIRTUAL_ADDRESS GetTransformationGpuAddress() const;

protected: // 内部ヘルパ

    /**
     * @brief CPU側のマテリアルデータをGPUリソースへ転送する（内部用）
     */
    void UpdateMaterials();

public: // ゲッター・セッター

    /**
     * @brief モデルが持つメッシュ数を取得
     */
    size_t GetMeshCount() const;

    // 指定したインデックスのメッシュのマテリアルを取得(読み取り専用)
    const ObjMaterial* GetMaterial(size_t meshIndex) const;
    // 指定したインデックスのメッシュのマテリアルを取得(書き込み可能)
    ObjMaterial* GetMaterial(size_t meshIndex);

    void SetColor(const Vector4& color) { color_ = color; MarkAsDirty(); }
    const Vector4& GetColor() const { return color_; }

    void SetAlpha(float alpha) { color_.w = alpha; MarkAsDirty(); }

    void SetPosition(const Vector3& position) { transform_.translate = position; MarkAsDirty(); }
    const Vector3& GetPosition() const { return transform_.translate; }

    std::shared_ptr<ObjModel> GetCpuModel() const;

    void SetTranslate(const Vector3& translate) { transform_.translate = translate; MarkAsDirty(); }

    void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; MarkAsDirty(); }
    const Vector3& GetRotate() const { return transform_.rotate; }

    void SetRotateX(const float& rotate) { transform_.rotate.x = rotate; MarkAsDirty(); }
    void SetRotateY(const float& rotate) { transform_.rotate.y = rotate; MarkAsDirty(); }
    void SetRotateZ(const float& rotate) { transform_.rotate.z = rotate; MarkAsDirty(); }

    void SetScale(const Vector3& scale) { transform_.scale = scale; MarkAsDirty(); }
    const Vector3& GetScale() const { return transform_.scale; }

    void SetTransform(const Transform& transform) { transform_ = transform; MarkAsDirty(); }
    const Transform& GetTransform() const { return transform_; }

    void SetEnvironmentCoefficient(float coefficient) { environmentCoefficient_ = coefficient; isDirty_ = true; MarkAsDirty(); }
    float GetEnvironmentCoefficient() const { return environmentCoefficient_; }

    void SetLightingModeOverride(int32_t mode) { lightingModeOverride_ = mode; isDirty_ = true; MarkAsDirty(); }
    void SetUseClampSamplerOverride(int32_t useClamp) { useClampSamplerOverride_ = useClamp; isDirty_ = true; MarkAsDirty(); }
    void SetEnableLightingOverride(int32_t enable) { enableLightingOverride_ = enable; isDirty_ = true; MarkAsDirty(); }
    
    void SetEnableLightingToAllMeshes(bool enable) { enableLightingOverride_ = enable ? 1 : 0; MarkAsDirty(); }

    void MarkAsDirty() override {
        MultiBufferSyncState::MarkAsDirty();
        for(auto& res : meshResources_) {
            if (res) res->MarkAsDirty();
        }
    }

    static void SetIrufemiEngine(IrufemiEngine* engine) { engine_ = engine; }
    static IrufemiEngine* GetIrufemiEngine() { return engine_; }

    void SetCullingEnabled(bool enabled) { isCullingEnabled_ = enabled; }
    bool IsCullingEnabled() const { return isCullingEnabled_; }
    void SetCastShadows(bool cast) { castShadows_ = cast; }
    bool GetCastShadows() const { return castShadows_; }

    const TransformationMatrix& GetTransformationMatrix() const { return transformationMatrix_; }
    void SetTransformationMatrix(const TransformationMatrix& transformationMatrix) { transformationMatrix_ = transformationMatrix; }

    void SetCustomPSO(ID3D12PipelineState* pso) {
        for(auto& res : meshResources_) { if(res) res->SetCustomPSO(pso); }
    }
    void SetCustomCBVAddress(D3D12_GPU_VIRTUAL_ADDRESS addr) {
        for(auto& res : meshResources_) { if(res) res->SetCustomCBVAddress(addr); }
    }

protected: // メンバ変数
    // 共有モデルデータ(CPU/GPU)
    std::shared_ptr<ManagedModel> managedModel_;
    std::shared_ptr<ManagedModel> nextManagedModel_; // 次フレームで切り替えるためのモデル
    bool isModelChanged_ = false; // モデル切り替えフラグ

    // オブジェクト全体のTransform
    Transform transform_{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
    TransformationMatrix transformationMatrix_{};
    Vector4 color_ = { 1.0f, 1.0f, 1.0f, 1.0f }; // インスタンスカラー
    float environmentCoefficient_ = 1.0f; // インスタンス環境マップ係数
    int32_t lightingModeOverride_ = -1; // -1:使用しない, 0以上:上書き
    int32_t useClampSamplerOverride_ = -1; // -1:使用しない, 0以上:上書き
    int32_t enableLightingOverride_ = -1; // -1:使用しない, 0以上:上書き

    // --- 描画リソース ---
    std::vector<std::unique_ptr<Object3DResource>> meshResources_;

    // 変換行列用リソース (全メッシュ共有)
    uint32_t transformCbIndex_ = static_cast<uint32_t>(-1);

    static IrufemiEngine* engine_;

    // 行列更新の最適化用
    bool isDirty_ = true;

    bool isCullingEnabled_ = true;
    bool castShadows_ = true;
    Matrix4x4 lastViewMatrix_ = {};
    Matrix4x4 lastProjectionMatrix_ = {};
};
