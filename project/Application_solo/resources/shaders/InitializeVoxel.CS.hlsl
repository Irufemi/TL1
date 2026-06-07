#include "VoxelParticle.hlsli"

StructuredBuffer<Voxel> gVoxels : register(t0);
RWStructuredBuffer<VoxelParticle> gParticles : register(u0);

[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	uint voxelIndex = dispatchThreadID.x;

	// バッファ範囲チェック
	uint count, stride;
	gVoxels.GetDimensions(count, stride);
	if (voxelIndex >= count)
		return;

	Voxel voxel = gVoxels[voxelIndex];
    
	gParticles[voxelIndex].position = voxel.position;
	gParticles[voxelIndex].velocity = float3(0.0f, 0.0f, 0.0f);
	gParticles[voxelIndex].color = float4(voxel.color.rgb, 0.0f); // 初期は透明にする
	gParticles[voxelIndex].life = 0.0f; // 非アクティブ
	gParticles[voxelIndex].size = 1.0f;
	gParticles[voxelIndex].isActive = 0; // 非アクティブ
	gParticles[voxelIndex].normal = voxel.normal; // 法線をコピー
}