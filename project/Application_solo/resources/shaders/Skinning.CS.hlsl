#include "SkinningObject3D.hlsli"

/*GPUの有効利用*/

/// 必要なResourceの定義

#include "VertexData.hlsli"

struct VertexInfluence
{
	float32_t4 weight;
	int32_t4 index;
};
struct SkinningInformation
{
	uint32_t numVertices;
};

// SkinningObject3d.VS.hlslで作ったものと同じPalette
StructuredBuffer<Well> gMatrixPalette : register(t0);
// VertexBufferViewのstream)として利用していた入力頂点
StructuredBuffer<VertexInput> gInputVertices : register(t1);
// VertexBufferViewのstream1として利用していた入力インフルエンス
StructuredBuffer<VertexInfluence> gInfluences : register(t2);
// Skinning計算後の頂点データ。SkinnedVertex
RWStructuredBuffer<VertexInput> gOutputVertices : register(u0);
// Skinningに関するちょっとした情報
ConstantBuffer<SkinningInformation> gSkinningInformation : register(b0);

/// ComputeShaderを作成する

[numthreads(256, 1, 1)] // ←一度にComputeShader内のthreadを起動する数。
void main(uint32_t3 DTid : SV_DispatchThreadID)
{
	
	/// 不必要に処理しないようにする
	uint32_t vertexIndex = DTid.x;
	if (vertexIndex < gSkinningInformation.numVertices)
	{
		// Skinning計算
		
		/// Skinning処理をする
		
		// 必要なデータをStructureBufferから取ってくる。
		// SkinningObject3D.VSでは入力頂点として受け取っていた
		VertexInput input = gInputVertices[vertexIndex];
		VertexInfluence influence = gInfluences[vertexIndex];
		
		// skinning後の頂点を計算
		VertexInput skinned;
		skinned.texcoord = input.texcoord;
		skinned.color = input.color; // 頂点カラーはそのままスルー
		
		// 計算の方法はSkinningObject3D.VSと同じ
		// データの取得方法が変わるだけで、原理が変わるわけではない
		// 位置の変換
		skinned.position = mul(input.position, gMatrixPalette[influence.index.x].skeletonSpaceMatrix) * influence.weight.x;
		skinned.position += mul(input.position, gMatrixPalette[influence.index.y].skeletonSpaceMatrix) * influence.weight.y;
		skinned.position += mul(input.position, gMatrixPalette[influence.index.z].skeletonSpaceMatrix) * influence.weight.z;
		skinned.position += mul(input.position, gMatrixPalette[influence.index.w].skeletonSpaceMatrix) * influence.weight.w;
		skinned.position.w = 1.0f; // 確実に1を入れる
		// 法線の変換
		skinned.normal = mul(input.normal, (float32_t3x3) gMatrixPalette[influence.index.x].skeletonInverseTransposeMatrix) * influence.weight.x;
		skinned.normal += mul(input.normal, (float32_t3x3) gMatrixPalette[influence.index.y].skeletonInverseTransposeMatrix) * influence.weight.y;
		skinned.normal += mul(input.normal, (float32_t3x3) gMatrixPalette[influence.index.z].skeletonInverseTransposeMatrix) * influence.weight.z;
		skinned.normal += mul(input.normal, (float32_t3x3) gMatrixPalette[influence.index.w].skeletonInverseTransposeMatrix) * influence.weight.w;
		skinned.normal = normalize(skinned.normal); // 正規化して戻してあげる
		
		// Skinning後の頂点データを格納。つまり書き込む。
		gOutputVertices[vertexIndex] = skinned;
	
	}
}