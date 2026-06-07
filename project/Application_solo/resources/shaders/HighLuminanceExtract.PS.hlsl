#include "Fullscreen.hlsli"

/**
 * @file HighLuminanceExtract.PS.hlsl
 * @brief 高輝度部を抽出するシェーダー（ブルームの第1パス用）
 */

struct BloomParams {
    float32_t threshold;    // しきい値
    float32_t sigma;
    float32_t intensity;
    int32_t kernelSize;
};

ConstantBuffer<BloomParams> gBloom : register(b0);
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PSOutput {
    float32_t4 color : SV_TARGET0;
};

PSOutput main(VertexShaderOutput input) {
    PSOutput output;
    float32_t4 color = gTexture.Sample(gSampler, input.texcoord);
    
    // 輝度（ルミナンス）を計算 (ITU-R BT.709)
    float32_t luminance = dot(color.rgb, float32_t3(0.2126, 0.7152, 0.0722));
    
    // しきい値を超えている部分だけを抽出
    // 境界を滑らかにするため、saturate でグラデーションをかけることも検討
    float32_t extract = saturate(luminance - gBloom.threshold);
    
    // 輝度がしきい値を超えた割合で色を残す
    output.color = float32_t4(color.rgb * extract, color.a);
    
    return output;
}
