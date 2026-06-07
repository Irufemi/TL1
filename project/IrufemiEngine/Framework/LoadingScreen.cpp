#include "LoadingScreen.h"
#include "../Engine/IrufemiEngine.h"
#include "../Renderer/Object2D/Sprite/Sprite.h"
#include "../Renderer/Object2D/Primitive/Circle2D.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "../Engine/Graphics/Pipeline/PSOManager.h"

LoadingScreen::LoadingScreen() = default;
LoadingScreen::~LoadingScreen() = default;

void LoadingScreen::Initialize(IrufemiEngine* engine) {
    if (!engine) return;

    camera_ = std::make_unique<Camera>();
    camera_->Initialize(engine->GetClientWidth(), engine->GetClientHeight());
    camera_->UpdateMatrix();

    nowLoadingText_ = std::make_unique<Sprite>();
    // 生成した「Now Loading」画像をセット
    nowLoadingText_->Initialize("resources/texture/load/now_loading.png");
    
    // 画像は黒背景に文字が含まれる
    // 描画時に加算合成(Add)を使うことで、黒を透過させる
    nowLoadingText_->SetColor({1.0f, 1.0f, 1.0f, 1.0f});

    float screenW = static_cast<float>(engine->GetClientWidth());
    float screenH = static_cast<float>(engine->GetClientHeight());
    
    // 背景を真っ黒に塗りつぶすスプライト
    bgSprite_ = std::make_unique<Sprite>();
    bgSprite_->Initialize("resources/whiteTexture.png");
    bgSprite_->SetColor({0.0f, 0.0f, 0.0f, 1.0f});
    bgSprite_->SetSize(screenW, screenH);
    bgSprite_->SetAnchor(0.0f, 0.0f); // 左上
    bgSprite_->SetPosition(0.0f, 0.0f);
    bgSprite_->SetTopMost(true);
    
    // スプライトのサイズと位置を右下に合わせる
    // 画像は正方形(1:1)なので、縮尺がおかしくならないよう同サイズにする
    nowLoadingText_->SetSize(256.0f, 256.0f);
    nowLoadingText_->SetAnchor(1.0f, 0.5f); // 右端・縦中央アンカー
    nowLoadingText_->SetPosition(screenW - 80.0f, screenH - 45.0f); // ドットの高さと合わせる
    
    // "..." のドットを3つ作る
    for (int i = 0; i < 3; ++i) {
        auto dot = std::make_unique<Circle2D>();
        // Circle2Dは白テクスチャを内部的に使って丸を描く
        dot->Initialize("resources/whiteTexture.png", 16); 
        dot->SetUseTexture(false); 
        dot->SetColor({1.0f, 1.0f, 1.0f, 1.0f}); // 文字のNeon Cyanに合わせた色にする
        
        // 中心座標をセット: nowLoadingText_ のさらに右側に等間隔で配置
        float baseX = screenW - 65.0f; 
        float baseY = screenH - 45.0f; 
        dot->SetCenter({baseX + i * 20.0f, baseY, 0.0f});
        dot->SetRadius(4.0f);
        
        // 最前面UIとして登録
        dot->SetTopMost(true);

        dots_.push_back(std::move(dot));
    }

    // 最前面UIとして登録
    nowLoadingText_->SetTopMost(true);
}

void LoadingScreen::Update(float deltaTime) {
    animationTimer_ += deltaTime;
    // 0.5秒ごとにドットが1個増え、0 -> 1 -> 2 -> 3 -> 0 のループになる
    const float kDotInterval = 0.5f;
    if (animationTimer_ > kDotInterval) {
        animationTimer_ -= kDotInterval;
        dotCount_ = (dotCount_ + 1) % 4; 
    }

    camera_->Update();
    if (bgSprite_) {
        bgSprite_->Update();
    }
    if (nowLoadingText_) {
        nowLoadingText_->Update();
    }
    for (auto& dot : dots_) {
        dot->Update();
    }
}

void LoadingScreen::Draw(IrufemiEngine* engine) {
    if (!engine) return;

    // ウィンドウのリサイズに対応するため、描画時に画面サイズに合わせて位置とサイズを動的に更新する
    float screenW = static_cast<float>(engine->GetClientWidth());
    float screenH = static_cast<float>(engine->GetClientHeight());
    float uiScale = screenH / 720.0f;

    if (bgSprite_) {
        // 背景は必ず加算ではなく通常のブレンド（不透明）で上書き描画する
        engine->SetBlend(BlendMode::kBlendModeNormal);
        bgSprite_->SetSize(screenW, screenH);
        bgSprite_->Draw();
    }
    
    // 文字とドットは加算合成（黒背景を透過）
    engine->SetBlend(BlendMode::kBlendModeAdd);
    if (nowLoadingText_) {
        nowLoadingText_->SetUIScale(uiScale);
        nowLoadingText_->SetPosition(screenW - 80.0f * uiScale, screenH - 45.0f * uiScale);
        nowLoadingText_->Draw();
    }
    
    float baseX = screenW - 65.0f * uiScale; 
    float baseY = screenH - 45.0f * uiScale; 
    for (int i = 0; i < dots_.size(); ++i) {
        dots_[i]->SetRadius(4.0f * uiScale);
        dots_[i]->SetCenter({baseX + i * 20.0f * uiScale, baseY, 0.0f});
    }

    for (int i = 0; i < dotCount_; ++i) {
        if (i < dots_.size()) {
            dots_[i]->Draw();
        }
    }
    
    // 描画後、安全のために元の通常ブレンドに戻す
    engine->SetBlend(BlendMode::kBlendModeNormal);
}
