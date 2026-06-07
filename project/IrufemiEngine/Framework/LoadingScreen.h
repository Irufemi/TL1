#pragma once
#include <memory>
#include <vector>

class IrufemiEngine;
class Sprite;
class Circle2D;
class Camera;

/**
 * @class LoadingScreen
 * @brief すべてのScene共通で使用されるローディング画面クラス
 * @details SceneManagerによって保持され、モデルやテクスチャのロード中（非同期読み込み待ち）の間だけ更新・描画されます。
 */
class LoadingScreen {
public:
    LoadingScreen();
    ~LoadingScreen();

    void Initialize(IrufemiEngine* engine);
    void Update(float deltaTime);
    void Draw(IrufemiEngine* engine);

private:
    std::unique_ptr<Camera> camera_;
    std::unique_ptr<Sprite> bgSprite_;
    std::unique_ptr<Sprite> nowLoadingText_;
    std::vector<std::unique_ptr<Circle2D>> dots_;
    
    float animationTimer_ = 0.0f;
    int dotCount_ = 0;
};
