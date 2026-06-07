#include "Fullscreen.hlsli"

struct OutlineParams {
    float32_t intensity;
    float32_t3 pad;
    float32_t4x4 projectionInverse;
};

ConstantBuffer<OutlineParams> gOutline : register(b0);
Texture2D<float32_t4> gTexture : register(t0);
Texture2D<float32_t> gDepthTexture : register(t1);
SamplerState gSampler : register(s0);
SamplerState gSamplerPoint : register(s1);

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

static const float32_t kPrewittHorizontalKernel[3][3] = {
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
};

static const float32_t kPrewittVerticalKernel[3][3] = {
    { -1.0f / 6.0f, -1.0f / 6.0f, -1.0f / 6.0f },
    { 0.0f, 0.0f, 0.0f },
    { 1.0f / 6.0f, 1.0f / 6.0f, 1.0f / 6.0f },
};

static const int32_t2 kIndex3x3[3][3] = {
    {{-1, -1}, {0, -1}, {1, -1}},
    {{-1,  0}, {0,  0}, {1,  0}},
    {{-1,  1}, {0,  1}, {1,  1}},
};

PixelShaderOutput main(VertexShaderOutput input) {
    uint32_t width, height;
    gTexture.GetDimensions(width, height);
    float32_t2 uvStepSize = float32_t2(1.0f / width, 1.0f / height);

    float32_t2 difference = float32_t2(0.0f, 0.0f);
    
    for (int32_t x = 0; x < 3; ++x) {
        for (int32_t y = 0; y < 3; ++y) {
            float32_t2 texcoord = input.texcoord + kIndex3x3[x][y] * uvStepSize;
            float32_t ndcDepth = gDepthTexture.Sample(gSamplerPoint, texcoord);
            // NDC -> View。P^{-1}においてxとyはzwに影響を与えないので0で良い
            float32_t4 viewSpace = mul(float32_t4(0.0f, 0.0f, ndcDepth, 1.0f), gOutline.projectionInverse);
            float32_t viewZ = viewSpace.z * rcp(viewSpace.w);
            
            difference.x += viewZ * kPrewittHorizontalKernel[x][y];
            difference.y += viewZ * kPrewittVerticalKernel[x][y];
        }
    }

    // 変化の長さをウェイトとして合成
    float32_t weight = length(difference);
    weight = saturate(weight * gOutline.intensity);

    PixelShaderOutput output;
    // エッジ部分を黒く表示するように合成
    output.color.rgb = (1.0f - weight) * gTexture.Sample(gSampler, input.texcoord).rgb;
    output.color.a = 1.0f;
    
    return output;
}
