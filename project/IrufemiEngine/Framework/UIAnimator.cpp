#include "UIAnimator.h"
#include <cmath>

void UIAnimator::Update(float deltaTime) {
    time_ += deltaTime;
}

void UIAnimator::Reset() {
    time_ = 0.0f;
}

float UIAnimator::GetPulseAlpha(float base, float amplitude, float speed) const {
    return base + std::sin(time_ * speed) * amplitude;
}

bool UIAnimator::GetFlashVisibility(float speed) const {
    return std::sin(time_ * speed) > 0.0f;
}

float UIAnimator::GetFloatOffset(float amplitude, float speed, float phaseOffset) const {
    return std::sin(time_ * speed + phaseOffset) * amplitude;
}
