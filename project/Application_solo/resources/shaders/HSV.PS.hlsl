#include "Fullscreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct HSVParams {
    float32_t hue;
    float32_t saturation;
    float32_t value;
};
ConstantBuffer<HSVParams> gHSVParams : register(b0);

float32_t WrapValue(float32_t value, float32_t minRange, float32_t maxRange) {
    float32_t range = maxRange - minRange;
    float32_t modValue = fmod(value - minRange, range);
    if (modValue < 0) {
        modValue += range;
    }
    return minRange + modValue;
}

float32_t3 RGBToHSV(float32_t3 rgb) {
    float32_t maxVal = max(rgb.r, max(rgb.g, rgb.b));
    float32_t minVal = min(rgb.r, min(rgb.g, rgb.b));
    float32_t delta = maxVal - minVal;

    float32_t3 hsv = float32_t3(0, 0, maxVal);

    if (delta > 0) {
        if (maxVal == rgb.r) {
            hsv.x = (rgb.g - rgb.b) / delta;
        } else if (maxVal == rgb.g) {
            hsv.x = 2 + (rgb.b - rgb.r) / delta;
        } else {
            hsv.x = 4 + (rgb.r - rgb.g) / delta;
        }
        hsv.x /= 6.0;
        if (hsv.x < 0) hsv.x += 1.0;

        hsv.y = delta / maxVal;
    }

    return hsv;
}

float32_t3 HSVToRGB(float32_t3 hsv) {
    float32_t h = hsv.x * 6.0;
    float32_t s = hsv.y;
    float32_t v = hsv.z;

    float32_t C = v * s;
    float32_t X = C * (1.0 - abs(fmod(h, 2.0) - 1.0));
    float32_t m = v - C;

    float32_t3 rgb = float32_t3(0, 0, 0);

    if (h < 1.0) rgb = float32_t3(C, X, 0);
    else if (h < 2.0) rgb = float32_t3(X, C, 0);
    else if (h < 3.0) rgb = float32_t3(0, C, X);
    else if (h < 4.0) rgb = float32_t3(0, X, C);
    else if (h < 5.0) rgb = float32_t3(X, 0, C);
    else rgb = float32_t3(C, 0, X);

    return rgb + m;
}

struct PSOutput {
    float32_t4 color : SV_TARGET0;
};

PSOutput main(VertexShaderOutput input) {
    PSOutput output;
    float32_t4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    
    // RGB -> HSV
    float32_t3 hsv = RGBToHSV(textureColor.rgb);
    
    // Adjust
    hsv.x += gHSVParams.hue;
    hsv.y += gHSVParams.saturation;
    hsv.z += gHSVParams.value;
    
    // Wrap/Saturate
    hsv.x = WrapValue(hsv.x, 0.0, 1.0);
    hsv.y = saturate(hsv.y);
    hsv.z = saturate(hsv.z);
    
    // HSV -> RGB
    output.color.rgb = HSVToRGB(hsv);
    output.color.a = textureColor.a;
    
    return output;
}
