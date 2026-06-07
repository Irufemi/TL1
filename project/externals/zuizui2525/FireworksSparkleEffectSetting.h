#pragma once
#include "EffectSetting.h"

class FireworksSparkleEffectSetting : public EffectSetting {
public:
    FireworksSparkleEffectSetting() {
        name = "FireworksSparkle";
        meshType = "cube";
        isBillboard = false;
        
        // パチッと小さく弾ける
        emitCountMin = 3;
        emitCountMax = 6;
        
        // 寿命は一瞬（0.1〜0.2秒）
        lifeTimeMin = 0.1f;
        lifeTimeMax = 0.2f;
        
        // 小さく四方に弾ける
        isSpherical = true;
        velocityMin = { 2.0f, 0.0f, 0.0f };
        velocityMax = { 5.0f, 0.0f, 0.0f };
        
        // サイズ設定（かなり小さくし、繊細な光の粒にする）
        scaleMin = { 0.05f, 0.05f, 0.05f };
        scaleMax = { 0.1f, 0.1f, 0.1f };
        scaleEndMin = { 0.0f, 0.0f, 0.0f };
        scaleEndMax = { 0.0f, 0.0f, 0.0f };
        
        // 色は明るい白〜ピカッと光る黄色
        colorStartMin = { 1.0f, 0.9f, 0.5f, 1.0f };
        colorStartMax = { 1.0f, 1.0f, 0.8f, 1.0f };
        colorEndMin = { 1.0f, 0.5f, 0.0f, 0.0f };
        colorEndMax = { 1.0f, 0.8f, 0.2f, 0.0f };
    }
};
