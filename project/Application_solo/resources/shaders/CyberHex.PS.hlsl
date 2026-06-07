/**
 * @file CyberHex.PS.hlsl
 * @brief サイバー風ヘックスシールド（六角形グリッド）描画用ピクセルシェーダ
 * 
 * @note
 * ==============================================================================
 * Original ShaderToy: "Hexagons - distance" by Inigo Quilez (https://www.shadertoy.com/view/Xd2CGt)
 * 
 * 【ShaderToyからの主な変更点（IrufemiEngine向け最適化）】
 * 1. 座標系のワールド空間化 (Triplanar Mappingの導入)
 *    - 元の `fragCoord.xy / iResolution.y` (画面座標) ではなく、`input.worldPosition` を使用。
 *    - `input.normal` を元にXY, XZ, ZY平面を自動判定し、PlaneのScaleに依存せず常に均一な密度で六角形を描画するように変更。
 * 
 * 2. C++側からのパラメータ制御 (`gMaterial.uvTransform` のハック)
 *    - `gMaterial.uvTransform[0][0]` をヘキサゴンの「密度（スケール）」パラメータとして利用。
 *    - `gMaterial.uvTransform[1][1]` をアニメーションの「進行速度」パラメータとして利用。
 * 
 * 3. プロシージャルノイズへの置換
 *    - 元コードのテクスチャ(`iChannel0`)に依存するノイズを、自作の `hash12_safe` および `noise` (3D Value Noise) に置き換え。
 * 
 * 4. 視覚効果の調整（フリッカー防止・色調調整）
 *    - 発光色の切り替わり境界を `smoothstep(0.45, 0.451)` から `smoothstep(0.3, 0.6)` へ広げ、激しい点滅（フリッカー）を防止。
 *    - 発光色をハードコードから `gMaterial.color` へ変更し、C++から動的に色（属性）を変更可能に。
 *    - 全体的な明るさが強すぎたため、ベースの `intensity` 計算とトーンマッピングを落ち着いた値に調整。
 * 
 * 5. エンジン標準の影（ShadowMap）の統合
 *    - 自発光だけでなく周囲の環境と馴染ませるため、`Lighting.hlsli` をインクルード。
 *    - `CalculateShadow` を使用して他のオブジェクトから落ちる影を受け取り、影の部分は暗くなるよう乗算処理を追加。
 * ==============================================================================
 */

#include "Object3d.hlsli"
#include "Lighting.hlsli"
#include "PerFrame.hlsli"

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<LightCommonData> gLightCommon : register(b1);
ConstantBuffer<PerFrameData> gPerFrame : register(b2);

SamplerComparisonState gShadowSampler : register(s2);
Texture2D<float32_t> gShadowMap : register(t5);

#include "Noise.hlsli"

// 六角形の距離とセルIDを計算する関数
// 戻り値: { 2d cell id x, 2d cell id y, distance to border, distance to center }
float4 hexagon(float2 p) 
{
    float2 q = float2(p.x * 2.0 * 0.5773503, p.y + p.x * 0.5773503);
    
    float2 pi = floor(q);
    float2 pf = frac(q);

    // 負の数に安全なモジュロ計算 (GLSLのmod互換)
    float v = (pi.x + pi.y) - 3.0 * floor((pi.x + pi.y) / 3.0);

    float ca = step(1.0, v);
    float cb = step(2.0, v);
    float2 ma = step(pf.xy, pf.yx);
    
    // distance to borders
    float e = dot(ma, 1.0 - pf.yx + ca * (pf.x + pf.y - 1.0) + cb * (pf.yx - 2.0 * pf.xy));

    // distance to center    
    p = float2(q.x + floor(0.5 + p.y / 1.5), 4.0 * p.y / 3.0) * 0.5 + 0.5;
    float f = length((frac(p) - 0.5) * float2(1.0, sqrt(3.0) / 2.0));        
    
    return float4(pi + ca - cb * ma, e, f);
}

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) 
{
    PixelShaderOutput output;
    
    // C++側から渡される gMaterial.uvTransform[1][1] をアニメーション速度として利用
    float speed = gMaterial.uvTransform[1][1];
    if (speed == 1.0) {
        speed = 0.2; // 未設定の場合はデフォルト速度
    }
    float time = gPerFrame.time * speed;
    
    // PlaneのScaleに依存せず、床や壁で同じ密度になるようワールド座標ベースでマッピングする
    float3 absN = abs(input.normal);
    float2 pos = float2(0.0, 0.0);
    
    // 面の向き（法線）に応じて投影する軸を決定
    if (absN.y > absN.x && absN.y > absN.z) {
        pos = input.worldPosition.xz; // 床・天井
    } else if (absN.x > absN.y && absN.x > absN.z) {
        pos = input.worldPosition.zy; // X軸方向の壁
    } else {
        pos = input.worldPosition.xy; // Z軸方向の壁
    }
    
    // C++側から渡される gMaterial.uvTransform[0][0] をヘキサゴンの密度パラメータとして利用する
    // ※値が大きいほどヘキサゴンが小さく（密に）なります
    float density = gMaterial.uvTransform[0][0];
    if (density == 1.0) {
        density = 0.05; // 未設定の場合はデフォルト値
    }
    
    pos *= density;

    // 空間を軽く歪ませてサイバーな空間の奥行き・レンズ効果を演出
    pos *= 1.2 + 0.05 * length(pos);

    // ==========================================
    // 1. ベースとなるグレーのヘックス（奥の層）
    // ==========================================
    float4 h = hexagon(8.0 * pos + 0.5 * time);
    float n = noise(float3(0.3 * h.xy + time * 0.1, time));
    float3 col = 0.15 + 0.15 * rand(h.xy + 1.2) * float3(1.0, 1.0, 1.0);
    col *= smoothstep(0.10, 0.11, h.z); // 枠線
    col *= smoothstep(0.10, 0.11, h.w); // 中心
    col *= 1.0 + 0.15 * sin(40.0 * h.z);
    col *= 0.75 + 0.5 * h.z * n;

    // ==========================================
    // 2. シャドウ（影の層）
    // ==========================================
    h = hexagon(6.0 * (pos + 0.1 * float2(-1.3, 1.0)) + 0.6 * time);
    col *= 1.0 - 0.8 * smoothstep(0.45, 0.451, noise(float3(0.3 * h.xy + time * 0.1, 0.5 * time)));

    // ==========================================
    // 3. 発光するカラーヘックス（手前の層）
    // ==========================================
    h = hexagon(6.0 * pos + 0.6 * time);
    n = noise(float3(0.3 * h.xy + time * 0.1, 0.5 * time));
    
    // マテリアルカラーを基準に発光色を決定
    float3 baseColor = gMaterial.color.rgb; 
    // 明滅の強さを抑える（0.9+0.8 から 0.6+0.4 へ）
    float intensity = 0.6 + 0.4 * sin(rand(h.xy) * 1.5 + 2.0); 
    float3 colb = baseColor * intensity;
    
    colb *= smoothstep(0.10, 0.11, h.z); // 枠線
    colb *= 1.0 + 0.15 * sin(40.0 * h.z);

    // ==========================================
    // 4. ブレンドとポスト処理
    // ==========================================
    // ノイズ値を使ってベース（奥）とカラー（手前）をブレンド
    // 境界を滑らかにしてチカチカするフリッカーを抑える（0.45, 0.451 -> 0.3, 0.6）
    col = lerp(col, colb, smoothstep(0.3, 0.6, n));
    
    // トーンマッピング（全体的な明るさを抑える）
    col *= 1.5 / (1.5 + col);

    // キャラクターや建物からの影（ShadowMap）を適用して接地感を出す
    float shadowFactor = CalculateShadow(input.shadowPos, gShadowMap, gShadowSampler, normalize(input.normal), gLightCommon.directionalLight.direction);
    // 影の領域は明るさを30%に落とす
    col *= lerp(0.3, 1.0, shadowFactor);

    // ビネット効果（四隅を暗くする）
    float2 uv = input.texcoord;
    col *= pow(max(16.0 * uv.x * (1.0 - uv.x) * uv.y * (1.0 - uv.y), 0.0), 0.1);

    output.color = float4(saturate(col), gMaterial.color.a);
    return output;
}
