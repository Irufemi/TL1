#include "SkinningObject3D.hlsli"
#include "Lighting.hlsli"

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);
ConstantBuffer<LightCommonData> gLightCommonData : register(b1);
StructuredBuffer<Well> gMatrixPalette : register(t0);

struct Skinned {
    float32_t4 position;
    float32_t3 normal;
};

struct VertexShaderInput {
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t4 color : COLOR0;
    float32_t4 weight : WEIGHT0;
    int32_t4 index : INDEX0;
};

Skinned Skinning(VertexShaderInput input) {
    Skinned skinned;
    skinned.position = mul(input.position, gMatrixPalette[input.index.x].skeletonSpaceMatrix) * input.weight.x;
    skinned.position += mul(input.position, gMatrixPalette[input.index.y].skeletonSpaceMatrix) * input.weight.y;
    skinned.position += mul(input.position, gMatrixPalette[input.index.z].skeletonSpaceMatrix) * input.weight.z;
    skinned.position += mul(input.position, gMatrixPalette[input.index.w].skeletonSpaceMatrix) * input.weight.w;
    skinned.position.w = 1.0f;
    skinned.normal = mul(input.normal, (float32_t3x3) gMatrixPalette[input.index.x].skeletonInverseTransposeMatrix) * input.weight.x;
    skinned.normal += mul(input.normal, (float32_t3x3) gMatrixPalette[input.index.y].skeletonInverseTransposeMatrix) * input.weight.y;
    skinned.normal += mul(input.normal, (float32_t3x3) gMatrixPalette[input.index.z].skeletonInverseTransposeMatrix) * input.weight.z;
    skinned.normal += mul(input.normal, (float32_t3x3) gMatrixPalette[input.index.w].skeletonInverseTransposeMatrix) * input.weight.w;
    skinned.normal = normalize(skinned.normal);
    return skinned;
}

VertexShaderOutput main(VertexShaderInput input) {
    VertexShaderOutput output;
    Skinned skinned = Skinning(input);
    
    // ワールド座標
    float4 worldPos = mul(skinned.position, gTransformationMatrix.World);
    
    // ライト視点での座標変換
    output.position = mul(worldPos, gLightCommonData.viewProjection);
    
    output.worldPosition = worldPos.xyz;
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(skinned.normal, (float32_t3x3) gTransformationMatrix.WorldInverseTranspose));
    output.shadowPos = output.position;
    output.color = input.color;

    return output;
}
