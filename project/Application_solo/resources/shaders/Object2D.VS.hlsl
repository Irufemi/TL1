// Sprite 用最小 VS(CPU側のCBレイアウトはWVP+Worldのまま維持。Worldは未使用)

#include "./Object2D.hlsli"
#include "VertexData.hlsli"

struct TransformationMatrix
{
	float32_t4x4 WVP;
	float32_t4x4 World; // レイアウト維持のため残す(未使用)
};
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

// struct VertexShaderInput は VertexData.hlsli で定義

VertexShaderOutput main(VertexInput input)
{
	VertexShaderOutput output;

    // WVP だけで投影
	output.position = mul(input.position, gTransformationMatrix.WVP);

    // UV はそのまま
	output.texcoord = input.texcoord;

    // Unlit 想定なので法線は固定(PSでは未使用)
	output.normal = float32_t3(0.0f, 0.0f, -1.0f);
	
	output.color = input.color;

	return output;
}