#pragma once
#include "EffectSetting.h"

class FireworksFlameEffectSetting : public EffectSetting {
public:
    FireworksFlameEffectSetting() {
        name = "FireworksFlame";
        meshType = "cube";
        isBillboard = false;
        
        // 軌跡として残る炎（数個）
        emitCountMin = 1;
        emitCountMax = 2;
        
        // 寿命は少し残ってフワッと消える（0.3〜0.6秒）
        lifeTimeMin = 0.3f;
        lifeTimeMax = 0.6f;
        
        // ランダムな方向にフワッと散る
        velocityMin = { -1.0f, -1.0f, -1.0f };
        velocityMax = {  1.0f,  1.0f,  1.0f };
        
        // サイズ設定（線に対して違和感が出ないよう、かなり小さく細かくする）
        scaleMin = { 0.05f, 0.05f, 0.05f };
        scaleMax = { 0.15f, 0.15f, 0.15f };
        scaleEndMin = { 0.0f, 0.0f, 0.0f };
        scaleEndMax = { 0.0f, 0.0f, 0.0f };
        
        // 色は燃え残る炎のようなオレンジ
        colorStartMin = { 1.0f, 0.6f, 0.0f, 1.0f };
        colorStartMax = { 1.0f, 0.8f, 0.2f, 1.0f };
        colorEndMin = { 0.5f, 0.0f, 0.0f, 0.0f };
        colorEndMax = { 0.8f, 0.2f, 0.0f, 0.0f };
    }
};
