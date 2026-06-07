#include "Text.hlsli"
#include "Material.hlsli"

ConstantBuffer<Material> gMaterial : register(b0);

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

// 3つの値の中央値を求める関数 (MSDFの基本数式)
float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // UV Transform (通常はテクスチャ全体を使うが、アトラスから一部を切り取るUVはコンポーネント側で頂点UVとして計算する)
    float32_t4 uvw = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t2 uv = uvw.xy;

    // アトラステクスチャからRGB(MSDF)をサンプリング
    float32_t3 sampleColor = gTexture.Sample(gSampler, uv).rgb;

    // 距離場を計算し、0.5 を基準値として内外を判定
    float sd = median(sampleColor.r, sampleColor.g, sampleColor.b) - 0.5f;

    // fwidth関数を用いて、画面サイズに応じたピクセル単位の変化量を取得し、アンチエイリアス幅を最適化
    float screenPxDistance = fwidth(sd);
    
    // 不透明度（アルファ）を計算：clampで 0.0(外側) ～ 1.0(内側) の滑らかなグラデーションを作る
    // （エッジ付近のジャギを綺麗に消す効果がある）
    float opacity = clamp(sd / max(screenPxDistance, 0.0001f) + 0.5f, 0.0f, 1.0f);

    // マテリアルの色と頂点カラー(input.color)を合成
    float32_t4 baseColor = gMaterial.color * input.color;

    // 文字の色にMSDFで求めたアルファ値を乗算する
    output.color = float32_t4(baseColor.rgb, baseColor.a * opacity);

    return output;
}
