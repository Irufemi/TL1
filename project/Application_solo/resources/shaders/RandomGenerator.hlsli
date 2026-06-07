
// classにしているがすべてpublic扱いになる。structとclassは完全に同じ。 publicやprivate自体がサポートされていない

//class RandomGenerator
//{
//	float32_t3 seed;
//	float32_t3 Generate3d()
//	{
//		seed = rand3dTo3d(seed);
//		return seed;
//	}
	
//	float32_t Generate1d()
//	{
//		float32_t result = rand3dTo1d(seed);
//		seed.x = result;
//		return result;
//	}
//};

class RandomGenerator
{
    // 既存互換のためuint3で定義（実際の計算はxを使用）
	uint3 seed;

    // 高品質なPCGベースの1D乱数ジェネレータ
	float Generate1d()
	{
		seed.x = seed.x * 747796405u + 2891336453u;
        uint word = ((seed.x >> ((seed.x >> 28u) + 4u)) ^ seed.x) * 277803737u;
        word = (word >> 22u) ^ word;
		return float(word) / 4294967295.0f;
	}

    // 0.0f ～ 1.0f の範囲の独立した乱数を3つ(xyz)生成する
	float3 Generate3d()
	{
		return float3(Generate1d(), Generate1d(), Generate1d());
	}
};