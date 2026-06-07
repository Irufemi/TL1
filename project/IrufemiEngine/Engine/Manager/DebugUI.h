#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <array>          
#include <cstddef>        
#include <memory>
#include <vector>
#include "../Core/Type/BlendMode.h"
#include "../Graphics/Pipeline/PSOManager.h"


// 前方宣言
class TextureManager;
class SceneManager;
class IrufemiEngine;
class DirectXCommon;
class Object3DResource;
class Object2DResource;
struct Material;
struct ObjMaterial;
struct Transform;
struct Matrix4x4;
struct DirectionalLight;
struct PointLight;
struct SpotLight;
struct AreaLight;
struct Sphere;
struct Animation;
struct LightningParams;

#ifdef USE_IMGUI

#include "imgui/imgui.h"
#include "imgui/ImGuizmo.h"


#endif // USE_IMGUI


/**
 * @class DebugUI
 * @brief ImGuiを使用したデバッグインターフェースを管理するクラス
 * @details 画面上に各種パラメータ（ライト、トランスフォーム、マテリアル等）を調整・確認するためのUIを表示します。
 *          パフォーマンス計測（FPS/フレーム時間）機能も備えています。
 */
class DebugUI{
private: // メンバ変数

    // ポインタ参照(非所有)

    DirectXCommon* dxCommon_ = nullptr;

    TextureManager* textureManager_ = nullptr;

    // ★追加: パフォーマンス履歴
    static constexpr size_t kPerfHistoryCount_ = 240;          // 約4秒分
    std::array<float, kPerfHistoryCount_> frameTimeHistory_{}; // ms
    size_t historyIndex_ = 0;
    bool historyFilled_ = false;

    // ★内部計算キャッシュ
    float cachedAvgMs_ = 0.0f;
    float cachedMinMs_ = 0.0f;
    float cachedMaxMs_ = 0.0f;
    float cachedP99Ms_ = 0.0f;   // 99th percentile frame time (≒ 1% worst)
    float cachedFps_ = 0.0f;
    void UpdatePerfStats_(float newFrameMs); // ★集計用内部関数

    bool showPerformance_ = true; // ★パフォーマンス情報の表示フラグ

    // --- ImGui用ライト編集テンプレート ---
    static std::unique_ptr<PointLight> templatePointLight_;
    static std::unique_ptr<SpotLight> templateSpotLight_;
    static std::unique_ptr<AreaLight> templateAreaLight_;

    uint32_t srvIndex_ = 0xFFFFFFFF;

public: // メンバ関数

    /** @name 初期化・終了処理 */
    ///@{
    /**
     * @brief 初期化
     */
    void Initialize(HWND hwnd, DirectXCommon* dxCommon);

    /**
     * @brief TextureManagerをセットする
     */
    void SetTextureManager(TextureManager* textureManager) { this->textureManager_ = textureManager; }

    /**
     * @brief 終了処理
     */
    void Shutdown();
    ///@}

#ifdef USE_IMGUI
    static LRESULT WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif // USE_IMGUI

    /** @name フレーム制御 */
    ///@{
    /**
     * @brief フレーム開始処理
     */
    void FrameStart();

    /**
     * @brief 描画コマンドの積み込み
     */
    void QueueDrawCommands();

    /**
     * @brief ポストプロセス以降の描画コマンド積み込み
     */
    void QueuePostDrawCommands();
    ///@}

    /** @name ライト・座標系のデバッグ */
    ///@{
    /**
     * @brief ライト全体の編集UIを表示する
     */
    static void DebugLights(
        DirectionalLight* directionalLight,
        std::vector<std::unique_ptr<PointLight>>& pointLights,
        std::vector<std::unique_ptr<SpotLight>>& spotLights,
        std::vector<std::unique_ptr<AreaLight>>& areaLights
    );

    /**
     * @brief 3Dトランスフォームの編集
     */
    static void DebugTransform(Transform& transform);

    /**
     * @brief 2Dトランスフォーム（スプライト用）の編集
     */
    static void DebugTransform2D(Transform& transform);

    /**
     * @brief トランスフォーム情報のテキスト表示
     */
    static void TextTransform(Transform& transform, const char* name = "");
    ///@}

    /** @name マテリアル・テクスチャのデバッグ */
    ///@{
    static void DebugMaterialBy3D(Material* material);
    
    static void DebugMaterialBy2D(Material* material);

    static void DebugObjMaterial(ObjMaterial* material, const char* unique_id = "");

    static void DebugMaterialByParticle(Material* material);

    /**
     * @brief テクスチャの選択・変更UI
     */
    void DebugTexture(Object3DResource* resource, int& selectedTextureIndex);
    void DebugTexture(Object2DResource* resource, int& selectedTextureIndex);


    static void DebugDirectionalLight(DirectionalLight* directionalLightData);

    static void DebugUvTransform(Transform& uvTransform);

    static void DebugUvTransform(Matrix4x4& uvTransform);

    /**
     * @brief マテリアルの個別オーバーライド設定を編集するUI
     */
    static void DebugMaterialOverrides(
        float* envCoef,
        int32_t* lightingMode,
        int32_t* useClamp,
        int32_t* enableLighting,
        const char* unique_id = ""
    );
    
    /**
     * @brief 電撃エフェクト調整UI
     */
    static void DebugLightning(LightningParams* params);

    /**
     * @brief アニメーション制御UI
     */
    static void DebugAnimationControl(const Animation& animation, float& currentTime, const char* unique_id = "");
    ///@}

    /** @name エンジン情報のデバッグ */
    ///@{
    /**
     * @brief 形状情報の表示（球体等）
     */
    static void DebugSphereInfo(Sphere& sphere);

    /**
     * @brief パフォーマンスオーバーレイの表示（FPS/ms表示）
     */
    void FPSDebug();

    /**
     * @brief シーン切り替えタブの表示
     */
    void SceneSelectorTab(SceneManager* sm);

    /**
     * @brief ポストプロセス調整タブの表示
     */
    void PostProcessTab(IrufemiEngine* engine);

    /**
     * @brief 統合デバッグウィンドウの開始
     */
    void BeginEngineDebugWindow();

    /**
     * @brief 統合デバッグウィンドウの終了
     */
    void EndEngineDebugWindow();


    /**
     * @brief PSO設定（描画ステート）の編集UI
     */
    static void DebugPsoSettings(
        BlendMode* blendMode,
        PSOManager::DepthWrite* depthWrite,
        PSOManager::CullMode* cullMode,
        const char* unique_id = "##PsoSettings"
    );
    ///@}
};
