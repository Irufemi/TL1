#include "Object3d.hlsli"
#include "Lighting.hlsli"
#include "VertexData.hlsli"

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);
ConstantBuffer<LightCommonData> gLightCommonData : register(b1);

// struct VertexShaderInput は VertexData.hlsli で定義

VertexShaderOutput main(VertexInput input) {
    VertexShaderOutput output;
    
    // ワールド座標の計算
    float32_t4 worldPos = mul(input.position, gTransformationMatrix.World);
    
    // ライト視点のプロジェクション変換 (ここが重要：カメラではなくライトの行列を使う)
    output.position = mul(worldPos, gLightCommonData.viewProjection);
    
    // 影用パスでは他の属性は不要だが、構造体の整合性のために一応入れる（または PS なしなら無視される）
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(input.normal, (float32_t3x3)gTransformationMatrix.WorldInverseTranspose));
    output.worldPosition = worldPos.xyz;
    output.shadowPos = output.position; // 自身がライト空間座標
    output.color = input.color;
    
    return output;
}
