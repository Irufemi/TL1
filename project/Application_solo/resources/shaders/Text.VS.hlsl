#include "Text.hlsli"
#include "VertexData.hlsli"

struct TransformationMatrix
{
    float32_t4x4 WVP;
    float32_t4x4 World;
};
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

VertexShaderOutput main(VertexInput input)
{
    VertexShaderOutput output;
    
    // WVP行列による座標変換
    output.position = mul(input.position, gTransformationMatrix.WVP);
    
    // UVと頂点カラーをパススルー
    output.texcoord = input.texcoord;
    output.color = input.color;
    
    return output;
}
