#pragma once
#include <d3d12.h>
#include <string>
#include "Engine/Graphics/Camera/Camera.h"
#include "../../../Engine/Graphics/Data/TransformationMatrix.h"
#include <wrl.h>
#include <cstdint>
#include <memory>
#include <vector>
#include "../../../Engine/Core/Math/Transform.h"
#include "../../../Engine/Core/Math/Vector4.h"
#include "../../../Engine/Core/Math/Matrix4x4.h"
#include "../../../Resource/Model/Data/ObjModel.h"
#include "../Object3DResource.h"
#include "../../../Engine/Graphics/Data/Material.h"
#include "../../../Engine/Graphics/DirectX/DynamicConstantBuffer.h"
#include "../BaseModel/BaseModel.h"
#include "../../../Resource/Model/Data/Skeleton.h"
#include "../../../Resource/Model/Data/SkinCluster.h"
#include "../../../Engine/Graphics/Compute/IComputeTask.h"

class TextureManager;
class DrawManager;
class DebugUI;
class ModelManager;

//==========================
// objが配布されているサイト
// https://quaternius.com/
// 使用する場合はライセンスがCCOのものを利用する
// https://creativecommons.org/publicdomain/zero/1.0/deed.ja
//==========================

/**
 * @class StaticModelObject
 * @brief 3Dモデル（OBJ/GLTF等）のインスタンスを描画・管理するクラス
 * @details ModelManager から取得した共有モデルデータを参照し、個別の位置・回転・拡縮やマテリアル設定を保持します。
 * スキン付きモデルの場合は、バインドポーズによる静的スキニングコンピュートタスクを実行します。
 */
class StaticModelObject : public BaseModel, public IComputeTask {



private:

    /**
     * @brief ロード完了後にメッシュ等のリソースを構築する（遅延初期化）
     */
    void InitializeResources();

public: //メンバ関数

    /**
     * @brief デストラクタ
     */
    ~StaticModelObject() override;

    /**
     * @brief 初期化
     * @param[in] filename モデルファイル名（ModelManager経由でロード）
     */
    void Initialize(const std::string& filename = "plane.obj");

    /**
     * @brief 更新処理
     * @details ワールド行列の計算と定数バッファへの転送を行います。
     */
    void Update();

    /**
     * @brief 描画コマンドの積み込み
     */
     void SyncBeforeDraw() override;
    void Draw() override;
    void DrawOutlineMask() override;

    /**
     * @brief デバッグ用UIの表示
     */
    void Debug(const char* objName = " ");

    /**
     * @brief デバッグ用タブの表示
     */
    void DebugTab();

    /**
     * @brief コンピュートシェーダーを用いたスキニング処理を実行します
     */
    void DispatchCompute() override;

private:
    Skeleton skeleton_;
    SkinCluster skinCluster_;
    uint32_t lastSkinnedFrameIndex_ = 0;

};

