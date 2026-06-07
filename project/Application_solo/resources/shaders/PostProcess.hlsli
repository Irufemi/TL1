#pragma once

// --- ポストプロセス モード定義 (C++の PostProcessMode と一致させる) ---
static const int32_t kPostProcessMode_None = 0;
static const int32_t kPostProcessMode_Grayscale = 1;
static const int32_t kPostProcessMode_Sepia = 2;
static const int32_t kPostProcessMode_Vignette = 3;
static const int32_t kPostProcessMode_Smoothing = 4;
static const int32_t kPostProcessMode_GaussianFilter = 5;
static const int32_t kPostProcessMode_DepthBasedOutline = 6;
static const int32_t kPostProcessMode_RadialBlur = 7;
static const int32_t kPostProcessMode_Dissolve = 8;
static const int32_t kPostProcessMode_Noise = 9;
static const int32_t kPostProcessMode_HSV = 10;
static const int32_t kPostProcessMode_ToneMapping = 11;
static const int32_t kPostProcessMode_Fade = 12;
static const int32_t kPostProcessMode_Slide = 13;
static const int32_t kPostProcessMode_Glitch = 15;

// --- ヘルパー関数 ---

// RGB -> HSV
float32_t3 RGBToHSV(float32_t3 rgb) {
    float32_t maxVal = max(rgb.r, max(rgb.g, rgb.b));
    float32_t minVal = min(rgb.r, min(rgb.g, rgb.b));
    float32_t delta = maxVal - minVal;
    float32_t3 hsv = float32_t3(0, 0, maxVal);
    if (delta > 0) {
        if (maxVal == rgb.r) hsv.x = (rgb.g - rgb.b) / delta;
        else if (maxVal == rgb.g) hsv.x = 2 + (rgb.b - rgb.r) / delta;
        else hsv.x = 4 + (rgb.r - rgb.g) / delta;
        hsv.x /= 6.0;
        if (hsv.x < 0) hsv.x += 1.0;
        hsv.y = delta / maxVal;
    }
    return hsv;
}

// HSV -> RGB
float32_t3 HSVToRGB(float32_t3 hsv) {
    float32_t h = hsv.x * 6.0;
    float32_t s = hsv.y;
    float32_t v = hsv.z;
    float32_t C = v * s;
    float32_t X = C * (1.0 - abs(fmod(h, 2.0) - 1.0));
    float32_t m = v - C;
    float32_t3 rgb = float32_t3(0, 0, 0);
    if (h < 1.0) rgb = float32_t3(C, X, 0);
    else if (h < 2.0) rgb = float32_t3(X, C, 0);
    else if (h < 3.0) rgb = float32_t3(0, C, X);
    else if (h < 4.0) rgb = float32_t3(0, X, C);
    else if (h < 5.0) rgb = float32_t3(X, 0, C);
    else rgb = float32_t3(C, 0, X);
    return rgb + m;
}

float32_t WrapValue(float32_t value, float32_t minRange, float32_t maxRange) {
    float32_t range = maxRange - minRange;
    float32_t modValue = fmod(value - minRange, range);
    if (modValue < 0) modValue += range;
    return minRange + modValue;
}

float32_t rand2dTo1d(float2 value) {
    float2 dot_res = dot(value, float2(12.9898, 78.233));
    return frac(sin(dot_res.x) * 43758.5453);
}

// ACES ToneMapping
float32_t3 ACESFilm(float32_t3 x) {
    float32_t a = 2.51f;
    float32_t b = 0.03f;
    float32_t c = 2.43f;
    float32_t d = 0.59f;
    float32_t e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

// 2次元ガウス関数
float32_t gauss(float32_t x, float32_t y, float32_t sigma) {
    static const float32_t PI = 3.1415926535f;
    float32_t exponent = -(x * x + y * y) * rcp(2.0f * sigma * sigma);
    float32_t denominator = 2.0f * PI * sigma * sigma;
    return exp(exponent) * rcp(denominator);
}

// --- エフェクト関数群 ---

// 1. Grayscale
float32_t3 ApplyGrayscale(float32_t3 color) {
    float32_t value = dot(color, float32_t3(0.2125f, 0.7154f, 0.0721f));
    return float32_t3(value, value, value);
}

// 2. Sepia
float32_t3 ApplySepia(float32_t3 color) {
    float32_t value = dot(color, float32_t3(0.2125f, 0.7154f, 0.0721f));
    return value * float32_t3(1.0f, 74.0f / 107.0f, 43.0f / 107.0f);
}

// 3. Vignette
float32_t3 ApplyVignette(float32_t3 color, float32_t2 uv, float32_t radius, float32_t softness, float32_t3 vignetteColor) {
    float dist = distance(uv, float2(0.5f, 0.5f));
    float vignette = smoothstep(radius, radius - softness, dist);
    return lerp(vignetteColor, color, vignette);
}

// 4. Noise
float32_t3 ApplyNoise(float32_t3 color, float32_t2 uv, float32_t intensity, float32_t time) {
    float32_t random = rand2dTo1d(uv * (time + 1.0f));
    float32_t noise = lerp(1.0f, random, intensity);
    return color * noise;
}

// 5. HSV
float32_t3 ApplyHSV(float32_t3 color, float32_t hue, float32_t saturation, float32_t value) {
    float32_t3 hsv = RGBToHSV(color);
    hsv.x = WrapValue(hsv.x + hue, 0.0, 1.0);
    hsv.y = saturate(hsv.y + saturation);
    hsv.z = saturate(hsv.z + value);
    return HSVToRGB(hsv);
}

// 6. ToneMapping
float32_t3 ApplyToneMapping(float32_t3 color, float32_t exposure) {
    return ACESFilm(color * exposure);
}

// 7. Fade
float32_t3 ApplyFade(float32_t3 color, float32_t3 fadeColor, float32_t intensity) {
    return lerp(color, fadeColor, intensity);
}

// 8. Slide
float32_t3 ApplySlide(float32_t3 color, float32_t2 uv, float32_t3 slideColor, float32_t threshold) {
    float32_t factor = smoothstep(threshold - 0.02f, threshold, uv.x);
    return lerp(slideColor, color, factor);
}

// 9. Dissolve (注: mask はサンプリング済みを渡す)
float32_t3 ApplyDissolve(float32_t3 color, float32_t mask, float32_t threshold, float32_t edgeRange, float32_t3 edgeColor) {
    if (mask <= threshold) return float32_t3(-1, -1, -1); // 棄却フラグとして負の値を返す
    float32_t edge = 1.0f - smoothstep(threshold, threshold + edgeRange, mask);
    return color + edge * edgeColor;
}

// 10. Glitch
/**
 * @brief グリッチエフェクトを適用する
 * @param color 現在のピクセルカラー
 * @param uv テクスチャ座標
 * @param time 時間
 * @param intensity グリッチの強度
 * @param tex サンプリングする元のテクスチャ
 * @param smp サンプラステート
 * @return グリッチ適用後のカラー
 */
float32_t3 ApplyGlitch(float32_t3 color, float32_t2 uv, float32_t time, float32_t intensity, Texture2D<float32_t4> tex, SamplerState smp) {
    // ブロックノイズ判定とUVの水平ズレ
    float2 block = floor(uv * float2(24.0, 9.0));
    float noise = rand2dTo1d(block + time);
    float offsetX = (noise - 0.5) * 0.1 * intensity;
    float2 displacedUv = saturate(uv + float2(offsetX, 0.0));

    // RGBシフト（色ズレサンプリング）
    float shift = 0.02 * intensity;
    float r = tex.SampleLevel(smp, displacedUv + float2(shift, 0.0), 0).r;
    float g = tex.SampleLevel(smp, displacedUv, 0).g;
    float b = tex.SampleLevel(smp, displacedUv - float2(shift, 0.0), 0).b;
    
    // スキャンラインを加味して返す
    float scanline = sin(uv.y * 800.0 + time * 10.0) * 0.04 * intensity;
    return saturate(float32_t3(r, g, b) + scanline);
}

// 11. Outline
float32_t3 ApplyDepthBasedOutline(float32_t3 color, float32_t2 uv, float32_t2 uvStepSize, float32_t4x4 projectionInverse, float32_t intensity, Texture2D<float32_t> depthTex, SamplerState smp) {
    float32_t2 difference = 0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float32_t depth = depthTex.Sample(smp, uv + float32_t2(x, y) * uvStepSize);
            float32_t4 viewSpace = mul(float32_t4(0, 0, depth, 1), projectionInverse);
            float32_t vz = viewSpace.z / viewSpace.w;
            
            float32_t wx = (x == 0) ? 0 : (x < 0 ? -1.0/6.0 : 1.0/6.0);
            float32_t wy = (y == 0) ? 0 : (y < 0 ? -1.0/6.0 : 1.0/6.0);
            difference.x += vz * wx;
            difference.y += vz * wy;
        }
    }
    float32_t weight = saturate(length(difference) * intensity);
    return color * (1.0f - weight);
}

// 12. RadialBlur
float32_t3 ApplyRadialBlur(float32_t3 color, float32_t2 uv, float32_t2 center, float32_t blurWidth, int32_t samples, Texture2D<float32_t4> tex, SamplerState smp) {
    float32_t2 dir = uv - center;
    float32_t dist = length(dir);
    
    // 中心点からの距離に応じてサンプリング数を最適化 (距離0なら1回、距離0.5以上なら最大回数)
    int32_t actualSamples = max(1, int32_t(float32_t(samples) * saturate(dist * 2.0f)));
    
    float32_t3 sum = 0;
    for (int32_t j = 0; j < actualSamples; ++j) {
        sum += tex.SampleLevel(smp, uv + dir * blurWidth * float32_t(j), 0).rgb;
    }
    return sum / float32_t(actualSamples);
}

// 13. Separable Gaussian Blur (1D)
float32_t3 ApplyGaussian1D(Texture2D<float32_t4> tex, SamplerState smp, float32_t2 uv, float32_t2 direction, float32_t sigma, int32_t halfSize) {
    uint32_t width, height;
    tex.GetDimensions(width, height);
    float32_t2 texelSize = rcp(float32_t2(width, height));
    
    float32_t3 sum = 0.0f;
    float32_t weightTotal = 0.0f;
    
    for (int32_t i = -halfSize; i <= halfSize; ++i) {
        float32_t w = gauss(float32_t(i), 0.0f, sigma); // gauss関数のシグネチャ(2D)に合わせるためy=0.0fを渡すか、1D用関数を使う。※この下に1D版も定義します
        float32_t2 offset = direction * float32_t(i) * texelSize;
        sum += tex.SampleLevel(smp, uv + offset, 0).rgb * w;
        weightTotal += w;
    }
    
    if (weightTotal > 0.0f) {
        return sum * rcp(weightTotal);
    }
    return tex.SampleLevel(smp, uv, 0).rgb;
}

// 1次元ガウス関数（オーバーロード）
float32_t gauss1D(float32_t x, float32_t sigma) {
    if (sigma <= 0.0f) return 1.0f;
    static const float32_t PI = 3.1415926535f;
    float32_t exponent = -(x * x) * rcp(2.0f * sigma * sigma);
    float32_t denominator = sqrt(2.0f * PI) * sigma;
    return exp(exponent) * rcp(denominator);
}

// 13. Separable Gaussian Blur (1D) 改良版
float32_t3 ApplyGaussian1D_Optimized(Texture2D<float32_t4> tex, SamplerState smp, float32_t2 uv, float32_t2 direction, float32_t sigma, int32_t halfSize) {
    uint32_t width, height;
    tex.GetDimensions(width, height);
    float32_t2 texelSize = rcp(float32_t2(width, height));
    
    float32_t3 sum = 0.0f;
    float32_t weightTotal = 0.0f;
    
    for (int32_t i = -halfSize; i <= halfSize; ++i) {
        float32_t w = gauss1D(float32_t(i), sigma);
        float32_t2 offset = direction * float32_t(i) * texelSize;
        sum += tex.SampleLevel(smp, uv + offset, 0).rgb * w;
        weightTotal += w;
    }
    
    if (weightTotal > 0.0f) {
        return sum * rcp(weightTotal);
    }
    return tex.SampleLevel(smp, uv, 0).rgb;
}

// 14. Separable Box Blur (1D)
float32_t3 ApplyBoxBlur1D(Texture2D<float32_t4> tex, SamplerState smp, float32_t2 uv, float32_t2 direction, int32_t halfSize) {
    uint32_t width, height;
    tex.GetDimensions(width, height);
    float32_t2 texelSize = rcp(float32_t2(width, height));
    
    float32_t3 sum = 0.0f;
    for (int32_t i = -halfSize; i <= halfSize; ++i) {
        float32_t2 offset = direction * float32_t(i) * texelSize;
        sum += tex.SampleLevel(smp, uv + offset, 0).rgb;
    }
    
    int32_t sampleCount = (halfSize * 2) + 1;
    return sum / float32_t(sampleCount);
}
