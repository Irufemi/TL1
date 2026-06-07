#pragma once
#include "Engine/Math/MathStructs.h"
#include <string>

// エフェクトの種類ごとに定義するパラメータ構造体
struct EffectSetting {
    std::string name;            // エフェクトの識別名（"Hit", "Slash" など）
    std::string textureName;     // 使用するテクスチャキー ("white", "circle2" など)

    // 1回の再生で発生するパーティクル数
    uint32_t emitCountMin;
    uint32_t emitCountMax;

    // 寿命 (秒)
    float lifeTimeMin;
    float lifeTimeMax;

    // 遅延 (秒)
    float delay; 

    // 指定したY座標を下回ったら消滅させる（着地判定などに使用。-999.0 なら無効）
    float killAtY; 

    // 発生範囲 (再生座標からのオフセットと、さらにランダムなばらつき範囲)
    Vector3 positionOffset;
    Vector3 spawnAreaMin;
    Vector3 spawnAreaMax;

    // 初速度
    Vector3 velocityMin;
    Vector3 velocityMax;

    // 加速度（重力など）
    Vector3 acceleration;

    // 初期スケールと終了スケール
    Vector3 scaleMin;
    Vector3 scaleMax;
    Vector3 scaleEndMin;
    Vector3 scaleEndMax;

    // 初期回転 (ラジアン等)
    Vector3 rotationMin;
    Vector3 rotationMax;

    // 回転速度
    Vector3 rotationVelocityMin;
    Vector3 rotationVelocityMax;

    // 初期カラーと終了カラー
    Vector4 colorStartMin;
    Vector4 colorStartMax;
    Vector4 colorEndMin;
    Vector4 colorEndMax;

    // --- 機能フラグ ---
    bool isBillboard;       // 常にカメラを向くか
    bool isEmitter;         // 継続して発生するか（エミッターモード）
    float emitFrequency;    // エミッターモード時の発生間隔（秒）
    std::string meshType;   // 使用するメッシュ ("cube"など。空なら2Dスプライト)
    bool isSpherical;       // 球状に拡散するか（velocityMin.x を最小スピード、velocityMax.x を最大スピードとして扱う）
    bool isVelocityAligned; // 進行方向に自動で向きを合わせ、長さ（Z軸）を引き伸ばすか
    float velocityAlignmentScale; // 進行方向への引き伸ばし倍率（デフォルト 0.15）

    // --- 連鎖（子パーティクル）機能 ---
    std::string trailEffectName;   // 移動中に落とすエフェクト名（空なら何もしない）
    float trailFrequency;          // 落とす間隔（秒）
    std::string onDeathEffectName; // 寿命が尽きた瞬間に発生させるエフェクト名（空なら何もしない）

    // --- イージング設定 ---
    bool useEaseInScale;    // スケール変化に EaseIn (t^2) を適用するか
    bool useEaseInAlpha;    // アルファ変化に EaseIn (t^2) を適用するか
    bool useEaseOutScale;   // スケール変化に EaseOut (1-(1-t)^2) を適用するか
    bool useEaseOutAlpha;   // アルファ変化に EaseOut (1-(1-t)^2) を適用するか
    bool useSineScaleY;     // Y軸スケールにサイン波 (0 -> 1 -> 0) を適用するか
    bool useSineScaleXZ;    // XZ軸スケールにサイン波 (0 -> 1 -> 0) を適用するか
    bool isUniformScaleXZ;  // X軸とZ軸のスケールを同じ値にするか（正円を保つ）

    // デフォルトコンストラクタ（無難な値）
    EffectSetting() :
        name("Default"), textureName("white"),
        emitCountMin(1), emitCountMax(1),
        lifeTimeMin(1.0f), lifeTimeMax(1.0f), delay(0.0f), killAtY(-999.0f),
        positionOffset{ 0.0f, 0.0f, 0.0f }, 
        spawnAreaMin{ 0.0f, 0.0f, 0.0f }, spawnAreaMax{ 0.0f, 0.0f, 0.0f },
        velocityMin{ 0.0f, 0.0f, 0.0f }, velocityMax{ 0.0f, 0.0f, 0.0f },
        acceleration{ 0.0f, 0.0f, 0.0f },
        scaleMin{ 1.0f, 1.0f, 1.0f }, scaleMax{ 1.0f, 1.0f, 1.0f },
        scaleEndMin{ -1.0f, -1.0f, -1.0f }, scaleEndMax{ -1.0f, -1.0f, -1.0f },
        rotationMin{ 0.0f, 0.0f, 0.0f }, rotationMax{ 0.0f, 0.0f, 0.0f },
        rotationVelocityMin{ 0.0f, 0.0f, 0.0f }, rotationVelocityMax{ 0.0f, 0.0f, 0.0f },
        colorStartMin{ 1.0f, 1.0f, 1.0f, 1.0f }, colorStartMax{ 1.0f, 1.0f, 1.0f, 1.0f },
        colorEndMin{ -1.0f, -1.0f, -1.0f, -1.0f }, colorEndMax{ -1.0f, -1.0f, -1.0f, -1.0f },
        isBillboard(true), isEmitter(false), emitFrequency(0.5f), meshType(""),
        isSpherical(false), isVelocityAligned(false), velocityAlignmentScale(0.15f),
        trailEffectName(""), trailFrequency(0.1f), onDeathEffectName(""),
        useEaseInScale(false), useEaseInAlpha(false),
        useEaseOutScale(false), useEaseOutAlpha(false),
        useSineScaleY(false), useSineScaleXZ(false),
        isUniformScaleXZ(false)
    {}
};

// エフェクト再生時に渡すパラメータ構造体
struct EffectPlayParam {
    Vector3 position = { 0.0f, 0.0f, 0.0f };
    Vector3 rotation = { 0.0f, 0.0f, 0.0f };
    Vector3 scale = { 1.0f, 1.0f, 1.0f };
    bool isLoop = false;
    std::string modelKey = "";
    std::string textureKey = "";
    Vector3 velocityOverride = { 0.0f, 0.0f, 0.0f };
    Vector4 colorOverride = { 0.0f, 0.0f, 0.0f, 0.0f }; // 追加：上書きする色（(0,0,0,0)なら上書きなし）
};
