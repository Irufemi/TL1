#include "Line.hlsli"
#include "VertexData.hlsli"

struct TransformationMatrix
{
	float32_t4x4 WVP;
	float32_t4x4 World; // 未使用
	float32_t4x4 WorldInverseTranspose; // 未使用
};
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

// struct VertexShaderInput は VertexData.hlsli で定義

VertexShaderOutput main(VertexInput input)
{
	VertexShaderOutput output;

    // WVP だけで投影
	output.position = mul(input.position, gTransformationMatrix.WVP);

    // VS は頂点カラーをそのままPSへ渡す。PS側で material カラーと乗算する。
	output.color = input.color;

	return output;
}