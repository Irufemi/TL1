#include "Fullscreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
Texture2D<float32_t> gMaskTexture : register(t1);
SamplerState gSampler : register(s0);

struct DissolveParams {
    float32_t4 edgeColor;
    float32_t4 backgroundColor; // 追加：C++側との位置合わせのため
    float32_t threshold;
    float32_t edgeRange;
    int32_t noiseType;
};

ConstantBuffer<DissolveParams> gParams : register(b0);

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;

    // マスク（ノイズ）をサンプリング
    float32_t mask = gMaskTexture.Sample(gSampler, input.texcoord);

    // 閾値以下ならピクセルを棄却
    if (mask <= gParams.threshold) {
        discard;
    }

    // メインテクスチャをサンプリング
    output.color = gTexture.Sample(gSampler, input.texcoord);

    // エッジ部分のハイライト
    // threshold ~ threshold + edgeRange の範囲を 1.0 ~ 0.0 に変換
    float32_t edge = 1.0f - smoothstep(gParams.threshold, gParams.threshold + gParams.edgeRange, mask);
    
    // エッジっぽいほど指定した色を加算（発光感）
    output.color.rgb += edge * gParams.edgeColor.rgb;

    return output;
}
