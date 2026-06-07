#include "Fullscreen.hlsli"

// ラジアルブラー用定数バッファ
struct RadialBlurParams {
    float32_t2 center;      // 中心点 (0.5, 0.5 等)
    float32_t blurWidth;    // ぼかしの幅 (0.01 等)
    int32_t numSamples;     // サンプリング数 (10 等)
};

ConstantBuffer<RadialBlurParams> gParams : register(b0);
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PSOutput {
    float32_t4 color : SV_TARGET0;
};

PSOutput main(VertexShaderOutput input) {
    PSOutput output;
    
    float32_t2 uv = input.texcoord;
    
    // 中心から現在のUVへの方向（正規化せずに距離を含めたまま使うのがコツ）
    float32_t2 direction = uv - gParams.center;
    float32_t dist = length(direction);
    
    // 中心点からの距離に応じてサンプリング数を最適化 (距離0なら1回、距離0.5以上なら最大回数)
    int32_t actualSamples = max(1, int32_t(float32_t(gParams.numSamples) * saturate(dist * 2.0f)));
    
    float32_t3 sum = 0.0f;
    
    // 放射状にサンプリングして平均化
    for (int32_t i = 0; i < actualSamples; ++i) {
        // 現在の点から中心とは逆方向にサンプリング点を進めていく
        float32_t2 sampleCoord = uv + direction * gParams.blurWidth * float32_t(i);
        sum += gTexture.Sample(gSampler, sampleCoord).rgb;
    }
    
    // 平均化して出力
    output.color.rgb = sum * rcp(float32_t(actualSamples));
    output.color.a = 1.0f;
    
    return output;
}
