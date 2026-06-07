
/*テクスチャを貼ろう*/

#include "Object3d.hlsli"
#include "Lighting.hlsli"
#include "VertexData.hlsli"

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);
ConstantBuffer<LightCommonData> gLightCommonData : register(b1);

/*三角形を表示しよう*/

//struct VertexShaderOutput
//{
//	float32_t4 position : SV_POSITION;

//};

// struct VertexShaderInput は VertexData.hlsli にて定義

#include "PerFrame.hlsli"

ConstantBuffer<PerFrameData> gPerFrame : register(b2);

/*テクスチャを貼ろう*/

VertexShaderOutput main(VertexInput input)
{
	VertexShaderOutput output;
	//output.position = input.position;
	
	/*三角形を動かそう*/
	
	float4 worldPos = mul(input.position, gTransformationMatrix.World);
	float4 viewPos = mul(worldPos, gPerFrame.view);
	output.position = mul(viewPos, gPerFrame.projection);
	
	/*テクスチャを貼ろう*/
	
	///VertexShaderをtexcoord対応する
	
	output.texcoord = input.texcoord;
	
	
	///*LambertianReflectance*/
	
	/////法線の座標系を変換してPixelShaderに送る
	
	//output.normal = normalize(mul(input.normal, (float32_t3x3) gTransformationMatrix.World));
	
	/*非均一スケール*/
	
	/// 組み込んで使う
	
	// 法線を変換する際に逆転置行列を使う
	
	output.normal = normalize(mul(input.normal, (float32_t3x3) gTransformationMatrix.WorldInverseTranspose));
	
	/* PhongReflectionModel */
	output.worldPosition = worldPos.xyz;

	/* Shadow Mapping */
	// ワールド空間の座標をライトの視点・射影行列で変換
	output.shadowPos = mul(worldPos, gLightCommonData.viewProjection);

	output.color = input.color; // 頂点カラーを渡す

	return output;
}

