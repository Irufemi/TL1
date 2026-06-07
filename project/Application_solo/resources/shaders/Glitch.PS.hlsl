#include "Fullscreen.hlsli"
#include "PostProcess.hlsli"

struct GlitchParams {
    float intensity;
    float time;
};

ConstantBuffer<GlitchParams> gParams : register(b0);
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;
    
    // 現在のカラーを取得
    float32_t4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    
    // ApplyGlitchで処理
    output.color.rgb = ApplyGlitch(textureColor.rgb, input.texcoord, gParams.time, gParams.intensity, gTexture, gSampler);
    output.color.a = textureColor.a;
    
    return output;
}
