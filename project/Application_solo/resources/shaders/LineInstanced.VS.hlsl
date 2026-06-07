#include "LineInstanced.hlsli"
#include "VertexData.hlsli"

// 各インスタンスのデータ
StructuredBuffer<InstanceData> gInstanceData : register(t1);

// struct VertexShaderInput は VertexData.hlsli で定義

VertexShaderOutput main(VertexInput input, uint instanceID : SV_InstanceID)
{
	VertexShaderOutput output;
    
	InstanceData instanceData = gInstanceData[instanceID];

	output.position = mul(input.position, instanceData.WVP);
	output.color = input.color * instanceData.color;

	return output;
}