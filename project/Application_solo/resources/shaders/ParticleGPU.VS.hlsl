#include "ParticleGPU.hlsli"
#include "VertexData.hlsli"

StructuredBuffer<Particle> gParticles : register(t0);
StructuredBuffer<ParticleSortData> gSortList : register(t1);
ConstantBuffer<PerView> gPerView : register(b0);

// struct VertexShaderInput は VertexData.hlsli で定義

// 回転行列の作成 (XYZ)
float4x4 MakeRotationMatrix(float3 rotate)
{
    float3 c = cos(rotate);
    float3 s = sin(rotate);

    float4x4 mX = { 1, 0, 0, 0, 0, c.x, s.x, 0, 0, -s.x, c.x, 0, 0, 0, 0, 1 };
    float4x4 mY = { c.y, 0, -s.y, 0, 0, 1, 0, 0, s.y, 0, c.y, 0, 0, 0, 0, 1 };
    float4x4 mZ = { c.z, s.z, 0, 0, -s.z, c.z, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };

    return mul(mZ, mul(mX, mY));
}

VertexShaderOutput main(VertexInput input, uint instanceId : SV_InstanceID) 
{
	VertexShaderOutput output;
    
    ParticleSortData sortData = gSortList[instanceId];
	Particle particle = gParticles[sortData.particleIndex];
    
    // ソート時に付与したdepthが負の場合は死んでいるパーティクルなので描画しない
    if (sortData.depth < 0.0f)
    {
        particle.scale = float3(0.0f, 0.0f, 0.0f);
    }
	
    float4x4 worldMatrix;
    
    if (particle.billboardMode == 1)
    {
        // Z軸回転の行列を作成
        float c = cos(particle.rotation.z);
        float s = sin(particle.rotation.z);
        float4x4 rotZ = {
             c, s, 0, 0,
            -s, c, 0, 0,
             0, 0, 1, 0,
             0, 0, 0, 1
        };
        
        // スケール行列の作成
        float4x4 scaleMatrix = {
            particle.scale.x, 0, 0, 0,
            0, particle.scale.y, 0, 0,
            0, 0, particle.scale.z, 0,
            0, 0, 0, 1
        };

        // スケール -> Z軸回転 -> ビルボード（カメラ向き）の順に行列を合成
        worldMatrix = mul(mul(scaleMatrix, rotZ), gPerView.billboardMatrix);
    }
    else if (particle.billboardMode == 2)
    {
        // 速度方向ビルボード (Velocity Billboard)
        float3 dir = particle.velocity;
        float len = length(dir);
        if (len < 0.0001f)
        {
            dir = float3(0.0f, 1.0f, 0.0f);
        }
        else
        {
            dir /= len;
        }

        // カメラからパーティクルへの方向ベクトル
        float3 viewDir = normalize(particle.translate - gPerView.worldPosition);

        // パーティクルの右方向（進行方向と視線ベクトルの外積）
        float3 right = cross(dir, viewDir);
        float lenR = length(right);
        if (lenR < 0.0001f)
        {
            // 進行方向と視線が平行な場合は、任意の右方向を定義
            float3 upVec = abs(dir.y) < 0.999f ? float3(0,1,0) : float3(1,0,0);
            right = normalize(cross(dir, upVec));
        }
        else
        {
            right /= lenR;
        }

        // パーティクルの手前（法線）方向
        float3 normal = cross(right, dir);

        // スケール行列
        float4x4 scaleMatrix = {
            particle.scale.x, 0, 0, 0,
            0, particle.scale.y, 0, 0,
            0, 0, particle.scale.z, 0,
            0, 0, 0, 1
        };

        // 速度方向ビルボード回転行列
        // Y軸が進行方向 (dir) に整列し、X軸が右 (right) に整列し、Z軸が手前 (normal) に整列する
        float4x4 rotMatrix = {
            right.x,  right.y,  right.z,  0,
            dir.x,    dir.y,    dir.z,    0,
            normal.x, normal.y, normal.z, 0,
            0,        0,        0,        1
        };

        worldMatrix = mul(scaleMatrix, rotMatrix);
    }
    else
    {
        // 3D回転 (SRT)
        float4x4 rotateMatrix = MakeRotationMatrix(particle.rotation);
        float4x4 scaleMatrix = {
            particle.scale.x, 0, 0, 0,
            0, particle.scale.y, 0, 0,
            0, 0, particle.scale.z, 0,
            0, 0, 0, 1
        };
        worldMatrix = mul(scaleMatrix, rotateMatrix);
    }
    
	worldMatrix[3].xyz = particle.translate;
    
	output.position = mul(input.position, mul(worldMatrix, gPerView.viewProjection));
	
    // UV アニメーション (テクスチャアトラス)
    float2 uv = input.texcoord;
    uint atlasRows = (particle.atlasSize >> 16) & 0xFFFF;
    uint atlasCols = particle.atlasSize & 0xFFFF;
    uint totalFrames = max(1, atlasRows * atlasCols);
    if (totalFrames > 1)
    {
        float t = saturate(particle.currentTime / particle.lifeTime);
        uint frameIndex = (uint)(t * (float)totalFrames);
        frameIndex = min(frameIndex, totalFrames - 1);
        
        uint row = frameIndex / max(1, atlasCols);
        uint col = frameIndex % max(1, atlasCols);
        
        float2 frameSize = 1.0f / float2(max(1, atlasCols), max(1, atlasRows));
        uv = (uv + float2(col, row)) * frameSize;
    }
    output.texcoord = float4(uv, particle.translate.xy);
    output.timeRatio = saturate(particle.currentTime / max(particle.lifeTime, 0.0001f));
	output.color = input.color * particle.color;
	return output;
}