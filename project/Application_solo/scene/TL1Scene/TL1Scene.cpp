#include "TL1Scene.h"
#include "Framework/SceneManager.h"
#include "Irufemi.h"
#include "../../LevelDataLoader.h"

// デストラクタ
TL1Scene::~TL1Scene() = default;

// 初期化
void TL1Scene::Initialize(IrufemiEngine* engine) {
    BaseScene::Initialize(engine);

    // プロジェクトのルート（または実行ファイル基準）から見た TL1.blevel のパス
    // パスは実行環境に合わせて適宜調整してください
    LevelDataLoader::LoadAndDeploy(this, "../../TL1.blevel");
}

// 更新
void TL1Scene::Update() {
    BaseScene::Update(); // これにより GameObject 群の Update が呼ばれる
}

// 描画
void TL1Scene::Draw() {
    BaseScene::Draw(); // これにより GameObject 群の Draw が呼ばれる
}

// デバッグタブの描画
void TL1Scene::DrawDebugTab() {
#if defined USE_IMGUI
    BaseScene::DrawDebugTab();
#endif
}
