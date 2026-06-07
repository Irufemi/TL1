#include "Object3d.hlsli"
#include "PerFrame.hlsli"

ConstantBuffer<PerFrameData> gPerFrame : register(b2);

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(VertexShaderOutput input) : SV_TARGET {
    // UVスクロール（高速に下に流れる）
    float2 uv = input.texcoord;
    float time = gPerFrame.time;
    
    // 2種類のノイズを合成して複雑な揺らぎを作る
    float2 scrollUV1 = uv * float2(1.0, 0.5) + float2(0.0, -time * 3.0);
    float2 scrollUV2 = uv * float2(2.0, 1.0) + float2(0.0, -time * 5.0);
    
    float noise1 = gTexture.Sample(gSampler, scrollUV1).r;
    float noise2 = gTexture.Sample(gSampler, scrollUV2).r;
    float combinedNoise = (noise1 + noise2) * 0.5;
    
    // 形状のマスク（上が太く、下に行くほど細く、かつノイズで削れる）
    float v = input.texcoord.y;
    float verticalFade = pow(1.0 - v, 0.8);
    
    // ノイズによる形状の浸食
    float erosion = v * 0.8;
    float alphaMask = smoothstep(erosion, erosion + 0.2, combinedNoise);
    
    // カラー：根元は白く、先端はオレンジ〜赤へ
    float3 coreColor = float3(1.0, 1.0, 0.9);
    float3 midColor = float3(1.0, 0.5, 0.1);
    float3 edgeColor = float3(0.8, 0.1, 0.0);
    
    float colorStep = combinedNoise * verticalFade;
    float3 color = lerp(edgeColor, midColor, smoothstep(0.2, 0.5, colorStep));
    color = lerp(color, coreColor, smoothstep(0.6, 1.0, colorStep));
    
    // 発光強度
    float intensity = 4.0 * verticalFade;
    float finalAlpha = alphaMask * verticalFade * 0.8;
    
    return float4(color * intensity, finalAlpha);
}
