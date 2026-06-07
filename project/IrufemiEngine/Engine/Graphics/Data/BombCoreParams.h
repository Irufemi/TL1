#pragma once
#include "Engine/Core/Math/Vector4.h"

struct BombCoreParams {
    Vector4 edgeColor;      // フチ（外側）の色
    Vector4 coreColor;      // 中心（内側）の色
    Vector4 crackColor;     // 亀裂から漏れ出る光の色
    float noiseScale;      // ノイズのスケール
    float distortion;      // 亀裂の歪み具合
    float pulseSpeed;      // 明滅の速度
    float intensity;       // 全体の発光強度
    float padding;         // 16バイトアライメント用
};
