#include "ParticleGPU.hlsli"
#include "RandomGenerator.hlsli"
#include "PerFrame.hlsli"

static const uint kMaxParticles = 32768;

RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int> gFreeListIndex : register(u1);
RWStructuredBuffer<int> gFreeList : register(u2);
StructuredBuffer<GPUParticleEmitter> gEmitters : register(t0);
ConstantBuffer<PerFrame> gPerFrame : register(b1);

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint particleIndex = DTid.x;
    if (particleIndex < kMaxParticles)
    {
        if (gParticles[particleIndex].currentTime < gParticles[particleIndex].lifeTime)
        {
            GPUParticleEmitter emitter = gEmitters[gParticles[particleIndex].emitterIndex];
            float dt = gPerFrame.deltaTime;
            
            // 座標のゆらぎ (Jitter)
            if (emitter.jitter > 0.0f) {
                RandomGenerator rng;
                rng.seed = uint3(particleIndex, (uint)gPerFrame.time, (uint)gPerFrame.time + 100);
                float3 randomVal = rng.Generate3d() * 2.0f - 1.0f;
                gParticles[particleIndex].translate += randomVal * emitter.jitter;
            }

            // 進捗 (0.0: 生まれたて, 1.0: 寿命)
            float t = saturate(gParticles[particleIndex].currentTime / gParticles[particleIndex].lifeTime);
            
            // === 親パーティクルの更新と子生成 ===
            if (gParticles[particleIndex].type == 0) {
                // アトラクター (引力)
                if (abs(emitter.attractorStrength) > 0.0001f) {
                    float3 dir = emitter.attractorPos - gParticles[particleIndex].translate;
                    float distSq = max(dot(dir, dir), 0.01f);
                    float3 force = normalize(dir) * (emitter.attractorStrength / distSq);
                    gParticles[particleIndex].velocity += force * dt;
                }

                // 物理更新: 重力と空気抵抗
                gParticles[particleIndex].velocity.y -= emitter.gravity * dt;
                gParticles[particleIndex].velocity *= pow(saturate(1.0f - emitter.damping), dt * 60.0f);

                // Trail放出判定
                if (emitter.enableTrail != 0) {
                    gParticles[particleIndex].trailTimer += dt;
                    if (gParticles[particleIndex].trailTimer > emitter.trailFrequency) {
                        gParticles[particleIndex].trailTimer -= emitter.trailFrequency;
                        
                        // 子（Trail / Flame）をEmit
                        int freeListIndex;
                        InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);
                        if (freeListIndex >= 0 && freeListIndex < (int)kMaxParticles) {
                            uint childIndex = (uint)gFreeList[freeListIndex];
                            
                            gParticles[childIndex].type = 1; // Trail
                            gParticles[childIndex].translate = gParticles[particleIndex].translate;
                            gParticles[childIndex].emitterIndex = gParticles[particleIndex].emitterIndex;
                            gParticles[childIndex].billboardMode = gParticles[particleIndex].billboardMode;
                            gParticles[childIndex].atlasSize = gParticles[particleIndex].atlasSize;
                            
                            RandomGenerator rng;
                            rng.seed = uint3(particleIndex, childIndex, (uint)gPerFrame.time);
                            float3 rDir = rng.Generate3d() * 2.0f - 1.0f;
                            // 親の速度を少し引き継ぎつつランダムに散らす
                            gParticles[childIndex].velocity = gParticles[particleIndex].velocity * 0.1f + rDir * 0.02f;
                            
                            gParticles[childIndex].currentTime = 0.0f;
                            gParticles[childIndex].lifeTime = 0.3f + rng.Generate1d() * 0.3f; // 0.3~0.6s
                            
                            // チームメイトのFlameパラメータ相当
                            gParticles[childIndex].startScale = float3(0.08f, 0.08f, 0.08f);
                            gParticles[childIndex].endScale = float3(0.0f, 0.0f, 0.0f);
                            gParticles[childIndex].scale = gParticles[childIndex].startScale;
                            
                            gParticles[childIndex].startColor = float4(1.0f, 0.6f, 0.0f, 1.0f);
                            gParticles[childIndex].endColor = float4(0.8f, 0.2f, 0.0f, 0.0f);
                            gParticles[childIndex].color = gParticles[childIndex].startColor;
                            
                            gParticles[childIndex].rotation = float3(0,0,0);
                            gParticles[childIndex].rotateSpeed = float3(0,0,0);
                        } else {
                            InterlockedAdd(gFreeListIndex[0], 1); // 戻す
                        }
                    }
                }
            } 
            // === Trail(Flame) の更新 ===
            else if (gParticles[particleIndex].type == 1) {
                // 親の重力の半分程度で落ちる
                gParticles[particleIndex].velocity.y -= (emitter.gravity * 0.5f) * dt;
                // 空気抵抗でフワッとさせる
                gParticles[particleIndex].velocity *= pow(saturate(1.0f - 0.1f), dt * 60.0f);
            }
            // === Death(Sparkle) の更新 ===
            else if (gParticles[particleIndex].type == 2) {
                // 少しだけ重力の影響を受ける
                gParticles[particleIndex].velocity.y -= (emitter.gravity * 0.2f) * dt;
                // 小さく弾けてすぐ減速
                gParticles[particleIndex].velocity *= pow(saturate(1.0f - 0.2f), dt * 60.0f);
            }

            // 共通の移動と時間更新
            gParticles[particleIndex].translate += gParticles[particleIndex].velocity;
            gParticles[particleIndex].currentTime += dt;
            gParticles[particleIndex].rotation += gParticles[particleIndex].rotateSpeed * dt;
            
            // 床衝突判定 (共通)
            if (gParticles[particleIndex].translate.y < emitter.groundHeight) {
                gParticles[particleIndex].translate.y = emitter.groundHeight;
                gParticles[particleIndex].velocity.y *= -emitter.bounce;
                gParticles[particleIndex].velocity.xz *= 0.8f;
            }

            // カラー・スケール更新: Start -> Mid -> End Lerp (type == 0 以外は初期設定をそのままLerp)
            if (gParticles[particleIndex].type == 0) {
                if (gParticles[particleIndex].midPoint > 0.0f) {
                    if (t < gParticles[particleIndex].midPoint) {
                        float progress = t / gParticles[particleIndex].midPoint;
                        gParticles[particleIndex].color = lerp(gParticles[particleIndex].startColor, gParticles[particleIndex].midColor, progress);
                        gParticles[particleIndex].scale = lerp(gParticles[particleIndex].startScale, gParticles[particleIndex].midScale, progress);
                    } else {
                        float progress = (t - gParticles[particleIndex].midPoint) / (1.0f - gParticles[particleIndex].midPoint);
                        gParticles[particleIndex].color = lerp(gParticles[particleIndex].midColor, gParticles[particleIndex].endColor, progress);
                        gParticles[particleIndex].scale = lerp(gParticles[particleIndex].midScale, gParticles[particleIndex].endScale, progress);
                    }
                } else {
                    gParticles[particleIndex].color = lerp(gParticles[particleIndex].startColor, gParticles[particleIndex].endColor, t);
                    gParticles[particleIndex].scale = lerp(gParticles[particleIndex].startScale, gParticles[particleIndex].endScale, t);
                }
            } else {
                gParticles[particleIndex].color = lerp(gParticles[particleIndex].startColor, gParticles[particleIndex].endColor, t);
                gParticles[particleIndex].scale = lerp(gParticles[particleIndex].startScale, gParticles[particleIndex].endScale, t);
            }

            // 寿命終了判定
            if (gParticles[particleIndex].currentTime >= gParticles[particleIndex].lifeTime)
            {
                // Death Emit 判定 (親パーティクルの場合のみ)
                if (gParticles[particleIndex].type == 0 && emitter.enableDeathEmit != 0) {
                    RandomGenerator rng;
                    rng.seed = uint3(particleIndex, (uint)gPerFrame.time, 777);
                    
                    // 数個のSparkleを発生 (3〜5個)
                    int numSparkles = 3 + (int)(rng.Generate1d() * 3.0f);
                    for (int s = 0; s < numSparkles; ++s) {
                        int freeListIndex;
                        InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);
                        if (freeListIndex >= 0 && freeListIndex < (int)kMaxParticles) {
                            uint childIndex = (uint)gFreeList[freeListIndex];
                            
                            gParticles[childIndex].type = 2; // Sparkle
                            gParticles[childIndex].translate = gParticles[particleIndex].translate;
                            gParticles[childIndex].emitterIndex = gParticles[particleIndex].emitterIndex;
                            gParticles[childIndex].billboardMode = gParticles[particleIndex].billboardMode;
                            gParticles[childIndex].atlasSize = gParticles[particleIndex].atlasSize;
                            
                            float3 rDir = rng.Generate3d() * 2.0f - 1.0f;
                            gParticles[childIndex].velocity = normalize(rDir) * (0.2f + rng.Generate1d() * 0.1f); // パチッと弾ける初速
                            
                            gParticles[childIndex].currentTime = 0.0f;
                            gParticles[childIndex].lifeTime = 0.1f + rng.Generate1d() * 0.1f; // 0.1~0.2s
                            
                            gParticles[childIndex].startScale = float3(0.08f, 0.08f, 0.08f);
                            gParticles[childIndex].endScale = float3(0.0f, 0.0f, 0.0f);
                            gParticles[childIndex].scale = gParticles[childIndex].startScale;
                            
                            gParticles[childIndex].startColor = float4(1.0f, 0.9f, 0.5f, 1.0f);
                            gParticles[childIndex].endColor = float4(1.0f, 0.5f, 0.0f, 0.0f);
                            gParticles[childIndex].color = gParticles[childIndex].startColor;
                            
                            gParticles[childIndex].rotation = float3(0,0,0);
                            gParticles[childIndex].rotateSpeed = float3(0,0,0);
                        } else {
                            InterlockedAdd(gFreeListIndex[0], 1);
                            break;
                        }
                    }
                }

                // 自身を破棄
                gParticles[particleIndex].scale = float3(0.0f, 0.0f, 0.0f);
                gParticles[particleIndex].color.a = 0.0f;

                int freeListIndex;
                InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);
                if ((freeListIndex + 1) >= 0 && (freeListIndex + 1) < (int)kMaxParticles)
                {
                    gFreeList[freeListIndex + 1] = (int)particleIndex;
                }
                else
                {
                    InterlockedAdd(gFreeListIndex[0], -1);
                }
            }
        }
    }
}