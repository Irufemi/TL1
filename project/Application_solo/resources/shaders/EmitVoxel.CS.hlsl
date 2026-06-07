#include "VoxelParticle.hlsli"
#include "RandomGenerator.hlsli"
#include "PerFrame.hlsli"

StructuredBuffer<Voxel> gVoxels : register(t0);
RWStructuredBuffer<VoxelParticle> gParticles : register(u0);
ConstantBuffer<VoxelEmitter> gEmitter : register(b0);
ConstantBuffer<PerFrame> gPerFrame : register(b1);

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
    
	RandomGenerator generator;
    // 乱数のシードを時間とインデックスで初期化
	generator.seed = voxelIndex * 12345 + uint3(1, 7, 11) * (uint) (gPerFrame.time * 1000.0f);

    // 1. スケーリング
	float3 localPos = voxel.position * gEmitter.scale;
    
    // 2. 回転 (XYZの順で回転行列を作成して適用)
	float cX = cos(gEmitter.rotate.x);
	float sX = sin(gEmitter.rotate.x);
	float cY = cos(gEmitter.rotate.y);
	float sY = sin(gEmitter.rotate.y);
	float cZ = cos(gEmitter.rotate.z);
	float sZ = sin(gEmitter.rotate.z);

	float3x3 rotX = { 1, 0, 0, 0, cX, -sX, 0, sX, cX };
	float3x3 rotY = { cY, 0, sY, 0, 1, 0, -sY, 0, cY };
	float3x3 rotZ = { cZ, -sZ, 0, sZ, cZ, 0, 0, 0, 1 };
	float3x3 rotateMat = mul(rotZ, mul(rotY, rotX));
    
	localPos = mul(rotateMat, localPos);
    
    // 3. 初期位置: エミッター位置 + 回転・スケール適用後のボクセル相対位置
	float3 worldPos = gEmitter.emitPosition + localPos;
	gParticles[voxelIndex].position = worldPos;

	// OBB判定 (衝突領域内かチェック)
	if (gEmitter.useCollision != 0)
	{
		float3 d = worldPos - gEmitter.collisionCenter;
		bool inside = true;
		[unroll]
		for (int i = 0; i < 3; ++i)
		{
			float dist = dot(d, gEmitter.collisionOrientations[i].xyz);
			if (abs(dist) > gEmitter.collisionSize[i])
			{
				inside = false;
				break;
			}
		}

		if (!inside)
		{
			// 既に飛散中の他のボクセルを消さないように、ここでは単にスキップするだけにする
			return;
		}
	}

	// 既にアクティブな場合、無理に再配置すると「飛んでいる途中で元の位置にワープ」して不自然になるためスキップ
	// ただし、使い回すために寿命が尽きている（または尽きかけている）場合は再エミットを許可する
	if (gEmitter.useCollision != 0 && gParticles[voxelIndex].isActive == 1 && gParticles[voxelIndex].life > 0.2f)
	{
		return;
	}
    
    // 4. 初期速度: ボクセルの法線を回転させ、衝突時は中心から外側へ向かうベクトルを加味する
	float3 rotatedNormal = normalize(mul(rotateMat, voxel.normal));
	float3 burstDir = (gEmitter.useCollision != 0) ? normalize(worldPos - gEmitter.collisionCenter) : rotatedNormal;
	
	float3 randomVec = (generator.Generate3d() * 2.0f - 1.0f) * 0.5f; // -0.5 ~ 0.5
    float3 finalVelocity = float3(0,0,0);

    if (gEmitter.particleType == 1 && gEmitter.useCollision != 0)
    {
        // ビル近接攻撃などの衝突飛散時：均一に吹き飛ぶ不自然さを解消
        float speedVariance = generator.Generate1d() * 0.6f + 0.4f; // 0.4 ~ 1.0 の速度ブレ
        
        float hitSpeed = length(gEmitter.baseVelocity);
        float3 hitDir = (hitSpeed > 0.001f) ? normalize(gEmitter.baseVelocity) : float3(0, 1, 0);
        
        // 打撃の進行方向(hitDir) と 衝突中心から外への方向(burstDir) をブレンドし、ランダム成分を足す
        float3 scatterDir = normalize(lerp(burstDir, hitDir, 0.4f) + randomVec);
        
        // 上に向かって破片が散るように Y 成分を少し底上げ
        scatterDir.y += abs(randomVec.y) * 0.8f + 0.2f;
        scatterDir = normalize(scatterDir);
        
        // 打撃の強さ(hitSpeed)を一部利用しつつ、バラつき(speedVariance)を与えることで塊で飛ぶのを防ぐ
        finalVelocity = scatterDir * (hitSpeed * 0.5f + gEmitter.dispersion) * speedVariance;
    }
    else
    {
        // 既存ロジック
	    float3 moveDir = normalize(lerp(rotatedNormal, burstDir, 0.7f) + randomVec);
	    finalVelocity = gEmitter.baseVelocity + moveDir * gEmitter.dispersion;
    }

	gParticles[voxelIndex].color = voxel.color;
	
	float delay = 0.0f;
	if (gEmitter.particleType == 2) // AshDisintegration
	{
		// 下から崩れるように、ローカルのY座標に応じたディレイを計算
		float noise = (generator.Generate1d() * 2.0f - 1.0f) * 0.2f;
		delay = max(0.0f, localPos.y * 0.05f + noise);
	}
	else if (gEmitter.particleType == 4) { // DebrisLargeGravity
		// エネミー破壊：真下に重く落ちる（横への広がりを抑え、下向きベクトルを付与）
		float3 randVec = generator.Generate3d() * 2.0f - 1.0f;
		finalVelocity = float3(randVec.x * 0.2f, -1.0f - randVec.y, randVec.z * 0.2f) * length(gEmitter.baseVelocity);
	}
	else if (gEmitter.particleType == 5) { // DebrisExplosive
		// プレイヤー寿命：空中で放射状に鋭く四散しつつ、元の吹き飛び方向（baseVelocity）の勢いも引き継ぐ
		float3 randDir = normalize(generator.Generate3d() * 2.0f - 1.0f);
		// baseVelocity（吹き飛びの速度）をベースにしつつ、全方向への散らばりを加える
		finalVelocity = gEmitter.baseVelocity * 0.7f + randDir * (length(gEmitter.baseVelocity) * 0.3f + gEmitter.dispersion * 3.0f);
	}
	else if (gEmitter.particleType == 3) // FineScatter (被弾時)
	{
		// 攻撃方向（baseVelocity）やburstDirが物体内部（法線と逆）に向かっている場合、
		// 巨大なモデルだと内部にめり込んで見えなくなるため、外側に反射（バウンス）させる
		if (dot(finalVelocity, rotatedNormal) < 0.0f)
		{
			finalVelocity = reflect(finalVelocity, rotatedNormal);
			// 確実に外へ飛び出すように少し押し出す
			finalVelocity += rotatedNormal * (gEmitter.dispersion * 0.5f);
		}

		// 元の色をベースにしつつ、衝撃の熱で強烈に発光させる (HDR)
		float hitNoise = generator.Generate1d();
		float3 sparkColor = lerp(float3(8.0f, 4.0f, 1.0f), float3(20.0f, 15.0f, 5.0f), hitNoise);
		gParticles[voxelIndex].color.rgb = voxel.color.rgb * sparkColor;
	}
	
	gParticles[voxelIndex].velocity = finalVelocity;
	gParticles[voxelIndex].life = 1.0f + delay; // 寿命を満タンにする＋ディレイを加算
	gParticles[voxelIndex].size = 1.0f; // サイズ
	gParticles[voxelIndex].isActive = 1; // アクティブ化
	gParticles[voxelIndex].normal = rotatedNormal; // 回転後の法線をコピー

    // 5. 初期回転と角速度の付与
    // Voxel自身の現在の回転を初期値とする
    gParticles[voxelIndex].rotation = gEmitter.rotate; 
    
    // 角速度（スピン）をランダムに設定
    float3 randomSpin = (generator.Generate3d() * 2.0f - 1.0f) * 15.0f; // -15.0 ~ 15.0 rad/s (かなり高速に回転させる)
    if (gEmitter.particleType == 3) { // FineScatterの場合はさらに速く
        randomSpin *= 1.5f;
    }
    gParticles[voxelIndex].angularVelocity = randomSpin;
}