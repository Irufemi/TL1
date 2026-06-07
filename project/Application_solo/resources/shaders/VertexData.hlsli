#pragma once

/**
 * @file VertexData.hlsli
 * @brief C++側のVertexData構造体と完全に一致する頂点入力レイアウト
 * @details 今後頂点情報（Tangent等）を追加する際は、このファイルとC++側のVertexData.hを同期させる。
 */
struct VertexInput {
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal   : NORMAL0;
    float32_t4 color    : COLOR0;
};
