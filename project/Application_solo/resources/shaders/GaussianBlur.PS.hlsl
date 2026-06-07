#include "Fullscreen.hlsli"
#include "PostProcess.hlsli"

/**
 * @file GaussianBlur.PS.hlsl
 * @brief 分離型ガウスぼかしシェーダー（縦または横の1次元パス）
 */

struct BloomParams {
    float32_t2 direction;   // ぼかし方向 ({1,0} or {0,1})
    float32_t threshold;
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
    
    int32_t halfSize = gBloom.kernelSize / 2;
    output.color.rgb = ApplyGaussian1D_Optimized(gTexture, gSampler, input.texcoord, gBloom.direction, gBloom.sigma, halfSize);
    output.color.a = 1.0f;
    
    return output;
}
