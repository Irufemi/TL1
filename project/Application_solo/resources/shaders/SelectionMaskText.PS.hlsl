#include "Text.hlsli"
#include "Material.hlsli"

ConstantBuffer<Material> gMaterial : register(b0);

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float32_t4 uvw = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t2 uv = uvw.xy;

    float32_t3 sampleColor = gTexture.Sample(gSampler, uv).rgb;
    float sd = median(sampleColor.r, sampleColor.g, sampleColor.b) - 0.5f;

    // 不透明度がほぼ無い場合はピクセルを破棄する
    if (sd < 0.0f) {
        discard;
    }

    // アウトラインマスク用なので真っ白を出力
    output.color = float32_t4(1.0f, 1.0f, 1.0f, 1.0f);

    return output;
}
