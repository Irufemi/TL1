#pragma once
#include "EffectSetting.h"

class FireworksAscendEffectSetting : public EffectSetting {
public:
    FireworksAscendEffectSetting() {
        name = "FireworksAscend";
        meshType = "cube";
        isBillboard = false;
        
        // --- 連鎖（子パーティクル）機能 ---
        trailEffectName = "FireworksFlame"; // 移動中に火の粉を落とす
        trailFrequency = 1.0f / 60.0f;      // 毎フレーム（60FPS想定）落とす
        
        // --- 親玉の動き設定 ---
        emitCountMin = 1;
        emitCountMax = 1;
        
        // 寿命は1.5秒（DebugSceneでのタイマーと同じ長さ）
        lifeTimeMin = 1.5f;
        lifeTimeMax = 1.5f;
        
        // 真上（Y軸プラス方向）に一定速度で飛ぶ
        velocityMin = { 0.0f, 15.0f, 0.0f };
        velocityMax = { 0.0f, 15.0f, 0.0f };
        
        // 親玉自身は見えなくていい（先端の光にしても良いが、火の粉が出るので透明でOK）
        scaleMin = { 0.0f, 0.0f, 0.0f };
        scaleMax = { 0.0f, 0.0f, 0.0f };
        scaleEndMin = { 0.0f, 0.0f, 0.0f };
        scaleEndMax = { 0.0f, 0.0f, 0.0f };
        
        colorStartMin = { 0.0f, 0.0f, 0.0f, 0.0f };
        colorStartMax = { 0.0f, 0.0f, 0.0f, 0.0f };
    }
};
