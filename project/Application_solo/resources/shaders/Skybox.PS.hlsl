#include "skyBox.hlsli"

struct Material
{
	float32_t4 color;
	float32_t intensity;
};
ConstantBuffer<Material> gMaterial : register(b0);

struct PixelShaderOutput
{
	float32_t4 color : SV_TARGET0;
};

TextureCube<float32_t4> gTexture : register(t0); //SRVのregisterはt
SamplerState gSampler : register(s0); //Samplerのregisterはs

PixelShaderOutput main(VertexShaderOutput input)
{
	PixelShaderOutput output;
	
	float32_t4 textureColor = gTexture.Sample(gSampler, input.texcoord);
	
	output.color = textureColor * gMaterial.color * gMaterial.intensity * input.color;
	
	return output;
}