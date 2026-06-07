#include "Object3d.hlsli"
#include "VertexData.hlsli"
#include "PerFrame.hlsli"

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);
ConstantBuffer<PerFrameData> gPerFrame : register(b2);

VertexShaderOutput main(VertexInput input) {
    VertexShaderOutput output;
    
    // 頂点位置をワールド座標系に変換
    float4 worldPosition = mul(input.position, gTransformationMatrix.World);
    // ビュー・プロジェクション変換
    float4 viewPosition = mul(worldPosition, gPerFrame.view);
    output.position = mul(viewPosition, gPerFrame.projection);
    
    // ピクセルシェーダーに渡すデータ
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(input.normal, (float3x3)gTransformationMatrix.WorldInverseTranspose));
    output.worldPosition = worldPosition.xyz;
    
    // シャドウマップ用（必要なら）
    output.shadowPos = float4(0, 0, 0, 1);
    output.color = input.color;
    
    return output;
}
