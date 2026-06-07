/**
 * @file EnergyCore.PS.hlsl
 * @brief エネルギー球体描画用ピクセルシェーダ
 * 
 * @note 
 * This shader is based on "Learning to Dig" by David Hoskins.
 * Original Source: https://www.shadertoy.com/view/4dX3Wn
 * License: Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.
 * 
 * 
 * 【ShaderToy (GLSL) から IrufemiEngine (HLSL 3D環境) への移植に伴う主要な変更点】
 * 
 * 1. 乱数精度の修正 (hash関数の安定化)
 *    - 元の `hash` 関数はGPUの浮動小数点精度によってはノイズが消失し「真っ白な球体」になる問題があったため、
 *      より堅牢な `hash12` (Dave_Hoskins氏の別実装) に差し替えてマグマのディテールを復元しました。
 * 
 * 2. 行列のメモリレイアウト修正 (フレアの描画)
 *    - GLSL(列優先)とHLSL(行優先)の行列積の仕様違いによりフレアが正しく回転・描画されなかったため、
 *      `mul` 関数の引数順序および回転行列の構造をHLSL向けに修正しました。
 * 
 * 3. 2D空間から3D球体へのマッピング (viewNormalの使用)
 *    - ShaderToyは画面全体の2D座標(UV)を使いますが、本エンジンでは3Dの球体モデル(Sphere)に貼り付けるため、
 *      `viewNormal`（カメラから見た頂点法線）のXY成分を使って擬似的な2D座標系(`xy`)を構築しています。
 * 
 * 4. ブレンドモードと背景合成の最適化 (Premultiplied Alpha)
 *    - 元コードは「真っ黒な背景」を前提に各色が合成（Pre-multiplied）されていました。
 *      これを青空などの3D背景と綺麗に合成するため、エンジン側に `kBlendModePremultiplied` を新設し、
 *      シェーダー内で「不透明なマグマ本体 (Opaque)」と「加算発光するオーラ・フレア (Additive Glow)」に
 *      レイヤーを完全に分離・再構築して出力するように設計を抜本的に変更しました。
 * 
 * 5. 時間経過によるアニメーション (time)
 *    - `gPerFrame.time` を用いて、マグマのサンプリング座標(q.y)をスクロールさせることで流体アニメーションを有効化しました。
 * 
 * 6. 色の汎用化による属性対応 (gMaterial.colorの利用)
 *    - 元のシェーダーでは緑と青の色計算がハードコードされていましたが、C++側から渡される `gMaterial.color` を
 *      ベース色として動的に「マグマの影・エッジ・オーラの色」を相対計算するように改修しました。
 *      これにより、1つのシェーダーで「炎(赤)」「雷(黄)」「氷(青)」などの属性表現が可能になりました。
 */

#include "Object3d.hlsli"
#include "PerFrame.hlsli"
#include "Material.hlsli"

ConstantBuffer<PerFrameData> gPerFrame : register(b2);
ConstantBuffer<Material> gMaterial : register(b0);

float hash_original(float2 p)
{
    float3 p3  = frac(float3(p.x, p.y, p.x) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return -1.0 + 2.0 * frac((p3.x + p3.y) * p3.z);
}

static const float2 add_vec = float2(1.0, 0.0);

float noise_custom(float2 x)
{
    float2 p = floor(x);
    float2 f = frac(x);
    f = f*f*(3.0 - 2.0*f);
    
    float res = lerp(lerp(hash_original(p), hash_original(p + float2(1.0, 0.0)), f.x),
                     lerp(hash_original(p + float2(0.0, 1.0)), hash_original(p + float2(1.0, 1.0)), f.x), f.y);
    return res;
}

static const float2x2 m = float2x2(0.80, 0.60, -0.60, 0.80);

float fbm4(float2 p)
{
    float f = 0.0;
    f += 0.5000 * noise_custom(p); p = mul(m, p) * 2.02;
    f += 0.2500 * noise_custom(p); p = mul(m, p) * 2.03;
    f += 0.1250 * noise_custom(p); p = mul(m, p) * 2.01;
    f += 0.0625 * noise_custom(p);
    return f / 0.9375;
}

float fbm6(float2 p)
{
    float f = 0.0;
    f += 0.500000 * (0.5 + 0.5 * noise_custom(p)); p = mul(m, p) * 2.02;
    f += 0.250000 * (0.5 + 0.5 * noise_custom(p)); p = mul(m, p) * 2.03;
    f += 0.125000 * (0.5 + 0.5 * noise_custom(p)); p = mul(m, p) * 2.01;
    f += 0.062500 * (0.5 + 0.5 * noise_custom(p)); p = mul(m, p) * 2.04;
    f += 0.031250 * (0.5 + 0.5 * noise_custom(p)); p = mul(m, p) * 2.01;
    f += 0.015625 * (0.5 + 0.5 * noise_custom(p));
    return f / 0.96875;
}

float3 lava(float2 q, float d, float time)
{
    q *= 2.0;
    q.y -= time * 0.2; // Add continuous scrolling flow to the magma
    float ql = length(q);
    q.x += 0.05 * sin(0.7 * time + ql * 4.7);
    q.y += 0.05 * sin(0.3 * time + ql * 4.7);
    q *= 0.7;

    float2 o = float2(0.0, 0.0);
    o.x = 0.5 + 0.5 * fbm6(2.0 * q);
    o.y = 0.5 + 0.5 * fbm6(2.0 * q + float2(5.2, 1.3));

    float ol = length(o);
    o.x += 0.02 * sin(0.12 * time * 14.0 + ol) / (ol + 0.001);
    o.y += 0.02 * sin(0.14 * time * 14.0 + ol) / (ol + 0.001);

    float2 n_shift;
    n_shift.x = fbm6(7.0 * o + float2(19.2, 19.2));
    n_shift.y = fbm6(7.0 * o + float2(15.7, 15.7));

    float2 p = 4.0 * q + 4.0 * n_shift;

    float f = 0.5 + 0.5 * fbm4(p);

    float3 n = float3(0.0, 0.0, 0.0);
    n.x = fbm4(p + float2(0.01, 0.0)) - fbm4(p - float2(0.01, 0.0));
    n.y = fbm4(p + float2(0.0, 0.01)) - fbm4(p - float2(0.0, 0.01));
    n.z = 0.05;
    n = normalize(n);

    f = lerp(f, f * f * f * 3.5, f * abs(n.x));

    float g = 0.5 + 0.5 * sin(4.0 * p.x) * sin(4.0 * p.y);
    f *= 1.0 - 0.5 * pow(abs(g), 8.0);

    // HLSL min < max was already true, no need to invert
    float3 col = lerp(float3(f, f, 0), float3(1.0 - f * 0.3, 1.0 - f * 0.3, 1.0 - f * 0.3), smoothstep(-0.4, -0.01, d));
    col += lerp(float3(0,0,0), float3(pow(abs(f), 5.0), pow(abs(f), 5.0), pow(abs(f), 5.0)) * 0.4, smoothstep(-0.5, -0.01, d));
    col = lerp(col, float3(1.0, 1.0, 0.0), n.x * 0.5);
    col -= float3(0.0, 1.0, 1.0) * dot(o, o) * (d + 0.5);
    
    return col;
}

float r(float2 x) { return frac(1e4 * sin(x.x * 545.3 + x.y * 314.1)); } 
float sr2(float x) { return r(float2(x, x + 0.1)) * 2.0 - 1.0; }

float flare(float2 U, float time)
{
    float2 A = sin(float2(0, 1.57) + time * 1.0);
    float2x2 m1 = float2x2(A.x, -A.y, A.y, A.x);
    float2x2 m2 = float2x2(2.0, 1.0, 0.0, 1.73);
    U = abs(mul(m1, U));
    U = mul(m2, U);
    return 0.2 / max(max(U.x, U.y), 0.001);
}

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    float time = gPerFrame.time;
    
    // カメラのView行列を使って、ワールド法線をビュー空間（カメラから見た空間）の法線に変換する
    float3 viewNormal = normalize(mul(normalize(input.normal), (float3x3)gPerFrame.view));

    // ビュー空間の法線のXY成分は、真正面(0,0)から輪郭(半径1の円)に広がる2D座標になる。
    float2 xy = viewNormal.xy * 1.2;
    float d = length(xy) - 0.85;

    float c = 0.0;
    // Stars/Flares (Optimized count for 3D model pixel shader)
    for (float i = 0.0; i < 20.0; i++)
    {
        float2 flarePos = xy * 0.5 - float2(sr2(i + 9.0), sr2(i * 1.5 + 3.0));
        float r_val = r(float2(i + 0.4, i * 2.0));
        c += flare(flarePos, time) * r_val * (1.4 + sin(time * 2.0 + r_val * 15.2)) * 0.01;
    }
    // ==========================================
    // 1. Opaque Magma Core (不透明なマグマ部分)
    // ==========================================
    float3 baseColor = gMaterial.color.rgb;
    float3 magmaCol = baseColor * 0.2; // ベースの暗い色
    magmaCol = lerp(magmaCol, lava(xy, d, time), 1.0 - smoothstep(-0.015, -0.01, d)); // マグマ
    magmaCol = lerp(magmaCol, float3(1.0, 1.0, 1.0), 1.0 - smoothstep(-0.8, -0.5, d)); // 中心コア（白）
    magmaCol = lerp(magmaCol, baseColor * 1.5, smoothstep(-0.01, 0.0, d) * (1.0 - smoothstep(0.0, 0.01, d))); // エッジ内側
    magmaCol = lerp(magmaCol, baseColor * 0.8, smoothstep(0.005, 0.011, d) * (1.0 - smoothstep(0.011, 0.012, d))); // エッジ外側
    
    // マグマ本体の不透明度（d=0付近で滑らかに透明になる）
    float magmaAlpha = 1.0 - smoothstep(-0.01, 0.01, d);

    // ==========================================
    // 2. Additive Glow (加算発光するオーラとフレア)
    // ==========================================
    float3 flareColor = float3(c, c, c); // フレアの光
    float auraWeight = (1.0 - smoothstep(0.001, 0.15, d)) * 0.8; // オーラの強さ（少し広げて強調）
    float3 auraColor = baseColor * auraWeight;
    
    // マグマの外側だけに加算光を適用する（マグマの中は不透明なマグマ色を優先）
    float3 glowColor = (flareColor + auraColor) * (1.0 - magmaAlpha);

    // ==========================================
    // 3. Composite for Premultiplied Alpha
    // ==========================================
    // 計算式: Dest = SrcColor + DestColor * (1 - SrcAlpha)
    float3 finalColor = (magmaCol * magmaAlpha) + glowColor;
    float finalAlpha = magmaAlpha;

    // 3Dモデルのメッシュ境界（球の外枠）で不自然に切れないように、メッシュエッジで全体をフェードアウト
    float edgeFade = 1.0 - smoothstep(0.85, 0.98, length(viewNormal.xy));
    finalColor *= edgeFade;
    finalAlpha *= edgeFade;

    // 完全に透明な場所は描画をスキップ（パフォーマンス最適化）
    if(finalAlpha < 0.005 && max(finalColor.r, max(finalColor.g, finalColor.b)) < 0.005) discard;

    output.color = float4(saturate(finalColor), saturate(finalAlpha));
    return output;
}
