/*テクスチャを貼ろう*/

#include "./Object3d.hlsli"
#include "./Lighting.hlsli"

/*三角形の色を変えよう*/

ConstantBuffer<Material> gMaterial : register(b0);
struct PixelShaderOutput
{
	float32_t4 color : SV_TARGET0;
};

/*テクスチャを貼ろう*/

///Textureを使う

Texture2D<float32_t4> gTexture : register(t0); //SRVのregisterはt
SamplerState gSamplerWrap : register(s0); //Samplerのregisterはs
SamplerState gSamplerPointClamp : register(s1); // パーティクル用等POINT補間
SamplerState gSamplerClamp : register(s3); // 新規: 完全クランプ・リニア補間
SamplerState gSamplerWrapClamp : register(s4); // U:Wrap, V:Clamp (横スクロール対応等)
SamplerComparisonState gShadowSampler : register(s2); // 比較サンプラー

/*Light Common & DirectionalLight*/

ConstantBuffer<LightCommonData> gLightCommon : register(b1);

/*PhongReflectionModel*/

#include "PerFrame.hlsli"

ConstantBuffer<PerFrameData> gPerFrame : register(b2);

/*Structured Light Buffers*/

StructuredBuffer<PointLight> gPointLights : register(t2);
StructuredBuffer<SpotLight> gSpotLights : register(t3);
StructuredBuffer<AreaLight> gAreaLights : register(t4);

/*周囲の映り込み*/

/// 環境マップを追加する

TextureCube<float32_t4> gEnvironmentTexture : register(t1);
Texture2D<float32_t> gShadowMap : register(t5);

/*テクスチャを貼ろう*/

PixelShaderOutput main(VertexShaderOutput input)
{
	PixelShaderOutput output;
	
	/*UVTransform*/
	
	///Materialを拡張する
	
	float4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
	
	float32_t4 textureColor;
	if (gMaterial.useClampSampler == 1) {
		textureColor = gTexture.Sample(gSamplerClamp, transformedUV.xy);
	} else if (gMaterial.useClampSampler == 2) {
		textureColor = gTexture.Sample(gSamplerPointClamp, transformedUV.xy);
	} else if (gMaterial.useClampSampler == 3) {
		textureColor = gTexture.Sample(gSamplerWrapClamp, transformedUV.xy);
	} else {
		textureColor = gTexture.Sample(gSamplerWrap, transformedUV.xy);
	}
	
    // sRGB -> Linear はハードウェアサンプラー（_SRGB形式）に任せるため削除
    textureColor.rgb = textureColor.rgb;
    
	/*2値抜き*/
		
	/// discard
		
	// textureのα値が alphaReference 以下の時にPixelを棄却
	if (textureColor.a <= gMaterial.alphaReference)
	{
		discard;
	}
	
	///Lightingの計算を行う
	
	if (gMaterial.enableLighting != 0) //Lightingする場合
	{
		LightContext context;
		context.normal = normalize(input.normal);
		context.worldPosition = input.worldPosition;
		context.toEye = normalize(gPerFrame.cameraWorldPosition - input.worldPosition);

		float3 albedo = gMaterial.color.rgb * textureColor.rgb * input.color.rgb;
		if (gMaterial.lightingMode == 9)
		{
			float luminance = dot(albedo, float3(0.2125f, 0.7154f, 0.0721f));
			float3 gray = float3(luminance, luminance, luminance);
			albedo = lerp(gray, float3(1.0f, 1.0f, 1.0f), 0.5f);
		}
		float3 totalDiffuse = 0;
		float3 totalSpecular = 0;

		// シャドウファクターの計算
		float shadowFactor = CalculateShadow(input.shadowPos, gShadowMap, gShadowSampler, context.normal, gLightCommon.directionalLight.direction);

		// 平行光源 (影を適用)
		float3 dirDiffuse = 0;
		float3 dirSpecular = 0;
		ApplyDirectionalLight(gLightCommon.directionalLight, gMaterial, albedo, context, dirDiffuse, dirSpecular);
		totalDiffuse += dirDiffuse * shadowFactor;
		totalSpecular += dirSpecular * shadowFactor;

		// 点光源
		for (uint32_t i = 0; i < gLightCommon.pointLightCount; ++i) {
			ApplyPointLight(gPointLights[i], gMaterial, albedo, context, totalDiffuse, totalSpecular);
		}

		// スポットライト
		for (uint32_t j = 0; j < gLightCommon.spotLightCount; ++j) {
			ApplySpotLight(gSpotLights[j], gMaterial, albedo, context, totalDiffuse, totalSpecular);
		}

		// エリアライト
		for (uint32_t k = 0; k < gLightCommon.areaLightCount; ++k) {
			ApplyAreaLight(gAreaLights[k], gMaterial, albedo, context, totalDiffuse, totalSpecular);
		}

		// 拡散反射・鏡面反射の合成
		if (gMaterial.lightingMode == 0) {
			output.color.rgb = albedo;
		} else {
			// 半球ライト (Hemisphere Light) による環境光の追加
			// 直接光が当たらない部位の下側や自己遮蔽による影が完全に真っ黒になるのを防ぐ
			float3 skyColor = float3(0.35f, 0.35f, 0.4f);
			float3 groundColor = float3(0.1f, 0.1f, 0.12f);
			float hemiFactor = context.normal.y * 0.5f + 0.5f;
			float3 ambientColor = lerp(groundColor, skyColor, hemiFactor);

			output.color.rgb = totalDiffuse + totalSpecular + (albedo * ambientColor);
		}
			
		/// <summary>
		/// 環境マップ（簡易Specular IBL）の取得
		/// Roughnessが高いほどミップマップレベルを上げ、ぼけた反射にする
		/// </summary>
		float32_t3 reflectedVector = reflect(-context.toEye, context.normal);
		
		uint envWidth, envHeight, envMipLevels;
		gEnvironmentTexture.GetDimensions(0, envWidth, envHeight, envMipLevels);
		float mipLevel = gMaterial.roughness * float(envMipLevels - 1);
		
		float32_t4 environmentColor = gEnvironmentTexture.SampleLevel(gSamplerWrap, reflectedVector, mipLevel);
		// ガンマ解除はハードウェアに任せるため削除
		environmentColor.rgb = environmentColor.rgb;
		
		// フレネルによる反射率の計算 (F0)
		// 金属の場合はアルベドを、非金属の場合は 0.04 をベースにする
		float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, gMaterial.metallic);
		float3 F = FresnelSchlick(saturate(dot(context.normal, context.toEye)), F0);
		
		// 映り込みの合成 (Roughnessが高いほど反射が鈍くなる近似)
		output.color.rgb += environmentColor.rgb * F * (1.0f - gMaterial.roughness) * gMaterial.environmentCoefficient;
		
		// アルファ
		output.color.a = gMaterial.color.a * textureColor.a * input.color.a;
		
		// output.colorのα値が0の時にPixelを棄却
		if (output.color.a == 0.0)
		{
			discard;
		}
	}
	else
	{
		output.color = gMaterial.color * textureColor * input.color;
		if (gMaterial.lightingMode == 9)
		{
			float luminance = dot(output.color.rgb, float3(0.2125f, 0.7154f, 0.0721f));
			float3 gray = float3(luminance, luminance, luminance);
			output.color.rgb = lerp(gray, float3(1.0f, 1.0f, 1.0f), 0.5f);
		}
	}
	
    // Linear -> sRGB はハードウェア RTV (_SRGB形式) に任せるため削除
    output.color.rgb = output.color.rgb;

	return output;
}