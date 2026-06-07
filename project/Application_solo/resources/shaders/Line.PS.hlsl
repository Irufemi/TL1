#include "Line.hlsli"

// material を PS 側で参照(register を b0 に合わせる)
ConstantBuffer<Material> gMaterial : register(b0);

struct PixelShaderOutput
{
	float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
	PixelShaderOutput output;
	
	// PS 側で material を使って最終色を決定
	output.color = gMaterial.color * input.color;
	
	return output;
}