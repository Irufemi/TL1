#pragma once

#include "../../Core/Math/Vector2.h"
#include "../../Core/Math/Vector4.h"
#include "../../Core/Math/Matrix4x4.h"
#include "../DirectX/RenderTexture.h"
#include <d3d12.h>
#include <wrl/client.h>
#ifndef __IInspectable_INTERFACE_DEFINED__
// ComPtr の一部のテンプレート展開で IInspectable が必要になる場合がある
typedef struct IInspectable IInspectable;
#endif
#include <memory>
#include <vector>
#include <array>
#include <cstdint>
#include <algorithm>
#include <mutex>

/**
 * @enum PostProcessMode
 * @brief ポストプロセスの各モードを定義する列挙型
 */
enum class PostProcessMode {
    None,               ///< 何も適用しない
    Grayscale,          ///< グレースケール
    Sepia,              ///< セピア調
    Vignette,           ///< ビネット（画面端を暗くする）
    Smoothing,          ///< 平滑化（ぼかし）
    GaussianFilter,     ///< ガウスぼかし
    DepthBasedOutline,  ///< 深度バッファを使用したアウトライン抽出
    RadialBlur,         ///< 放射状ぼかし
    Dissolve,           ///< ディゾルブ（ノイズテクスチャによる消失演出）
    Noise,              ///< ランダムノイズ粒子
    HSV,                ///< HSV色空間による色調整
    ToneMapping,        ///< トーンマッピング（ACES）
    Fade,               ///< フェード（指定色への塗りつぶし）
    Slide,              ///< スライド（ワイプ演出）
    Bloom,              ///< ブルーム（高輝度抽出による発光）
    Glitch,             ///< グリッチ（ノイズや色収差による映像の乱れ）
};

class DirectXCommon;

/**
 * @class PostProcessManager
 * @brief ポストプロセス（画面全体にかけるエフェクト）を管理するクラス。
 * 
 * マルチパスレンダリングに対応しており、複数のエフェクトをスタックに追加して重ね掛けできます。
 * 
 * @par 推奨される適用順序:
 * プロの現場でも、以下のような順序で適用することで意図した映像表現になります。
 * 1. 色調補正系 (ToneMapping, Grayscale, Sepia, HSV 等)
 * 2. 空間・ぼかし系 (Smoothing, GaussianFilter, RadialBlur 等)
 * 3. 画面演出系 (Vignette, Noise, Glitch, Dissolve 等)
 * 4. 画面遷移系 (Fade, Slide)
 * 
 * @par シーンでの使用例:
 * @code
 * // 1. マネージャーの取得
 * auto* pp = engine->GetPostProcessManager();
 * 
 * // 2. エフェクトのリセットと追加
 * pp->ClearActiveModes();
 * pp->AddActiveMode(PostProcessMode::DepthBasedOutline);
 * pp->AddActiveMode(PostProcessMode::Noise);
 * 
 * // 3. 各エフェクトのパラメータ調整
 * pp->GetNoiseParams().intensity = 0.2f;
 * @endcode
 */
class PostProcessManager {
public:
    using Mode = PostProcessMode;

    struct PostProcessWorkspace {
        class RenderTexture* workTextures[2] = { nullptr, nullptr };
        class RenderTexture* bloomExtract = nullptr;
        class RenderTexture* bloomBlur = nullptr;
    };

    /**
     * @struct NoiseParams
     * @brief ノイズエフェクト用パラメータ
     */
    struct NoiseParams {
        float intensity = 0.5f; ///< ノイズの強度 (0.0 ~ 1.0)
        float time = 0.0f;      ///< 時間経過（内部で更新される）
    };

    /**
     * @struct VignetteParams
     * @brief ビネットエフェクト用パラメータ
     */
    struct VignetteParams {
        Vector4 color = { 0.0f, 0.0f, 0.0f, 1.0f }; ///< ビネットの色 (RGB)
        float radius = 0.8f;    ///< 減衰の開始半径 (0.0~1.5)
        float softness = 0.5f;  ///< 減衰の柔らかさ (0.0~1.0)
        float pad[2];           // 16バイトアライメント用パディング
    };

    /**
     * @struct SmoothingParams
     * @brief 平滑化エフェクト用パラメータ
     */
    struct SmoothingParams {
        Vector2 direction = { 1.0f, 0.0f }; ///< ぼかしの方向 ({1,0}で横, {0,1}で縦)
        int32_t kernelSize = 3; ///< カーネルサイズ (奇数推奨)
        float pad;
    };

    /**
     * @struct GaussianParams
     * @brief ガウスぼかし用パラメータ
     */
    struct GaussianParams {
        Vector2 direction = { 1.0f, 0.0f }; ///< ぼかしの方向 ({1,0}で横, {0,1}で縦)
        float sigma = 2.0f;     ///< 標準偏差（ぼけ具合）
        int32_t kernelSize = 3; ///< カーネルサイズ (奇数推奨)
    };

    /**
     * @struct RadialBlurParams
     * @brief 放射状ぼかし用パラメータ
     */
    struct RadialBlurParams {
        Vector2 center = { 0.5f, 0.5f }; ///< ぼかしの中心点 (UV空間 0.0 ~ 1.0)
        float blurWidth = 0.01f;         ///< ぼかしの幅
        int32_t numSamples = 10;         ///< サンプル数
    };

    /**
     * @struct OutlineParams
     * @brief アウトラインエフェクト用パラメータ
     */
    struct OutlineParams {
        float intensity = 6.0f;         ///< アウトラインの強度
        float pad[3];
        Matrix4x4 projectionInverse;    ///< 逆投影行列 (自動でセットされる)
    };

    /**
     * @struct DissolveParams
     * @brief ディゾルブエフェクト用パラメータ
     */
    struct DissolveParams {
        Vector4 edgeColor = { 1.0f, 0.4f, 0.3f, 1.0f }; ///< 境界線の色
        Vector4 backgroundColor = { 0.0f, 0.0f, 0.0f, 1.0f }; ///< 背景色 (追加)
        float threshold = 0.0f;                         ///< 消失しきい値 (0.0 ~ 1.0)
        float edgeRange = 0.03f;                        ///< 境界線の幅
        int32_t noiseType = 0;                          ///< 使用するノイズテクスチャのインデックス (0 or 1)
    };

    /**
     * @struct HSVParams
     * @brief HSVエフェクト用パラメータ
     */
    struct HSVParams {
        float hue = 0.0f;        ///< 色相オフセット (-1.0 ~ 1.0)
        float saturation = 0.0f; ///< 彩度オフセット (-1.0 ~ 1.0)
        float value = 0.0f;      ///< 明度オフセット (-1.0 ~ 1.0)
    };

    /**
     * @struct ToneMappingParams
     * @brief トーンマッピングエフェクト用パラメータ
     */
    struct ToneMappingParams {
        float exposure = 1.0f;   ///< 露出補正 (0.0 ~ )
    };

    /**
     * @struct FadeParams
     * @brief フェードエフェクト用パラメータ
     */
    struct FadeParams {
        Vector4 color = { 0.0f, 0.0f, 0.0f, 1.0f }; ///< フェード色
        float intensity = 0.0f;                      ///< 強度 (0.0 ~ 1.0)
    };

    /**
     * @struct SlideParams
     * @brief スライドエフェクト用パラメータ
     */
    struct SlideParams {
        Vector4 color = { 0.0f, 0.0f, 0.0f, 1.0f }; ///< スライドの色
        float threshold = 0.0f;                      ///< 進行度 (0.0 ~ 1.0)
    };

    /**
     * @struct BloomParams
     * @brief ブルームエフェクト用パラメータ
     */
    struct BloomParams {
        Vector2 direction = { 1.0f, 0.0f }; ///< ぼかしの方向 ({1,0}で横, {0,1}で縦)
        float threshold = 0.8f;             ///< 高輝度抽出のしきい値
        float sigma = 3.0f;                 ///< ぼかしの強さ
        float intensity = 1.0f;             ///< ブルームの強度
        int32_t kernelSize = 21;            ///< ぼかしのカーネルサイズ
    };

    /**
     * @struct GlitchParams
     * @brief グリッチエフェクト用パラメータ
     */
    struct GlitchParams {
        float intensity = 1.0f; ///< グリッチの強さ
        float time = 0.0f;      ///< 時間経過（内部で更新される）
    };

    /**
     * @struct CombinedParams
     * @brief 統合ポストプロセス用定数バッファ構造体
     */
    struct CombinedParams {
        int32_t effectCount;
        int32_t pad0[3]; // HLSLの int4[4] の開始位置（16バイト境界）に合わせるためのパディング
        int32_t effects[16];

        // Vignette
        Vector4 vignetteColor;
        float vignetteRadius;
        float vignetteSoftness;
        float pad1[2];

        // Noise
        float noiseIntensity;
        float noiseTime;
        float pad_noise[2]; // HLSLの float4(dissolveEdgeColor) 用に16バイト境界までパディング

        // Dissolve
        Vector4 dissolveEdgeColor;
        Vector4 dissolveBackgroundColor;
        float dissolveThreshold;
        float dissolveEdgeRange;

        // HSV
        float hsvHue;
        float hsvSaturation;
        float hsvValue;

        // ToneMapping
        float toneMappingExposure;
        float pad2[2]; // HLSLの float4(fadeColor) 用に16バイト境界までパディング

        // Fade
        Vector4 fadeColor;
        float fadeIntensity;
        float pad3[3]; // HLSLの float4(slideColor) 用に16バイト境界までパディング

        // Slide
        Vector4 slideColor;
        float slideThreshold;
        float pad4[3]; // HLSLの Matrix(projectionInverse) 用に16バイト境界までパディング

        // Outline
        Matrix4x4 projectionInverse;
        float outlineIntensity;
        float pad_outline[3]; // HLSLの float4境界に合わせるためのパディング

        // RadialBlur
        Vector2 radialBlurCenter;
        float radialBlurWidth;
        int32_t radialBlurSamples;

        // Glitch
        float glitchIntensity;
        float glitchTime;
        float pad6[2]; // HLSLの float4 境界に合わせるためのパディング
    };

public:
    /**
     * @brief ポストプロセスの初期化
     * @param dxCommon DirectX基盤クラス
     * @param rtvFormat 最終的な出力先のRTVフォーマット
     */
    void Initialize(DirectXCommon* dxCommon, DXGI_FORMAT rtvFormat);



    /**
     * @brief 更新処理
     * @param totalTime 累計時間（ノイズ等のアニメーションに使用）
     */
    void Update(float totalTime);

    /**
     * @brief 描画実行（マルチパス対応）
     * @param commandList コマンドリスト
     * @param srcTexture 元となるレンダリングテクスチャ（メインの描画結果）
     * @param rtvHandle 最終的な出力先（バックバッファ）のRTV
     * @param workspace Transient Resource が割り当てられた作業用領域
     */
    void Draw(ID3D12GraphicsCommandList* commandList, class RenderTexture* srcTexture, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, const PostProcessWorkspace& workspace);

    // --- Getters & Setters ---

    /** @brief 描画フェーズに備えて保留中の状態を同期する */
    void CommitPendingModes() {
        std::lock_guard<std::mutex> lock(modesMutex_);
        activeModes_ = pendingActiveModes_;
    }

    /** @brief 現在アクティブなエフェクトスタックを取得 */
    const std::vector<Mode>& GetActiveModes() const { return activeModes_; }

    /** @brief エフェクトをスタックに追加 */
    void AddActiveMode(Mode mode) {
        std::lock_guard<std::mutex> lock(modesMutex_);
        pendingActiveModes_.push_back(mode);
    }

    /** @brief 指定したエフェクトをスタックから削除 */
    void RemoveActiveMode(Mode mode) {
        std::lock_guard<std::mutex> lock(modesMutex_);
        pendingActiveModes_.erase(std::remove(pendingActiveModes_.begin(), pendingActiveModes_.end(), mode), pendingActiveModes_.end());
    }

    /** @brief 全てのエフェクトを解除（クリア） */
    void ClearActiveModes() {
        std::lock_guard<std::mutex> lock(modesMutex_);
        pendingActiveModes_.clear();
    }

    /** @brief エフェクトスタックを一括設定 */
    void SetActiveModes(const std::vector<Mode>& modes) {
        std::lock_guard<std::mutex> lock(modesMutex_);
        pendingActiveModes_ = modes;
    }

    /** @brief 全てのパラメータをデフォルト状態にリセットする */
    void ResetAllParams();

    /** @brief 指定したエフェクトが現在有効かチェック */
    bool HasActiveMode(Mode mode) const {
        return std::find(activeModes_.begin(), activeModes_.end(), mode) != activeModes_.end();
    }
    
    /** @brief 互換性のための単一セット (既存リストをクリアして1つ追加) */
    void SetMode(Mode mode) { 
        std::lock_guard<std::mutex> lock(modesMutex_);
        pendingActiveModes_.clear(); 
        if (mode != Mode::None) pendingActiveModes_.push_back(mode); 
    }

    /** @brief 互換性のための取得 (リストが空でなければ先頭を返す) */
    Mode GetMode() const { return activeModes_.empty() ? Mode::None : activeModes_.front(); }
    
    // 各エフェクトのパラメータ取得 (シーンからの演出用)
    NoiseParams& GetNoiseParams() { return noiseParams_; }
    VignetteParams& GetVignetteParams() { return vignetteParams_; }
    SmoothingParams& GetSmoothingParams() { return smoothingParams_; }
    GaussianParams& GetGaussianParams() { return gaussianParams_; }
    RadialBlurParams& GetRadialBlurParams() { return radialBlurParams_; }
    OutlineParams& GetOutlineParams() { return outlineParams_; }
    DissolveParams& GetDissolveParams() { return dissolveParams_; }
    HSVParams& GetHSVParams() { return hsvParams_; }
    ToneMappingParams& GetToneMappingParams() { return toneMappingParams_; }
    FadeParams& GetFadeParams() { return fadeParams_; }
    SlideParams& GetSlideParams() { return slideParams_; }
    BloomParams& GetBloomParams() { return bloomParams_; }
    GlitchParams& GetGlitchParams() { return glitchParams_; }

    void SetDissolveNoiseHandle(int index, D3D12_GPU_DESCRIPTOR_HANDLE handle) {
        if (index >= 0 && index < 2) dissolveNoiseHandle_[index] = handle;
    }
    
    void SetDepthSrvHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle) { depthSrvHandle_ = handle; }

private:
    void CreatePSOs();
    void CreateConstantBuffers();
    void DrawSinglePass(ID3D12GraphicsCommandList* commandList, Mode mode, RenderTexture* srcTexture, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, bool isFinalPass = false, ID3D12PipelineState* psoOverride = nullptr);
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateBuffer(size_t size);

private:
    DirectXCommon* dxCommon_ = nullptr;
    ID3D12Device* device_ = nullptr;
    ID3D12RootSignature* rootSig_ = nullptr;
    DXGI_FORMAT rtvFormat_ = DXGI_FORMAT_UNKNOWN;

    Mode mode_ = Mode::None; // 互換性用（内部では不使用にする）
    
    std::mutex modesMutex_;
    std::vector<Mode> activeModes_;
    std::vector<Mode> pendingActiveModes_;

    // PSOs
    struct PipelineSet {
        Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
    };
    // モードに対応するPSOを保持 (中間パス用: _UNORM)
    std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, 40> psos_;
    // 最終パス用 (スワップチェーン等の _SRGB 形式用)
    std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, 40> finalPsos_;

    // ブルーム専用 PSO (内部のパス用)
    Microsoft::WRL::ComPtr<ID3D12PipelineState> bloomExtractPSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> bloomBlurHPSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> bloomBlurVPSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> bloomCombinePSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> finalBloomCombinePSO_;

    // 統合ポストプロセス用 PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState> combinedPSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> finalCombinedPSO_;

    // 分離可能フィルタ用 PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState> smoothingBlurPSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> finalSmoothingBlurPSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> gaussianBlurPSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> finalGaussianBlurPSO_;

    // Constant Buffers
    Microsoft::WRL::ComPtr<ID3D12Resource> noiseCB_;
    NoiseParams* mappedNoise_ = nullptr;
    NoiseParams noiseParams_;

    Microsoft::WRL::ComPtr<ID3D12Resource> vignetteCB_;
    VignetteParams* mappedVignette_ = nullptr;
    VignetteParams vignetteParams_;

    Microsoft::WRL::ComPtr<ID3D12Resource> smoothingCB_;
    SmoothingParams* mappedSmoothing_ = nullptr;
    SmoothingParams smoothingParams_;

    Microsoft::WRL::ComPtr<ID3D12Resource> gaussianCB_;
    GaussianParams* mappedGaussian_ = nullptr;
    GaussianParams gaussianParams_;

    Microsoft::WRL::ComPtr<ID3D12Resource> radialBlurCB_;
    RadialBlurParams* mappedRadialBlur_ = nullptr;
    RadialBlurParams radialBlurParams_;

    Microsoft::WRL::ComPtr<ID3D12Resource> outlineCB_;
    OutlineParams* mappedOutline_ = nullptr;
    OutlineParams outlineParams_;

    Microsoft::WRL::ComPtr<ID3D12Resource> dissolveCB_;
    DissolveParams* mappedDissolve_ = nullptr;
    DissolveParams dissolveParams_;

    Microsoft::WRL::ComPtr<ID3D12Resource> hsvCB_;
    HSVParams* mappedHsv_ = nullptr;
    HSVParams hsvParams_;

    Microsoft::WRL::ComPtr<ID3D12Resource> toneMappingCB_;
    ToneMappingParams* mappedToneMapping_ = nullptr;
    ToneMappingParams toneMappingParams_;

    Microsoft::WRL::ComPtr<ID3D12Resource> fadeCB_;
    FadeParams* mappedFade_ = nullptr;
    FadeParams fadeParams_;

    Microsoft::WRL::ComPtr<ID3D12Resource> slideCB_;
    SlideParams* mappedSlide_ = nullptr;
    SlideParams slideParams_;

    Microsoft::WRL::ComPtr<ID3D12Resource> bloomCB_;
    BloomParams* mappedBloom_ = nullptr;
    BloomParams bloomParams_;

    Microsoft::WRL::ComPtr<ID3D12Resource> glitchCB_;
    GlitchParams* mappedGlitch_ = nullptr;
    GlitchParams glitchParams_;

    Microsoft::WRL::ComPtr<ID3D12Resource> combinedCB_;
    CombinedParams* mappedCombined_ = nullptr;
    CombinedParams combinedParams_;

    D3D12_GPU_DESCRIPTOR_HANDLE depthSrvHandle_{};
    std::array<D3D12_GPU_DESCRIPTOR_HANDLE, 2> dissolveNoiseHandle_{};

    // 状態追跡用は上に移動済み
};
