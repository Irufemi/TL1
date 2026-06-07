#include "Fullscreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct SlideParams {
    float32_t4 color;
    float32_t threshold;
};

ConstantBuffer<SlideParams> gParams : register(b0);

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;
    
    // 元のテクスチャカラーを取得
    float32_t4 texColor = gTexture.Sample(gSampler, input.texcoord);
    
    // 境界を少しぼかすための幅
    float32_t edgeWidth = 0.02f;
    
    // UV.x が threshold 以下ならスライド色、以上なら元の色
    // smoothstep を使って境界を滑らかにする
    float32_t factor = smoothstep(gParams.threshold - edgeWidth, gParams.threshold, input.texcoord.x);
    
    output.color.rgb = lerp(gParams.color.rgb, texColor.rgb, factor);
    output.color.a = texColor.a;
    
    return output;
}
