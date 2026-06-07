#include "Fullscreen.hlsli"

// --- 定数バッファ ---
struct ToneMappingParams {
    float32_t exposure; // 露出補正
    float32_t3 padding;
};

ConstantBuffer<ToneMappingParams> gParams : register(b0);

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput {
    float32_t4 color : SV_Target0;
};

// ACES Filmic Tone Mapping 近似式
// Narkowicz 氏のフィッティングモデルを使用
float32_t3 ACESFilm(float32_t3 x) {
    float32_t a = 2.51f;
    float32_t b = 0.03f;
    float32_t c = 2.43f;
    float32_t d = 0.59f;
    float32_t e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;
    
    // テクスチャサンプリング (HDR リニア空間)
    float32_t4 texColor = gTexture.Sample(gSampler, input.texcoord);
    
    // 1. 露出（Exposure）調整
    // デフォルト 1.0。値を上げると画面全体が明るくなり、下げると暗くなる。
    float32_t3 color = texColor.rgb * gParams.exposure;
    
    // 2. ACES トーンマッピングの適用
    // 1.0 を超える輝度をなだらかに 1.0 へ収束させ、映画のような質感にする
    color = ACESFilm(color);
    
    // アルファ値はそのまま維持
    output.color = float32_t4(color, texColor.a);
    
    return output;
}
