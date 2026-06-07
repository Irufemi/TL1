#include "Object3d.hlsli"
#include "Noise.hlsli"

/**
 * @file LightningCrawl.PS.hlsl
 * @brief モデル表面を電撃が這う表現を行うピクセルシェーダー
 */

// 特殊パラメータ (register b6 / RootSlot::Special)
struct LightningParams {
    float32_t4 color;           //!< 表面の色
    float32_t4 coreColor;       //!< 芯の色
    float32_t speed;            //!< アニメーション速度
    float32_t intensity;        //!< 表面の輝度
    float32_t noiseScale;       //!< 表面の密度
    float32_t noiseThreshold;   //!< 表面のしきい値
    float32_t coreIntensity;    //!< 芯の輝度
    float32_t coreThreshold;    //!< 芯の太さ
    float32_t coreScale;        //!< 芯の密度
    float32_t pad;
};
ConstantBuffer<LightningParams> gLightning : register(b6);

#include "PerFrame.hlsli"

// カメラ情報 (register b2 / RootSlot::Camera)
ConstantBuffer<PerFrameData> gPerFrame : register(b2);

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;

    float2 uv = input.texcoord;
    float32_t time = gPerFrame.time * gLightning.speed;

    // --- 1. Surface Crawl (表面を這う電撃) ---
    // UVを3次元の円筒座標に変換してサンプリングすることで、シームレス化を実現
    float angle = uv.x * TAU;
    // v方向(uv.y)に時間を引くことで、プラズマが前方に流れるアニメーションを付ける
    float3 pSurf = float3(cos(angle), sin(angle), uv.y - time * 2.0);
    float3 pTime = float3(time * 0.2, time * 0.1, time * 0.3);
    
    float32_t nSurf = fBm(pSurf * gLightning.noiseScale + pTime);
    // しきい値の判定を少し広めにとり、かつ急峻に立ち上げることで「繋がり」を維持
    float32_t surfaceBolt = 1.0 - saturate(abs(nSurf - gLightning.noiseThreshold) / 0.15);
    surfaceBolt = pow(surfaceBolt, 2.0); // 芯を強調
    
    // 表面に微細なノイズを乗せる
    float32_t sparkle = fBm(uv * gLightning.noiseScale * 8.0 - float32_t2(time * 2.0, time));
    surfaceBolt *= (0.7 + 0.3 * sparkle);

    // --- 2. Core Bolt (内部の芯) ---
    // Fresnel効果: 視線と法線が並行（正面）に近いほど大きくなる
    float32_t3 V = normalize(gPerFrame.cameraWorldPosition - input.worldPosition);
    float32_t3 N = normalize(input.normal);
    float32_t fresnel = saturate(dot(N, V));
    
    // 芯用のノイズもシームレスに前方へ流す
    float3 pCore = float3(cos(angle), sin(angle), uv.y * 0.5 - time * 3.0);
    float3 pTimeCore = float3(time * 1.0, -time * 0.5, time * 0.8);
    float32_t nCore = fBm(pCore * gLightning.coreScale + pTimeCore);

    // 芯は Fresnel が強い中心部に限定し、急激に減衰させる
    float32_t coreEffect = pow(fresnel, 8.0); // 中心を鋭く
    float32_t coreBolt = 1.0 - saturate(abs(nCore - gLightning.coreThreshold) / 0.08);
    coreBolt *= coreEffect;

    // --- 3. 合成 ---
    // 表面と芯を個別に加算
    float32_t3 finalColor = 0;
    
    // 表面レイヤー
    finalColor += gLightning.color.rgb * gLightning.intensity * surfaceBolt;
    
    // 芯レイヤー (より強く光らせる)
    finalColor += gLightning.coreColor.rgb * gLightning.coreIntensity * coreBolt;
    
    // ボリューム感を出すための弱いグロー
    finalColor += gLightning.coreColor.rgb * gLightning.coreIntensity * 0.3 * coreEffect;

    float32_t alpha = saturate(surfaceBolt + coreBolt + coreEffect * 0.5);
    
    if (alpha <= 0.01) { discard; }

    output.color = float32_t4(finalColor, alpha * gLightning.color.a);

    return output;
}
