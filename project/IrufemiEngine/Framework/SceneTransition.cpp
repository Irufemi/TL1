#include "SceneTransition.h"
#include <algorithm>

void SceneTransition::Initialize(PostProcessManager* ppManager) {
    ppManager_ = ppManager;
}

void SceneTransition::Start(Type type, float duration, bool isOut, EaseType easeType) {
    if (!ppManager_) return;

    currentType_ = type;
    easeType_ = easeType;
    duration_ = (std::max)(0.001f, duration); // 0除算防止
    isOut_ = isOut;
    timer_ = 0.0f;
    isActive_ = true;

    // 前回のトランジション演出で使ったエフェクトだけを確実に取り除く
    for (auto mode : activeTransitionModes_) {
        ppManager_->RemoveActiveMode(mode);
    }
    activeTransitionModes_.clear();

    // 基本は黒フェードにリセットしておく（白フェード等で上書きされた色が残るのを防ぐため）
    ppManager_->GetFadeParams().color = {0.0f, 0.0f, 0.0f, 1.0f};

    switch (currentType_) {
    case Type::Fade:
        ppManager_->AddActiveMode(PostProcessMode::Fade);
        activeTransitionModes_.push_back(PostProcessMode::Fade);
        break;
    case Type::Dissolve:
        ppManager_->AddActiveMode(PostProcessMode::Dissolve);
        activeTransitionModes_.push_back(PostProcessMode::Dissolve);
        break;
    case Type::Slide:
        ppManager_->AddActiveMode(PostProcessMode::Slide);
        activeTransitionModes_.push_back(PostProcessMode::Slide);
        break;
    case Type::RadialBlur:
        // 放射状ブラーとフェードを併用
        ppManager_->AddActiveMode(PostProcessMode::RadialBlur);
        ppManager_->AddActiveMode(PostProcessMode::Fade);
        activeTransitionModes_.push_back(PostProcessMode::RadialBlur);
        activeTransitionModes_.push_back(PostProcessMode::Fade);
        ppManager_->GetFadeParams().color = {0.0f, 0.0f, 0.0f, 1.0f}; // 黒
        break;
    case Type::RadialBlurWhite:
        // 放射状ブラーとフェード(白)を併用
        ppManager_->AddActiveMode(PostProcessMode::RadialBlur);
        ppManager_->AddActiveMode(PostProcessMode::Fade);
        activeTransitionModes_.push_back(PostProcessMode::RadialBlur);
        activeTransitionModes_.push_back(PostProcessMode::Fade);
        ppManager_->GetFadeParams().color = {1.0f, 1.0f, 1.0f, 1.0f}; // 白
        break;
    }
}

void SceneTransition::Update(float deltaTime) {
    if (!isActive_ || !ppManager_) return;

    timer_ += deltaTime;
    float totalDuration = duration_ + kDwellTime;

    if (timer_ >= totalDuration) {
        timer_ = totalDuration;
        isActive_ = false;
        
        // フェードイン（画面が表示される方）が完了した場合はトランジションエフェクトのみをクリア
        if (!isOut_) {
            for (auto mode : activeTransitionModes_) {
                ppManager_->RemoveActiveMode(mode);
            }
            activeTransitionModes_.clear();
        }
    }

    // 演出自体の進行度 (0.0 ~ 1.0) 
    // 溜め時間 (kDwellTime) 中は 1.0 固定にする
    float progress = (std::min)(1.0f, timer_ / duration_);
    
    // イージング関数の適用
    float easedProgress = EvaluateEase(easeType_, progress);
    
    // 実際にエフェクトに適用する係数
    float factor = isOut_ ? easedProgress : (1.0f - easedProgress);

    // 各モードのパラメータに反映
    switch (currentType_) {
    case Type::Fade:
        ppManager_->GetFadeParams().intensity = factor;
        break;
    case Type::Dissolve:
        // 1.1 まで動かすことでノイズを確実に消し去る
        ppManager_->GetDissolveParams().threshold = factor * 1.1f;
        break;
    case Type::Slide:
        // 1.05 まで動かすことで境界のボケ(edgeWidth=0.02)を画面外へ完全に追いやる
        ppManager_->GetSlideParams().threshold = factor * 1.05f;
        break;
    case Type::RadialBlur:
    case Type::RadialBlurWhite:
        ppManager_->GetRadialBlurParams().blurWidth = factor * 0.05f;
        ppManager_->GetFadeParams().intensity = factor;
        break;
    }
}
