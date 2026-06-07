#include "Fullscreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// SmoothingParams 定数バッファ
struct SmoothingParams {
    int32_t kernelSize; // 3, 5, 7, ...
};
ConstantBuffer<SmoothingParams> gParams : register(b0);

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;
    
    // 1. uvStepSize の算出 (1テクセル分のUV移動量)
    uint32_t width, height;
    gTexture.GetDimensions(width, height);
    float32_t2 uvStepSize = float32_t2(rcp(float32_t(width)), rcp(float32_t(height)));

    // 2. カーネルサイズから半径を算出
    // 例: kernelSize=3 なら radius=1, kernelSize=5 なら radius=2
    int32_t radius = (gParams.kernelSize - 1) / 2;
    // 最低でも半径1(3x3)は確保する（あるいは0ならそのまま返す）
    if (radius < 0) {
        output.color = gTexture.Sample(gSampler, input.texcoord);
        return output;
    }

    float32_t3 accumColor = float32_t3(0.0f, 0.0f, 0.0f);
    
    // 3. ループを回して周囲のテクセルをサンプリング
    for (int32_t x = -radius; x <= radius; ++x) {
        for (int32_t y = -radius; y <= radius; ++y) {
            // 現在のテクセル位置からのオフセット
            float32_t2 offset = float32_t2(float32_t(x), float32_t(y)) * uvStepSize;
            // サンプリングして加算
            accumColor += gTexture.Sample(gSampler, input.texcoord + offset).rgb;
        }
    }

    // 4. 合計色をピクセル数で割って平均化
    float32_t numPixels = float32_t(gParams.kernelSize * gParams.kernelSize);
    output.color.rgb = accumColor / numPixels;
    output.color.a = 1.0f;

    return output;
}
