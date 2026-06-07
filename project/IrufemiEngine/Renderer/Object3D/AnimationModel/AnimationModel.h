#pragma once
#include "../../Core/IRenderable.h"

#include "../BaseModel/BaseModel.h"
#include "Resource/Model/Data/Animation.h"
#include "Resource/Model/Data/NodeAnimation.h"
#include "Resource/Model/Data/Skeleton.h"
#include "Resource/Model/Data/SkinCluster.h"

#include "../../../Engine/Graphics/Compute/IComputeTask.h"
#include <d3d12.h>
#include <string>
#include <cstdint>
#include <memory>
#include <vector>
#include <map>
#include <wrl.h>

// 前方宣言
class Camera;
class IrufemiEngine;
class PrimitiveRegion;
class Line3DRegion;
struct ManagedModel;
struct ObjMaterial; 
struct Material;


class AnimationModel : public IComputeTask, public BaseModel {
public: // メンバ関数

    AnimationModel();
    ~AnimationModel() override;

    void DispatchCompute() override;

    void Initialize(const std::string& filename);

    void Update();

    void SyncBeforeDraw() override;
    void Draw() override;

    void Debug(const char* objName = " ");



    void UpdateAnimation();

    void InitializeResources();



private: // メンバ変数
private: // メンバ変数

    Skeleton skeleton_;

    SkinCluster skinCluster_;

    // ノードアニメーション用の固有Matrix
    Matrix4x4 localMatrix_;

    Matrix4x4 worldMatrix_;

    Animation animation_;

    float animationTime_ = 0.0f;

    // --- 追加：関節表示用のインスタンス描画機構 ---
    std::unique_ptr<PrimitiveRegion> jointSpheres_;
    std::unique_ptr<Line3DRegion> boneLines_;

    uint32_t lastUpdateFrame_ = static_cast<uint32_t>(-1);
    uint32_t lastSkinnedFrameIndex_ = 0; // スキニングが最後に実行されたフレームインデックス（ポーズ中の不整合対策）

    std::string filename_;
};