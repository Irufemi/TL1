
/**
 * @file LightningUtility.hlsli
 * @brief 電撃表現用ノイズ・ユーティリティ関数
 */

static const float TAU = 6.28318530718;

// 擬似乱数 (2D -> 1D)
float rand(float2 n) { 
	return frac(sin(dot(n, float2(12.9898, 4.1414))) * 43758.5453);
}

// 擬似乱数 (3D -> 1D)
float rand(float3 n) {
    return frac(sin(dot(n, float3(12.9898, 78.233, 45.164))) * 43758.5453);
}

// シンプルなノイズ (2D)
float noise(float2 p) {
	float2 ip = floor(p);
	float2 u = frac(p);
	u = u * u * (3.0 - 2.0 * u);
	
	float res = lerp(
		lerp(rand(ip), rand(ip + float2(1.0, 0.0)), u.x),
		lerp(rand(ip + float2(0.0, 1.0)), rand(ip + float2(1.0, 1.0)), u.x), u.y);
	return res * res;
}

// シンプルなノイズ (3D)
float noise(float3 p) {
    float3 ip = floor(p);
    float3 u = frac(p);
    u = u * u * (3.0 - 2.0 * u);

    float n000 = rand(ip);
    float n100 = rand(ip + float3(1.0, 0.0, 0.0));
    float n010 = rand(ip + float3(0.0, 1.0, 0.0));
    float n110 = rand(ip + float3(1.0, 1.0, 0.0));
    float n001 = rand(ip + float3(0.0, 0.0, 1.0));
    float n101 = rand(ip + float3(1.0, 0.0, 1.0));
    float n011 = rand(ip + float3(0.0, 1.0, 1.0));
    float n111 = rand(ip + float3(1.0, 1.0, 1.0));

    float res = lerp(
        lerp(lerp(n000, n100, u.x), lerp(n010, n110, u.x), u.y),
        lerp(lerp(n001, n101, u.x), lerp(n011, n111, u.x), u.y), u.z);
    return res * res;
}

// Fractal Brownian Motion (2D)
float fBm(float2 p) {
	float f = 0.0;
	float a = 0.5;
	for (int i = 0; i < 4; i++) {
		f += a * noise(p);
		p *= 2.0;
		a *= 0.5;
	}
	return f;
}

// Fractal Brownian Motion (3D)
float fBm(float3 p) {
    float f = 0.0;
    float a = 0.5;
    for (int i = 0; i < 4; i++) {
        f += a * noise(p);
        p *= 2.0;
        a *= 0.5;
    }
    return f;
}
