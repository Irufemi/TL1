#include "ParticleGPU.hlsli"

RWStructuredBuffer<ParticleSortData> gSortList : register(u3);

cbuffer SortConstants : register(b2)
{
    uint k;
    uint j;
};

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint i = DTid.x;
    uint ixj = i ^ j;
    
    // スレッドIDがソート対象ペアの「前側」である場合のみ処理
    if (ixj > i)
    {
        ParticleSortData a = gSortList[i];
        ParticleSortData b = gSortList[ixj];
        
        bool swap = false;
        if ((i & k) == 0)
        {
            // 昇順ブロック（全体の降順化に向けて、ここでは要素ペアを降順に揃える）
            // 奥にあるパーティクル(depth大)を前に持ってくるため、降順ソート
            if (a.depth < b.depth) swap = true;
        }
        else
        {
            // 降順ブロック（ここでは要素ペアを昇順に揃える）
            if (a.depth > b.depth) swap = true;
        }
        
        if (swap)
        {
            gSortList[i] = b;
            gSortList[ixj] = a;
        }
    }
}
