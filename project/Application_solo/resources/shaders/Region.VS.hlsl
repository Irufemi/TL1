// Blocks 用インスタンシング VS(Particle と同様に VS: t0 を使用)
// RootParameter[4] (VS) に SRV テーブルをバインド(t0)
// 出力は Object3d.hlsli の VertexShaderOutput に合わせる

#include "./Object3d.hlsli"
#include "./Lighting.hlsli"
#include "VertexData.hlsli"

ConstantBuffer<LightCommonData> gLightCommonData : register(b1);

struct InstanceData
{
	float32_t4x4 WVP;
	float32_t4x4 World;
	float32_t4x4 WorldInverseTranspose;
	float32_t4 color; // 未使用なら無視
};
StructuredBuffer<InstanceData> gBlocks : register(t0);

// struct VertexShaderInput は VertexData.hlsli で定義
struct Camera {
	float32_t4x4 view;
	float32_t4x4 projection;
	float32_t3 worldPosition;
};
ConstantBuffer<Camera> gCamera : register(b2);

VertexShaderOutput main(VertexInput input, uint32_t instanceId : SV_InstanceID)
{
	VertexShaderOutput output;

	InstanceData inst = gBlocks[instanceId];

    // 位置
	float32_t4 worldPos = mul(input.position, inst.World);
	float4 viewPos = mul(worldPos, gCamera.view);
	output.position = mul(viewPos, gCamera.projection);

    // UV
	output.texcoord = input.texcoord;

    // 法線(逆転置行列で変換)
	float32_t4 n4 = mul(float32_t4(input.normal, 0.0f), inst.WorldInverseTranspose);
	output.normal = normalize(n4.xyz);

    // ワールド座標(PS 側で視線方向などに使用)
	output.worldPosition = worldPos.xyz;

	// シャドウマッピング用の座標変換
	output.shadowPos = mul(worldPos, gLightCommonData.viewProjection);

	output.color = input.color * inst.color; // 頂点カラーとインスタンスカラーの乗算

	return output;
}