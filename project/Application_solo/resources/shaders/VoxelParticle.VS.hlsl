#include "VoxelParticle.hlsli"
#include "VertexData.hlsli"

// struct VSInput は VertexData.hlsli (VertexInput) で定義

#include "PerFrame.hlsli"

// パーティクルごとのデータ
StructuredBuffer<VoxelParticle> gParticles : register(t1);

ConstantBuffer<PerFrameData> gPerFrame : register(b2);
ConstantBuffer<VoxelEmitter> gEmitter : register(b0);

VertexShaderOutput main(VertexInput input, uint instanceID : SV_InstanceID)
{
	VertexShaderOutput output;
	VoxelParticle particle = gParticles[instanceID];

	// 削除した (死んだパーティクルは PS の color.a <= 0 で discard される)
	// 初期状態(isActive==0)でも、元の形状を描画する必要があるためカリングしない。

    // ワールド行列の作成 (スケール -> 回転 -> 平行移動)
    // スケール
    float scaleVal = particle.size;
    float3 s = gEmitter.scale * scaleVal;
    
    // 回転 (XYZ軸)
    float cX = cos(particle.rotation.x);
    float sX = sin(particle.rotation.x);
    float cY = cos(particle.rotation.y);
    float sY = sin(particle.rotation.y);
    float cZ = cos(particle.rotation.z);
    float sZ = sin(particle.rotation.z);

    float3x3 rotX = { 1, 0, 0, 0, cX, -sX, 0, sX, cX };
    float3x3 rotY = { cY, 0, sY, 0, 1, 0, -sY, 0, cY };
    float3x3 rotZ = { cZ, -sZ, 0, sZ, cZ, 0, 0, 0, 1 };
    float3x3 rotateMat = mul(rotZ, mul(rotY, rotX));
    
    // 平行移動
    float4x4 worldMatrix =
    {
        s.x * rotateMat._11, s.y * rotateMat._12, s.z * rotateMat._13, 0,
        s.x * rotateMat._21, s.y * rotateMat._22, s.z * rotateMat._23, 0,
        s.x * rotateMat._31, s.y * rotateMat._32, s.z * rotateMat._33, 0,
        particle.position.x, particle.position.y, particle.position.z, 1
    };

    // 位置変換
    float4 localPos = input.position;
    float4 worldPos = mul(localPos, worldMatrix);
    
    // 非アクティブなら画面外へ飛ばす
    if (particle.isActive == 0) {
        worldPos.xyz = float3(0, -10000, 0);
    }
    
    float4 viewPos = mul(worldPos, gPerFrame.view);
    output.position = mul(viewPos, gPerFrame.projection);
    output.worldPosition = worldPos.xyz;

    // 法線変換（立方体モデルの頂点法線 input.normal を回転させる）
    output.normal = normalize(mul(input.normal, rotateMat));
    
    // UVと色
    output.texcoord = input.texcoord;
    output.color = input.color * particle.color;

	return output;
}