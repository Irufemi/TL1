#pragma once
#include "EffectSetting.h"

class FireworksBurstEffectSetting : public EffectSetting {
public:
    FireworksBurstEffectSetting() {
        name = "FireworksBurst";
        meshType = "cube";
        isBillboard = false;
        
        // --- 連鎖（子パーティクル）機能 ---
        trailEffectName = "FireworksFlame"; // 移動中に落とす火の粉
        trailFrequency = 0.015f;            // 間隔をさらに詰め、滑らかな線にする
        onDeathEffectName = "FireworksSparkle"; // 消滅時にパチッと光る

        // --- 花火を綺麗にする新機能 ---
        isSpherical = true;
        isVelocityAligned = true;
        
        // ドカンと弾けるため大量に放出
        emitCountMin = 150;
        emitCountMax = 200;
        
        // 寿命に1秒程度の幅を持たせ、バラバラに消えるようにする（リアリティ向上）
        lifeTimeMin = 0.6f;
        lifeTimeMax = 0.7f;
        
        // 球状拡散のスピードを少し抑え、破裂サイズを全体のバランスに合わせる
        velocityMin = { 10.0f, 0.0f, 0.0f };
        velocityMax = { 15.0f, 0.0f, 0.0f };
        
        // 重力を追加（下方向にゆっくり加速して落ちていく）
        acceleration = { 0.0f, -8.0f, 0.0f };
        
        // 数を増やした分、一本一本をさらに細くし、繊細な光の集合体にする
        scaleMin = { 0.1f, 0.1f, 0.1f };
        scaleMax = { 0.2f, 0.2f, 0.2f };
        scaleEndMin = { 0.0f, 0.0f, 0.0f };
        scaleEndMax = { 0.0f, 0.0f, 0.0f };
        
        // isVelocityAligned が true の場合、自転は不要なので0にする
        rotationMin = { 0.0f, 0.0f, 0.0f };
        rotationMax = { 0.0f, 0.0f, 0.0f };
        
        // 線の存在感が強すぎないよう、最初から少し半透明（アルファ0.6〜0.8）にして「淡い光の線」にする
        colorStartMin = { 1.0f, 0.8f, 0.6f, 0.6f };
        colorStartMax = { 1.0f, 1.0f, 0.9f, 0.8f };
        
        // 消えるときは少し暗いオレンジにフェードアウト
        colorEndMin = { 0.8f, 0.4f, 0.0f, 0.0f };
        colorEndMax = { 1.0f, 0.6f, 0.2f, 0.0f };
    }
};
