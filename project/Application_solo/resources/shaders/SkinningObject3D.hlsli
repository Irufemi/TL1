
/*テクスチャを貼ろう*/

///Object3d/hlsliを使うようにする

struct VertexShaderOutput
{
	float32_t4 position : SV_POSITION;
	float32_t2 texcoord : TEXCOORD0;
	float32_t3 normal : NORMAL0;
	float32_t3 worldPosition : POSITION0;
	float4 shadowPos : SHADOW_POS;
	float4 color : COLOR0; // 追加
};

struct TransformationMatrix
{
	float32_t4x4 WVP;
	float32_t4x4 World;
	float32_t4x4 WorldInverseTranspose;
};

struct Well
{
	float32_t4x4 skeletonSpaceMatrix;
	float32_t4x4 skeletonInverseTransposeMatrix;
};