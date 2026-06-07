#include "Fullscreen.hlsli"

struct VignetteParams {
    float4 color;
    float radius;
    float softness;
    float2 pad;
};

ConstantBuffer<VignetteParams> gVignette : register(b0);
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput {
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;
    output.color = gTexture.Sample(gSampler, input.texcoord);

    // 中心からの距離で計算
    float dist = distance(input.texcoord, float2(0.5f, 0.5f));
    // smoothstepによる自然な減衰
    float vignette = smoothstep(gVignette.radius, gVignette.radius - gVignette.softness, dist);
    // 係数として補間
    output.color.rgb = lerp(gVignette.color.rgb, output.color.rgb, vignette);

    return output;
}
