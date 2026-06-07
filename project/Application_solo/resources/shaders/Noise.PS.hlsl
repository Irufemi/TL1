#include "Fullscreen.hlsli"
#include "PostProcess.hlsli"

struct NoiseParams {
    float intensity;
    float time;
};

ConstantBuffer<NoiseParams> gParams : register(b0);
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;
    
    // 元の色を取得
    float32_t4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    
    // PostProcess.hlsli に定義されている共通関数を呼び出してノイズを適用
    output.color.rgb = ApplyNoise(textureColor.rgb, input.texcoord, gParams.intensity, gParams.time);
    output.color.a = textureColor.a;
    
    return output;
}
