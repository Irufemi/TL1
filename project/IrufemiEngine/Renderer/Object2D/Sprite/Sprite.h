#include "../../Core/IRenderable.h"
#pragma once

#include <d3d12.h>
#include <vector>
#include <string>
#include <cstdint>
#include "Renderer/Object2D/Object2DResource.h"
#include "Engine/Core/Math/Vector2.h" 
#include <wrl.h>
#include <memory>

// 前方宣言
class TextureManager;
class DrawManager;
class DebugUI;
class CameraManager;

/**
 * @class Sprite
 * @brief 2Dスプライトを描画・管理するクラス
 * @details テクスチャの表示、座標変換（位置・回転・拡縮）、アンカーポイントの設定、トリミング（Rect指定）などを行います。
 */
class Sprite : public IRenderable {
private:

    std::unique_ptr<Object2DResource> resource_ = nullptr;

    bool isRotateY_ = true;

    int selectedTextureIndex_ = 0;

    static CameraManager* cameraManager_;

    static TextureManager* textureManager_;

    static DrawManager* drawManager_;

    static DebugUI* ui_;

    // サイズとアンカー
    Vector2 size_{ 640.0f, 360.0f };   // 既存の見た目互換のため初期値を640x360に
    Vector2 anchor_{ 0.0f, 0.0f };     // 左上(0,0) / 中央(0.5,0.5) / 右下(1,1)

    // フリップ状態
    bool isFlipX_ = false;
    bool isFlipY_ = false;

    // 現在のテクスチャのピクセルサイズ(取得できない場合は 0)
    Vector2 textureSize_{ 0.0f, 0.0f };

    // 切り出し矩形(ピクセル指定)
    bool  useTexRect_ = false;
    Vector2 texRectLeftTop_{ 0.0f, 0.0f }; // px
    Vector2 texRectSize_{ 0.0f, 0.0f };    // px

    /**
     * @brief 現在のテクスチャ解像度にスプライトサイズを合わせる（内部用）
     */
    void AdjustTextureSize(); 

    /**
     * @brief アンカー反映で頂点ローカル座標を更新（内部用）
     */
    void ApplyAnchorToVertices();

public: //メンバ関数
    /**
     * @brief デストラクタ
     */
    ~Sprite() = default;

    /**
     * @brief 初期化
     * @param[in] textureName 使用するテクスチャ名（ファイルパス）
     */
    void Initialize(const std::string& textureName = "resources/uvChecker.png");

    /**
     * @brief 更新処理
     * @details 行列計算や定数バッファの更新を行います。
     */
    void Update();

    /**
     * @brief 描画コマンドの積み込み
     */
    void SyncBeforeDraw() override;
    void Draw() override;

    /**
     * @brief デバッグ用UIの表示
     * @param[in] spriteName UIに表示する名前
     */
    void Debug(const char* spriteName = "");

    /** @name ゲッター */
    ///@{
    Object2DResource* GetD3D12Resource() { return this->resource_.get(); }
    const Vector2& GetSize() const { return size_; }
    const Vector2& GetAnchor() const { return anchor_; }
    const Vector2 GetPosition2D() const;
    const Vector3& GetRotation()const { return resource_ ? resource_->transform_.rotate : Vector3{}; }
    const Vector4& GetColor()const { return resource_->GetMaterialData()->color; }
    bool IsFlipX() const { return isFlipX_; }
    bool IsFlipY() const { return isFlipY_; }
    std::string GetTextureName() const;
    ///@}

    /** @name 設定用API */
    ///@{
    /**
     * @brief スプライトのサイズを設定
     */
    void SetSize(const float& width, const float& height);

    /**
     * @brief UIスケールを設定（解像度対応用）
     */
    void SetUIScale(float scale) { uiScale_ = scale; isDirty_ = true; }

    /**
     * @brief アンカーポイント（原点位置）を設定
     * @param[in] ax X座標 (0:左, 0.5:中央, 1:右)
     * @param[in] ay Y座標 (0:上, 0.5:中央, 1:下)
     */
    void SetAnchor(const float& ax, const float& ay) { anchor_ = { ax, ay }; isDirty_ = true; }

    /**
     * @brief 位置を設定
     */
    void SetPosition(const float& x, const float& y, const float& z = 0.0f) { if (resource_) { resource_->transform_.translate = { x, y, z }; } isDirty_ = true; }

    /**
     * @brief 回転を設定（Z軸回転）
     */
    void SetRotation(const float& rotate) { if (resource_) { resource_->transform_.rotate = Vector3{ 0.0f,0.0f,rotate }; } isDirty_ = true; }

    /**
     * @brief 色（RGBA）を設定
     */
    void SetColor(const Vector4& color) { resource_->GetMaterialData()->color = color; isDirty_ = true; }

    /**
     * @brief 反転状態を一括設定
     */
    void SetFlip(bool flipX, bool flipY) { isFlipX_ = flipX; isFlipY_ = flipY; isDirty_ = true; }
    void SetFlipX(bool flip) { isFlipX_ = flip; isDirty_ = true; }
    void SetFlipY(bool flip) { isFlipY_ = flip; isDirty_ = true; }

    /**
     * @brief テクスチャ内の切り出し範囲をピクセル単位で指定
     * @param[in] x 左上X
     * @param[in] y 左上Y
     * @param[in] w 幅
     * @param[in] h 高さ
     * @param[in] autoResize trueならスプライト自体のサイズも切り出しサイズに合わせる
     * @return 成功なら true
     */
    bool SetTextureRectPixels(int x, int y, int w, int h, bool autoResize = false);

    /**
     * @brief 切り出し指定を解除し、テクスチャ全体を表示するように戻す
     */
    void ClearTextureRect();
    
    /**
     * @brief テクスチャを動的に変更する
     */
    void SetTexture(const std::string& textureName);
    ///@}

    /** @name 便利エイリアス */
    ///@{
    void SetPositionTopLeft(const float& x, const float& y) { SetAnchor(0.0f, 0.0f); SetPosition(x, y); }
    void SetPositionCenter(const float& x, const float& y) { SetAnchor(0.5f, 0.5f); SetPosition(x, y); }
    ///@}

    /** @name 最前面描画設定 */
    ///@{
    /**
     * @brief ポストプロセスの影響を受けない最前面のUIとして描画するかを設定する
     * @param[in] isTopMost trueなら最前面(バックバッファ直接)に描画する
     */
    void SetTopMost(bool isTopMost) { isTopMost_ = isTopMost; }
    ///@}

    /** @name 静的メンバ設定（エンジン内部用） */
    ///@{
    static void SetTextureManager(TextureManager* texM) { textureManager_ = texM; }
    static TextureManager* GetTextureManager() { return textureManager_; }
    static void SetDrawManager(DrawManager* drawM) { drawManager_ = drawM; }
    static void SetDebugUI(DebugUI* ui) { ui_ = ui; }
    static void SetCameraManager(CameraManager* camM) { cameraManager_ = camM; }
    ///@}

private:
    // 行列更新の最適化用
    bool isDirty_ = true;
    bool isTopMost_ = false; // 最前面UIフラグ
    float uiScale_ = 1.0f;   // UIスケール
    Matrix4x4 lastViewMatrix_ = {};
    Matrix4x4 lastProjectionMatrix_ = {};
};


