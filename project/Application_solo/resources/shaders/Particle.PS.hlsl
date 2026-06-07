#include "Particle.hlsli"
#include "Material.hlsli"

/*三角形の色を変えよう*/

ConstantBuffer<Material> gMaterial : register(b0);
struct PixelShaderOutput
{
	float32_t4 color : SV_TARGET0;
};

/*テクスチャを貼ろう*/

///Textureを使う

Texture2D<float32_t4> gTexture : register(t0); //SRVのregisterはt
SamplerState gSamplerWrap : register(s0); //Samplerのregisterはs
SamplerState gSamplerClamp : register(s1);

/*LambertianReflectance*/

struct DirectionalLight
{
	 //!< ライトの色
	float32_t4 color;
    //!< ライトの向き
	float32_t3 direction;
    //!< 輝度
	float intensity;
};
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

/*テクスチャを貼ろう*/

PixelShaderOutput main(VertexShaderOutput input)
{
	PixelShaderOutput output;
	
	/*UVTransform*/
	
	///Materialを拡張する
	
	float4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
	float32_t4 textureColor;
	if (gMaterial.useClampSampler != 0)
	{
		textureColor = gTexture.Sample(gSamplerClamp, transformedUV.xy);
	}
	else
	{
		textureColor = gTexture.Sample(gSamplerWrap, transformedUV.xy);
	}
	output.color = gMaterial.color * textureColor * input.color;
	
	/*2値抜き*/
		
	/// disxard
		
	// output.aolorのα値が0の時にPixelを棄却
	if (output.color.a == 0.0)
	{
		discard;
	}
	
	
	return output;
}