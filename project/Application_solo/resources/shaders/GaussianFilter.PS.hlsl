#include "Fullscreen.hlsli"
#include "PostProcess.hlsli"

// ガウスフィルタ用定数バッファ
struct GaussianParams {
    float32_t2 direction;
    float32_t sigma;
    int32_t kernelSize;
};

ConstantBuffer<GaussianParams> gParams : register(b0);
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PSOutput {
    float32_t4 color : SV_TARGET0;
};

PSOutput main(VertexShaderOutput input) {
    PSOutput output;
    
    int32_t halfSize = gParams.kernelSize / 2;
    output.color.rgb = ApplyGaussian1D_Optimized(gTexture, gSampler, input.texcoord, gParams.direction, gParams.sigma, halfSize);
    output.color.a = 1.0f;
    
    return output;
}
