#include "Fullscreen.hlsli"
#include "PostProcess.hlsli"

/**
 * @file BoxBlur.PS.hlsl
 * @brief 分離型ボックスぼかしシェーダー（縦または横の1次元パス）
 */

struct SmoothingParams {
    float32_t2 direction;   // ぼかし方向 ({1,0} or {0,1})
    int32_t kernelSize;
    float32_t pad;
};

ConstantBuffer<SmoothingParams> gSmoothing : register(b0);
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PSOutput {
    float32_t4 color : SV_TARGET0;
};

PSOutput main(VertexShaderOutput input) {
    PSOutput output;
    
    int32_t halfSize = gSmoothing.kernelSize / 2;
    output.color.rgb = ApplyBoxBlur1D(gTexture, gSampler, input.texcoord, gSmoothing.direction, halfSize);
    output.color.a = 1.0f;
    
    return output;
}
