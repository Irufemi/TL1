#include "./Material.hlsli"

// --- 構造体定義 ---

struct DirectionalLight {
    float32_t4 color;
    float32_t3 direction;
    float32_t intensity;
};

struct LightCommonData {
    DirectionalLight directionalLight;
    float32_t4x4 viewProjection;
    uint32_t pointLightCount;
    uint32_t spotLightCount;
    uint32_t areaLightCount;
    uint32_t padding;
};

struct PointLight {
    float32_t4 color;
    float32_t3 position;
    float32_t intensity;
    float32_t radius;
    float32_t decay;
    int32_t isActive;
    float32_t padding;
};

struct SpotLight {
    float32_t4 color;
    float32_t3 position;
    float32_t intensity;
    float32_t3 direction;
    float32_t distance;
    float32_t decay;
    float32_t cosAngle;
    float32_t falloff;
    int32_t isActive;
    float32_t4 padding; // 80バイト合わせ
};

struct AreaLight {
    float32_t4 color;
    float32_t3 position;
    float32_t intensity;
    float32_t3 direction;
    float32_t range;
    float32_t2 size;
    int32_t isActive;
    float32_t padding; // 64バイト合わせ
};

// --- 計算用コンテキスト ---

struct LightContext {
    float3 normal;
    float3 worldPosition;
    float3 toEye;
};

// --- ライティング計算関数 ---

static const float32_t PI = 3.14159265f;

/**
 * シャドウファクターを計算する (0.0: 影, 1.0: 光)
 */
float CalculateShadow(float4 shadowPos, Texture2D<float> shadowMap, SamplerComparisonState shadowSampler, float3 normal, float3 lightDir) {
    // 同次座標系からUV座標系に変換 (-1~1 -> 0~1)
    float3 projectedPos = shadowPos.xyz / shadowPos.w;
    float2 uv = projectedPos.xy * float2(0.5, -0.5) + float2(0.5, 0.5);
    float currentDepth = projectedPos.z;

    // 範囲外は影にしない
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        return 1.0;
    }

    // 遠すぎる場合は影にしない (Orthographic の Z 範囲外)
    if (currentDepth < 0.0 || currentDepth > 1.0) {
        return 1.0;
    }

    // スロープスケールバイアス (シャドウアクネ対策)
    // 面がライトに対して傾いているほどバイアスを大きくする
    float bias = max(0.005 * (1.0 - dot(normal, -lightDir)), 0.0005);

    // 3x3 PCF (Percentage Closer Filtering) によるソフトシャドウ
    float shadowFactor = 0.0;
    const float2 texelSize = 1.0 / 2048.0; // シャドウマップのテクセルサイズ

    [unroll]
    for (float y = -1.0; y <= 1.0; y += 1.0) {
        [unroll]
        for (float x = -1.0; x <= 1.0; x += 1.0) {
            float2 offset = float2(x, y) * texelSize;
            shadowFactor += shadowMap.SampleCmpLevelZero(shadowSampler, uv + offset, currentDepth - bias);
        }
    }
    shadowFactor /= 9.0;

    // 影を完全に真っ暗にせず、少し明るくする (0.5 ~ 1.0 にマップ)
    return 0.5 + shadowFactor * 0.5;
}

/**
 * 拡散反射強度を計算する (Lambert / Half-Lambert)
 */
float CalculateDiffuseFactor(float3 normal, float3 lightDir, int mode) {
    float NdotL = dot(normal, -lightDir);
    if (mode == 1) { // Lambert
        return saturate(NdotL);
    } else if (mode == 2) { // Half-Lambert
        return pow(NdotL * 0.5f + 0.5f, 2.0f);
    }
    return 1.0f;
}

/**
 * 鏡面反射強度を計算する (Blinn-Phong)
 */
float CalculateSpecularFactor(float3 normal, float3 lightDir, float3 toEye, float roughness) {
    float3 halfVector = normalize(-lightDir + toEye);
    float NdotH = dot(normal, halfVector);
    float shininess = (1.0f - roughness) * 100.0f; // 簡易変換
    return pow(saturate(NdotH), shininess);
}

// --- PBR関数 (Cook-Torrance BRDF) ---

/**
 * 法線分布関数 (Trowbridge-Reitz GGX)
 */
float DistributionGGX(float3 N, float3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = saturate(dot(N, H));
    float NdotH2 = NdotH * NdotH;
    
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    
    return num / max(denom, 0.0000001);
}

/**
 * 幾何減衰関数 (Smith-Schlick GGX)
 */
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    
    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    
    return num / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness) {
    float NdotV = saturate(dot(N, V));
    float NdotL = saturate(dot(N, -L));
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    
    return ggx1 * ggx2;
}

/**
 * フレネル反射率 (Schlick's approximation)
 */
float3 FresnelSchlick(float cosTheta, float3 F0) {
    return F0 + (1.0 - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}

/**
 * PBR ライティング計算 (Cook-Torrance BRDF)
 */
void CalculatePBR(LightContext context, float3 lightDir, float3 lightColor, float intensity, Material material, float3 albedo, inout float3 diffuseColor, inout float3 specularColor) {
    float3 N = context.normal;
    float3 V = context.toEye;
    float3 L = -lightDir;
    float3 H = normalize(V + L);
    
    // F0 (垂直入射時の反射率) 
    // 非金属は 0.04 固定、金属はアルベド（テクスチャ * ベースカラー）を使用
    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, albedo, material.metallic);
    
    // Cook-Torrance BRDF
    float D = DistributionGGX(N, H, material.roughness);
    float G = GeometrySmith(N, V, lightDir, material.roughness);
    float3 F = FresnelSchlick(saturate(dot(H, V)), F0);
    
    float3 numerator = D * G * F;
    float denominator = 4.0 * saturate(dot(N, V)) * saturate(dot(N, L)) + 0.0001;
    float3 specular = numerator / denominator;
    
    float3 kS = F;
    float3 kD = (float3(1.0, 1.0, 1.0) - kS) * (1.0 - material.metallic);
    
    float NdotL = saturate(dot(N, L));
    float3 radiance = lightColor * intensity;
    
    diffuseColor += kD * albedo / PI * radiance * NdotL;
    specularColor += specular * radiance * NdotL;
}

/**
 * 平行光源の計算
 */
void ApplyDirectionalLight(DirectionalLight light, Material material, float3 albedo, LightContext context, inout float3 diffuseColor, inout float3 specularColor) {
    if (material.lightingMode == 3) { // PBR
        CalculatePBR(context, light.direction, light.color.rgb, light.intensity, material, albedo, diffuseColor, specularColor);
    } else {
        float diffuseFactor = CalculateDiffuseFactor(context.normal, light.direction, material.lightingMode);
        float specularFactor = CalculateSpecularFactor(context.normal, light.direction, context.toEye, material.roughness);
        
        diffuseColor += albedo * light.color.rgb * light.intensity * diffuseFactor;
        specularColor += light.color.rgb * light.intensity * specularFactor;
    }
}

/**
 * 点光源の計算
 */
void ApplyPointLight(PointLight light, Material material, float3 albedo, LightContext context, inout float3 diffuseColor, inout float3 specularColor) {
    if (light.isActive == 0) return;

    float3 lightDir = normalize(context.worldPosition - light.position);
    float d = length(context.worldPosition - light.position);
    float attenuation = saturate(1.0f - d / max(light.radius, 0.0001f));
    attenuation = pow(attenuation, light.decay);

    if (material.lightingMode == 3) { // PBR
        CalculatePBR(context, lightDir, light.color.rgb, light.intensity * attenuation, material, albedo, diffuseColor, specularColor);
    } else {
        float diffuseFactor = CalculateDiffuseFactor(context.normal, lightDir, material.lightingMode);
        float specularFactor = CalculateSpecularFactor(context.normal, lightDir, context.toEye, material.roughness);

        diffuseColor += albedo * light.color.rgb * light.intensity * diffuseFactor * attenuation;
        specularColor += light.color.rgb * light.intensity * specularFactor * attenuation;
    }
}

/**
 * スポットライトの計算
 */
void ApplySpotLight(SpotLight light, Material material, float3 albedo, LightContext context, inout float3 diffuseColor, inout float3 specularColor) {
    if (light.isActive == 0) return;

    float3 lightDir = normalize(context.worldPosition - light.position);
    float d = length(context.worldPosition - light.position);
    
    float attenuation = pow(saturate(1.0f - d / max(light.distance, 1e-5f)), light.decay);
    
    float cosAngle = dot(lightDir, light.direction);
    float falloff = saturate((cosAngle - light.cosAngle) / (1.0f - light.cosAngle));
    
    float factor = attenuation * falloff;

    if (material.lightingMode == 3) { // PBR
        CalculatePBR(context, lightDir, light.color.rgb, light.intensity * factor, material, albedo, diffuseColor, specularColor);
    } else {
        float diffuseFactor = CalculateDiffuseFactor(context.normal, lightDir, material.lightingMode);
        float specularFactor = CalculateSpecularFactor(context.normal, lightDir, context.toEye, material.roughness);

        diffuseColor += albedo * light.color.rgb * light.intensity * diffuseFactor * factor;
        specularColor += light.color.rgb * light.intensity * specularFactor * factor;
    }
}

/**
 * エリアライトの計算 (代表点近似)
 */
void ApplyAreaLight(AreaLight light, Material material, float3 albedo, LightContext context, inout float3 diffuseColor, inout float3 specularColor) {
    if (light.isActive == 0) return;

    // 1. 面光源のローカル基底ベクトルを計算 (directionを法線とする)
    float3 N_light = normalize(light.direction);
    // Y軸との外積から右ベクトルを生成 (平行な場合はZ軸を使う)
    float3 up = abs(N_light.y) < 0.999 ? float3(0, 1, 0) : float3(0, 0, 1);
    float3 right_light = normalize(cross(up, N_light));
    float3 up_light = cross(N_light, right_light);

    // 2. 面の中心からピクセルへのベクトルをローカル座標系に射影
    float3 p0 = context.worldPosition - light.position;
    float projRight = dot(p0, right_light);
    float projUp = dot(p0, up_light);
    
    // 3. 矩形のサイズ内にクランプして、ピクセルに最も近い「代表点」を求める
    float halfWidth = light.size.x * 0.5f;
    float halfHeight = light.size.y * 0.5f;
    projRight = clamp(projRight, -halfWidth, halfWidth);
    projUp = clamp(projUp, -halfHeight, halfHeight);
    
    // 代表点のワールド座標
    float3 closestPoint = light.position + right_light * projRight + up_light * projUp;
    
    // 4. 代表点からピクセルへの方向と距離
    float3 lightDir = normalize(context.worldPosition - closestPoint);
    float d = length(context.worldPosition - closestPoint);
    
    // 5. 面の裏側には光を放たないためのカットオフ
    // N_light(光源の向き)とlightDir(光源からピクセルへの向き)の内積で判定
    float NdotL_light = dot(N_light, lightDir);
    if (NdotL_light <= 0.0f) return;

    // 6. 距離減衰と、光源面からの角度による減衰(Lambertian)
    float attenuation = saturate(1.0f - d / max(light.range, 1e-5f));
    // 以前のポイントライトに近い明るさを確保するため線形減衰(1乗)とし、
    // 横方向への光の広がりを確保するため角度減衰を緩和する
    attenuation *= lerp(0.5f, 1.0f, NdotL_light);

    // 7. 最終的なライティングの適用
    if (material.lightingMode == 3) { // PBR
        CalculatePBR(context, lightDir, light.color.rgb, light.intensity * attenuation, material, albedo, diffuseColor, specularColor);
    } else {
        float diffuseFactor = CalculateDiffuseFactor(context.normal, lightDir, material.lightingMode);
        float specularFactor = CalculateSpecularFactor(context.normal, lightDir, context.toEye, material.roughness);

        diffuseColor += albedo * light.color.rgb * light.intensity * diffuseFactor * attenuation;
        specularColor += light.color.rgb * light.intensity * specularFactor * attenuation;
    }
}
