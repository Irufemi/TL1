// - αカットアウト(閾値0.5)のみ
// - ライティング関連は削除

#include "./Object2D.hlsli" // 2D用の入出力定義へと変更
#include "Material.hlsli"

ConstantBuffer<Material> gMaterial : register(b0);

struct PixelShaderOutput
{
	float32_t4 color : SV_TARGET0;
};

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

PixelShaderOutput main(VertexShaderOutput input)
{
	PixelShaderOutput output;

    // UV 変換
	float32_t4 uvw = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
	float32_t2 uv = uvw.xy;

    // hasTexture 時のみサンプル(帯域節約)
	float32_t4 texColor = (gMaterial.hasTexture != 0)
        ? gTexture.Sample(gSampler, uv)
        : float32_t4(1.0f, 1.0f, 1.0f, 1.0f);

    // ベースカラー
	float32_t4 baseColor = texColor * gMaterial.color * input.color;

    // αカットアウト：テクスチャ使用時のみ(閾値0.5)
    // → 無地半透明(hasTexture == 0)の場合は discard しない(加算/通常ブレンドが効く)
	

	output.color = baseColor;
	return output;
}