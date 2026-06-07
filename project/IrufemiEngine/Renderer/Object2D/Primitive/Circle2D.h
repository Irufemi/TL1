#include "../../Core/IRenderable.h"
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include "Engine/Graphics/Camera/Camera.h"
#include "Renderer/Object2D/Object2DResource.h"

// 前方宣言
class TextureManager;
class DrawManager;
class DebugUI;

struct Circle2DInfo {
    Vector3 center{ 0.0f, 0.0f, 0.0f };
    float   radius = 1.0f;
};

class Circle2D : public IRenderable {
protected:
    Circle2DInfo info_{};

    const float pi_ = 3.141592654f;
    uint32_t subdivision_ = 64; // 周方向分割

    // D3D12 リソース
    std::unique_ptr<Object2DResource> resource_ = nullptr;

    // 参照(非所有)
    static TextureManager* textureManager_;
    static DrawManager*    drawManager_;
    static DebugUI*        ui_;
    static class IrufemiEngine* engine_;

    // 状態
    bool useTexture_ = true;
    bool isTopMost_ = false; // 最前面UIフラグ
    int  selectedTextureIndex_ = 0;

public:
    Circle2D() = default;
    ~Circle2D() = default;

    // 初期化(subdivは偶数推奨)
    void Initialize(const std::string& textureName = "resources/whiteTexture.png", uint32_t subdiv = 64);

    // 更新(Debug UIなど)
    void Update();

    // 描画
    void SyncBeforeDraw() override;
    void Draw() override;

    // デバッグ
    void Debug(const char* circleName = " ");

    // 情報アクセス
    Object2DResource* GetD3D12Resource() { return resource_.get(); }
    const Circle2DInfo& GetInfo() const { return info_; }
    void SetInfo(const Circle2DInfo& info);
    void SetCenter(const Vector3& center);
    void SetRadius(float radius);

    /** @name 最前面描画設定 */
    ///@{
    /**
     * @brief ポストプロセスの影響を受けない最前面のUIとして描画するかを設定する
     * @param[in] isTopMost trueなら最前面(バックバッファ直接)に描画する
     */
    void SetTopMost(bool isTopMost) { isTopMost_ = isTopMost; }
    ///@}

    // 見た目
    void SetColor(const Vector4& color) { resource_->GetMaterialData()->color = color; }
    void SetRotateZ(float rad) { resource_->transform_.rotate = { 0.0f, 0.0f, rad }; }

    // テクスチャ制御
    void SetUseTexture(bool use) { useTexture_ = use; resource_->GetMaterialData()->hasTexture = use; }
    bool SetTextureByName(const std::string& textureName); // 名前からハンドル設定＆選択更新

    // 共有マネージャ設定
    static void SetTextureManager(TextureManager* texM) { textureManager_ = texM; }
    static void SetDrawManager(DrawManager* drawM) { drawManager_ = drawM; }
    static void SetDebugUI(DebugUI* ui) { ui_ = ui; }
    static void SetEngine(class IrufemiEngine* engine) { engine_ = engine; }

private:
    // 単位円ディスクのVB/IB作成(中心＋リング)
    void BuildUnitCircleFan(uint32_t subdiv);

    // WVPと材質の初期設定
    void InitMaterialAndMatrix();

    // 変換の更新(center/radius/rotate)
    void UpdateMatrix();
};




