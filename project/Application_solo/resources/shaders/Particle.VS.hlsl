/*テクスチャを貼ろう*/

#include "./Particle.hlsli"
#include "VertexData.hlsli"

/*三角形を動かそう*/

struct ParticleForGPU
{
	float32_t4x4 WVP;
	
	/*LambertianReflectance*/
	
	float32_t4x4 World;
	
	float32_t4 color;
};
StructuredBuffer<ParticleForGPU> gParticle : register(t0);

// struct VertexShaderInput は VertexData.hlsli で定義

struct Camera {
	float32_t4x4 view;
	float32_t4x4 projection;
	float32_t3 worldPosition;
};
ConstantBuffer<Camera> gCamera : register(b2);

/*テクスチャを貼ろう*/

VertexShaderOutput main(VertexInput input, uint32_t instanced : SV_InstanceID)
{
	VertexShaderOutput output;
	//output.position = input.position;
	
	/*三角形を動かそう*/
	
	float32_t4 worldPos = mul(input.position, gParticle[instanced].World);
	float4 viewPos = mul(worldPos, gCamera.view);
	output.position = mul(viewPos, gCamera.projection);
	
	/*テクスチャを貼ろう*/
	
	///VertexShaderをtexcoord対応する
	
	output.texcoord = input.texcoord;
	
	
	/*LambertianReflectance*/
	
	///法線の座標系を変換してPixelShaderに送る
	
	output.color = input.color * gParticle[instanced].color;
	
	/*三角形を表示しよう*/

	return output;
}

