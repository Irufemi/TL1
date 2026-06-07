struct PixelInput {
    float4 pos : SV_POSITION;
};

float4 main(PixelInput input) : SV_TARGET {
    // 真っ白（マスクとして1を出力）
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}
