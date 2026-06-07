#include "ParticleGPU.hlsli"

static const uint kMaxParticles = 32768;

StructuredBuffer<Particle> gParticles : register(t0);
RWStructuredBuffer<ParticleSortData> gSortList : register(u3);
ConstantBuffer<PerView> gPerView : register(b0);

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint particleIndex = DTid.x;
    if (particleIndex >= kMaxParticles) return;
    
    ParticleSortData sortData;
    sortData.particleIndex = particleIndex;
    
    Particle p = gParticles[particleIndex];
    if (p.color.a > 0.0f && p.currentTime < p.lifeTime)
    {
        // 距離の2乗でもソートは可能ですが、他のオブジェクトとの兼ね合いや
        // 精度低下を避けるためlengthを使用します。
        // 奥にあるパーティクルほどdepthが大きくなります。
        sortData.depth = length(p.translate - gPerView.worldPosition);
    }
    else
    {
        // 死んでいるパーティクルはソート後に配列の末尾（後方）に追いやるため、負の値を設定します。
        // ビトニックソートは降順(大きい順)でソートするため、-1.0f は最後尾になります。
        sortData.depth = -1.0f;
    }
    
    gSortList[particleIndex] = sortData;
}
