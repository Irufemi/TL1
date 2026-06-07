#pragma once

#include "../../Core/IRenderable.h"
#include <memory>
#include <string>
#include <vector>
#include <functional>

#include "Engine/Core/Type/PrimitiveType.h"
#include "Renderer/Data/RenderData.h"

// 前方宣言
class Camera;
class TextureManager;
class DrawManager;
class DebugUI;
struct PrimitiveData;

/**
 * @class Primitive3DObject
 * @brief 汎用的な3Dプリミティブ（立方体、球、平面など）を管理・描画するクラス
 * @details コンポーネント指向に基づき、メッシュ・マテリアル・トランスフォームの各機能を内部に持ちます。
 *          ImGuiエディタからのリアルタイムな形状変更やプロパティ編集に対応します。
 */
class Primitive3DObject : public IRenderable {
public:
    Primitive3DObject() = default;
    ~Primitive3DObject() = default;

    /**
     * @brief 初期化処理
     * @param[in] camera 使用するカメラのポインタ
     * @param[in] type 初期形状タイプ
     * @param[in] texturePath 使用するテクスチャのパス
     */
    void Initialize(PrimitiveType type, const std::string& texturePath = "resources/uvChecker.png");

    /**
     * @brief 更新処理
     */
    void Update();

    /**
     * @brief 描画処理
     */
    void SyncBeforeDraw() override;
    void Draw() override;
    void Draw(bool isUI);
    void DrawOutlineMask() override;

    /**
     * @brief 描画処理（カメラを外部から指定する場合）
     * @param[in] camera 描画に使用するカメラ
     */
    void Draw(const Camera& camera);
    void Draw(const Camera& camera, bool isUI);

    /**
     * @brief ImGuiによるデバッグ・編集用UIを表示する
     * @param[in] label UIウィンドウおよび識別用のラベル
     */
    void Debug(const char* label = "Primitive Object");

    // --- 各コンポーネントへのアクセサ ---
    PrimitiveTransform& GetTransform() { return transform_; }
    const PrimitiveTransform& GetTransform() const { return transform_; }
    MeshDesc& GetMesh() { return mesh_; }
    MaterialDesc& GetMaterial() { return material_; }
    bool IsCullingEnabled() const { return isCullingEnabled_; }

    // --- 補助メソッド ---
    Vector3 GetCenter() const { return transform_.transform.translate; }
    Vector3 GetRight() const;
    Vector3 GetUp() const;
    Vector3 GetDirection() const;

    // --- ヘルパーSetter ---
    void SetTransform(const Transform& t) { transform_.transform = t; transform_.isDirty = true; }
    void SetPosition(const Vector3& pos) { transform_.transform.translate = pos; transform_.isDirty = true; }
    void SetRotate(const Vector3& rot) { transform_.transform.rotate = rot; transform_.isDirty = true; }
    void SetScale(const Vector3& scale) { transform_.transform.scale = scale; transform_.isDirty = true; }
    void SetColor(const Vector4& color) { material_.color = color; }
    void SetTexture(const std::string& path) { material_.texturePath = path; }
    void SetShape(PrimitiveType type) { mesh_.ChangeMesh(type); transform_.isDirty = true; }
    void SetCustomPSO(ID3D12PipelineState* pso) { if (mesh_.resource) mesh_.resource->SetCustomPSO(pso); }
    void SetCustomCBVAddress(D3D12_GPU_VIRTUAL_ADDRESS addr) { if (mesh_.resource) mesh_.resource->SetCustomCBVAddress(addr); }

    /**
     * @brief カスタムの PrimitiveData を用いて現在のリソースを破棄し再初期化する
     * @param[in] data 再生成に使用する頂点・インデックスデータ
     */
    void ReinitializeMesh(const PrimitiveData& data);

    void SetCullingEnabled(bool enabled) { isCullingEnabled_ = enabled; }
    void SetCastShadows(bool cast) { castShadows_ = cast; }
    bool GetCastShadows() const { return castShadows_; }

    // --- コールバック ---
    using CustomSyncCallback = std::function<void(uint32_t frameIndex)>;
    /**
     * @brief 描画前のバッファ同期時に呼び出されるコールバックを設定する
     * @param[in] callback 現在のフレームインデックスを受け取る関数
     */
    void SetCustomSyncCallback(CustomSyncCallback callback) { customSyncCallback_ = std::move(callback); }

    // --- 静的各種マネージャの設定 ---
    static void SetTextureManager(TextureManager* texM) { textureManager_ = texM; }
    static void SetDrawManager(DrawManager* drawM) { drawManager_ = drawM; }
    static void SetDebugUI(DebugUI* ui) { ui_ = ui; }
    static void SetEngine(class IrufemiEngine* engine) { engine_ = engine; }

private:
    PrimitiveTransform transform_; //!< トランスフォームコンポーネント
    MeshDesc mesh_;              // 形状データ
    MaterialDesc material_;      // マテリアルデータコンポーネント
    bool isCullingEnabled_ = true; //!< 視錐台カリングの有効フラグ
    bool castShadows_ = true;      //!< 影を落とすフラグ
    CustomSyncCallback customSyncCallback_; //!< カスタムの同期処理用コールバック

    // 静的ポインタ（既存の設計パターンを継承）
    static TextureManager* textureManager_;
    static DrawManager* drawManager_;
    static DebugUI* ui_;
    static class IrufemiEngine* engine_;
};
