#include "VertexData.hlsli"
#include "Object3d.hlsli"
#include "PerFrame.hlsli"

struct PixelInput {
    float4 pos : SV_POSITION;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);
ConstantBuffer<PerFrameData> gPerFrame : register(b2);

PixelInput main(VertexInput input) {
    PixelInput output;
    float4 worldPos = mul(input.position, gTransformationMatrix.World);
    float4 viewPos = mul(worldPos, gPerFrame.view);
    output.pos = mul(viewPos, gPerFrame.projection);
    return output;
}
