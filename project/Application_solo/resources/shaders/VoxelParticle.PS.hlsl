#include "VoxelParticle.hlsli"
#include "Lighting.hlsli"
#include "PerFrame.hlsli"

ConstantBuffer<LightCommonData> gLightCommon : register(b1);
ConstantBuffer<PerFrameData> gPerFrame : register(b2);

struct PixelShaderOutput
{
	float4 color : SV_TARGET0;
};

// 3Dハッシュ関数（ノイズ生成用）
float Hash3D(float3 p) {
    return frac(sin(dot(p, float3(12.9898, 78.233, 45.164))) * 43758.5453);
}

PixelShaderOutput main(VertexShaderOutput input)
{
	PixelShaderOutput output;

	// Alphaチャンネルには UpdateVoxel.CS.hlsl で更新された life(1.0 -> 0.0) が入っている
	float life = input.color.a;
	float noise = 0.0f;

	if (life < 1.0f) {
		// ワールド座標ベースでブロック状の高周波ノイズを生成する（砂粒感）
		noise = Hash3D(floor(input.worldPosition * 25.0f)); 
		float threshold = life * 1.5f;
		if (noise > threshold) { 
			// ピクセルを描画しない（透過・侵食）
			discard;
		}
	}

	// 最終出力の計算
	float3 finalColor = input.color.rgb;

	// ライティング計算
	LightContext context;
	context.normal = normalize(input.normal);
	context.worldPosition = input.worldPosition;
	context.toEye = normalize(gPerFrame.cameraWorldPosition - input.worldPosition);

	// VoxelParticle用の簡易マテリアル設定
	Material mat;
	mat.lightingMode = 1; // Lambert
	mat.roughness = 0.8f;
	mat.metallic = 0.0f;
	mat.color = float4(1,1,1,1);
	
	float3 diffuseColor = 0;
	float3 specularColor = 0;
	
	ApplyDirectionalLight(gLightCommon.directionalLight, mat, finalColor, context, diffuseColor, specularColor);
	
	// 環境光（半球ライトの近似）
	float3 skyColor = float3(0.35f, 0.35f, 0.4f);
	float3 groundColor = float3(0.1f, 0.1f, 0.12f);
	float hemiFactor = context.normal.y * 0.5f + 0.5f;
	float3 ambientColor = lerp(groundColor, skyColor, hemiFactor);

	// ディゾルブの溶け際（境界線）の演出
	float edgeGlow = 0.0f;
	if (life < 1.0f) {
		float threshold = life * 1.5f;
		float edge = threshold - noise;
		if (edge < 0.1f) {
			edgeGlow = 1.0f;
		}
	}

	// 通常のカラー（ライティング適用後）に、エッジの強烈な発光（HDR）を加算
	finalColor = diffuseColor + specularColor + (finalColor * ambientColor);
	
	if (edgeGlow > 0.0f) {
		// 溶け際はライティングを無視して、強烈なオレンジ（Bloomするレベル）にする
		finalColor += float3(8.0f, 2.0f, 0.0f);
	}

	// 最終出力
	// RGBのマイナス値（炭化表現用）を0にクランプしつつ出力し、ディゾルブ用にアルファは1固定で描画
	output.color = float4(max(float3(0, 0, 0), finalColor), 1.0f); 

	return output;
}