#include "SceneManager.h"
#include "IScene.h"
#include "BaseScene.h"
#include "../Engine/IrufemiEngine.h"
#include <Windows.h>
#include "../Engine/Platform/Input/InputManager.h"
#include "../Engine/Platform/Input/Mouse.h"
#include "SceneSerializer.h"

namespace {
    /**
     * @class DataDrivenScene
     * @brief JSONファイルのみから構築される汎用シーン
     */
    class DataDrivenScene : public BaseScene {
    public:
        DataDrivenScene(const std::string& sceneFilePath) : sceneFilePath_(sceneFilePath) {}
        virtual ~DataDrivenScene() = default;

        void Initialize(IrufemiEngine* engine) override {
            BaseScene::Initialize(engine);
            SceneSerializer::Load(this, sceneFilePath_);
        }

    private:
        std::string sceneFilePath_;
    };
}

SceneManager::SceneManager(IrufemiEngine* engine) : engine_(engine) {
}

// 登録順を保持しつつ登録
void SceneManager::Register(const Key& name, Factory f) {
    auto it = factories_.find(name);
    if (it == factories_.end()) {
        order_.push_back(name);            // 新規登録時のみ順序リストに追加
        factories_.emplace(name, std::move(f));
    } else {
        it->second = std::move(f);         // 既存は上書きのみ(順序は変えない)
    }
}

// シーン切替要求(次の Update 冒頭で反映)
void SceneManager::Request(const Key& next) { pending_ = next; }

// 即時切替(初期化時など)
bool SceneManager::ChangeTo(const Key& next) {
    auto it = factories_.find(next);
    if (it == factories_.end()) { return false; }

    // シーン破棄前にGPU処理の完了を待つ (リソース解放中のアクセス違反を防ぐ)
    engine_->GetDirectXCommon()->WaitForGPU();

    // シーン破棄前に、現在のスタックの全シーンを終了させる
    for (auto it = sceneStack_.rbegin(); it != sceneStack_.rend(); ++it) {
        it->scene->OnExit();
        it->scene->Finalize();
    }
    sceneStack_.clear();

    SceneStackItem item;
    item.name = next;
    item.scene = it->second();
    
    // データがあればロード
    bool hasData = SceneSerializer::Load(item.scene.get(), next);

    isInitializing_ = true;
    item.scene->Initialize(engine_);
    isInitializing_ = false;

    // データがなくてエディタモードなら、初期状態を自動生成
#ifdef EditorMode
    if (!hasData) {
        SceneSerializer::Save(item.scene.get(), next);
    }
#endif

    item.scene->OnEnter(); // シーン開始

    sceneStack_.push_back(std::move(item));
    wasLoading_ = false;

    engine_->SetTimeScale(1.0f); // 時間の進みもリセット
    engine_->SetCursorLocked(!sceneStack_.back().scene->IsCursorVisible()); 
    return true;
}

void SceneManager::PushScene(const Key& name) {
    auto it = factories_.find(name);
    if (it == factories_.end()) { return; }

    engine_->GetDirectXCommon()->WaitForGPU();

    if (!sceneStack_.empty()) {
        // 現在の最前面シーンをサスペンド状態にする（バックグラウンドへ）
        sceneStack_.back().scene->OnSuspend();
        // ★全てのSE（効果音）とBGMをポーズする
        engine_->GetAudioManager()->PauseAll();
    }

    SceneStackItem item;
    item.name = name;
    item.scene = it->second();

    // データがあればロード
    bool hasData = SceneSerializer::Load(item.scene.get(), name);

    item.scene->Initialize(engine_);

    // データがなくてエディタモードなら、初期状態を自動生成
#ifdef EditorMode
    if (!hasData) {
        SceneSerializer::Save(item.scene.get(), name);
    }
#endif

    item.scene->OnEnter(); // 新しいシーン開始

    sceneStack_.push_back(std::move(item));

    engine_->SetCursorLocked(!sceneStack_.back().scene->IsCursorVisible());
}

void SceneManager::PopScene() {
    if (sceneStack_.empty()) return;

    engine_->GetDirectXCommon()->WaitForGPU();

    // 最前面のシーンの終了処理
    sceneStack_.back().scene->OnExit();
    sceneStack_.back().scene->Finalize();
    sceneStack_.pop_back();

    if (!sceneStack_.empty()) {
        // 次のシーンが最前面に復帰するためレジューム処理を行う
        sceneStack_.back().scene->OnResume();
        // ★全てのSE（効果音）とBGMを再開する
        engine_->GetAudioManager()->ResumeAll();
        engine_->SetCursorLocked(!sceneStack_.back().scene->IsCursorVisible());
    } else {
        engine_->SetCursorLocked(false);
    }
}

void SceneManager::TransitionTo(const Key& next, SceneTransition::Type type, float duration, EaseType easeType) {
    if (transitionPhase_ != TransitionPhase::None) return; // 二重遷移防止

    pendingTransition_ = next;
    pendingType_ = type;
    pendingDuration_ = duration;
    pendingEaseType_ = easeType;
    transitionPhase_ = TransitionPhase::Closing;

    engine_->GetSceneTransition()->Start(type, duration, true, easeType);
}

void SceneManager::LoadScene(const std::string& sceneJsonName, SceneTransition::Type type, float duration, EaseType easeType) {
    // 未登録の場合、DataDrivenScene を生成するファクトリを自動登録
    if (factories_.find(sceneJsonName) == factories_.end()) {
        Register(sceneJsonName, [sceneJsonName]() {
            return std::make_unique<DataDrivenScene>(sceneJsonName);
        });
    }
    
    // 通常の遷移処理に回す
    TransitionTo(sceneJsonName, type, duration, easeType);
}


bool SceneManager::UpdateLoadStatus() {
    bool isLoading = IsLoading();

    if (isLoading != wasLoading_) {
        if (isLoading) {
            // ロード中はマウスカーソルを表示させる
            engine_->SetCursorLocked(false);
        } else {
            // ロード完了時に、現在のシーン本来の設定に戻す
            if (!sceneStack_.empty()) {
                engine_->SetCursorLocked(!sceneStack_.back().scene->IsCursorVisible());
            }
        }
        wasLoading_ = isLoading;
    }
    return isLoading;
}

void SceneManager::ProcessImmediateTransition() {
    if (!pending_.empty()) {
        pendingTransition_ = pending_;
        StartAsyncInitialize(pending_); // 即時切替の場合も非同期初期化を開始する
        transitionPhase_ = TransitionPhase::Initializing;
        pending_.clear();
    }
}

void SceneManager::ProcessTransitionPhase(bool& isLoading) {
    if (transitionPhase_ == TransitionPhase::Closing) {
        // フェードアウト完了待ち
        if (engine_->GetSceneTransition()->IsOutFinished()) {
            // 新しいシーンに非同期で切り替え、裏ロード開始
            StartAsyncInitialize(pendingTransition_); 
            transitionPhase_ = TransitionPhase::Initializing; // 初期化フェーズへ
        }
    }
    else if (transitionPhase_ == TransitionPhase::Initializing) {
        // バックグラウンドでのシーン破棄・初期化完了待ち
        if (!isAsyncInitializing_.load()) {
            if (initFuture_.valid()) {
                initFuture_.get(); // 例外があればキャッチ
            }
            
            {
                std::lock_guard<std::mutex> lock(nextSceneMutex_);
                SceneStackItem item;
                item.name = pendingTransition_;
                item.scene = std::move(nextScene_);
                // ここではまだ呼ばない（ロード完了後に呼ぶ）
                sceneStack_.push_back(std::move(item));
            }
            
            pendingTransition_.clear(); // ロード完了後にクリアする
            isInitializing_ = false;
            
            // ポーズ可能なシーンかどうかに応じてマウスをロック
            if (!sceneStack_.empty()) {
                engine_->SetCursorLocked(!sceneStack_.back().scene->IsCursorVisible());
            }
            
            transitionPhase_ = TransitionPhase::LoadingWait; // ロード待機フェーズへ
        }
    }
    else if (transitionPhase_ == TransitionPhase::LoadingWait) {
        // 全てのアセットのロード完了を待つ (画面は真っ暗なまま、上にLoadingScreenが描画される)
        if (!engine_->IsAssetLoading()) {
            transitionPhase_ = TransitionPhase::Opening;
            
            // ---------------------------------------------------------
            // 【重要】フェードイン中 (Opening) は Update() がスキップされるため、
            // そのまま Draw() が呼ばれると、未初期化の定数バッファ（ゼロ行列）や
            // 前シーンのカメラデータが使われてしまい、深刻な点滅・描画崩れが発生する。
            // これを防ぐため、ロード完了直後に強制的に1回だけ Update() を回し、
            // 全オブジェクトの初回更新（UpdateAll等）とカメラ設定を済ませる。
            // ---------------------------------------------------------
            if (!sceneStack_.empty()) {
                sceneStack_.back().scene->OnEnter(); // ロード完了直後にBGM再生等を開始
                sceneStack_.back().scene->Update();
            }

            // ロードが完了した瞬間に、フェードインを開始する
            engine_->GetSceneTransition()->Start(pendingType_, pendingDuration_, false, pendingEaseType_);
        }
    }
    else if (transitionPhase_ == TransitionPhase::Opening) {
        // フェードイン完了待ち
        if (engine_->GetSceneTransition()->IsFinished()) {
            transitionPhase_ = TransitionPhase::None;
        }
    }

    // ロード中かどうかを最新の状態で再評価する（非同期初期化が開始された直後を考慮）
    isLoading = IsLoading();
}

void SceneManager::UpdateActiveScenes() {
    if (!sceneStack_.empty()) {
        // ※フェードイン中 (Opening) は Update を呼ばないよう制限
        if (transitionPhase_ != TransitionPhase::Opening) {
            // 上層（末尾）から順に、UpdateBlocking が true のシーンを見つける
            int updateStartIndex = static_cast<int>(sceneStack_.size()) - 1;
            for (int i = static_cast<int>(sceneStack_.size()) - 1; i >= 0; --i) {
                updateStartIndex = i;
                if (sceneStack_[i].scene->IsUpdateBlocking()) {
                    break;
                }
            }
            // 見つけたシーンから上層へ順番に Update を実行
            size_t initialSize = sceneStack_.size();
            for (int i = updateStartIndex; i < static_cast<int>(sceneStack_.size()); ++i) {
                sceneStack_[i].scene->Update();
                // Update中にPush/Popが行われた場合は、誤作動（同じ入力での即座の反応等）を防ぐためループを抜ける
                if (sceneStack_.size() != initialSize) {
                    break;
                }
            }
        }
    }
}

void SceneManager::Update() {
    // 1. ロード状況の確認とカーソル制御
    bool isLoading = UpdateLoadStatus();

    // 2. 即時シーン切り替え要求の処理
    ProcessImmediateTransition();

    // 3. 遷移プロセスの更新
    ProcessTransitionPhase(isLoading);

    // ロード中であれば、ここで更新を止めてLoadingUIだけアニメーションさせる
    if (isLoading) {
        return;
    }

    // 4. ロード完了時のみ、シーン自体のUpdateを回す
    UpdateActiveScenes();
}

void SceneManager::Draw() {
    // ロード待ちの場合は背景のみ(UIはIrufemiEngine側で描画される)
    if (IsLoading()) {
        return;
    }

    if (!sceneStack_.empty()) {
        // 上層（末尾）から順に、DrawBlocking が true のシーンを見つける
        int drawStartIndex = static_cast<int>(sceneStack_.size()) - 1;
        for (int i = static_cast<int>(sceneStack_.size()) - 1; i >= 0; --i) {
            drawStartIndex = i;
            if (sceneStack_[i].scene->IsDrawBlocking()) {
                break;
            }
        }
        // 見つけたシーンから上層へ順番に Draw を実行
        for (int i = drawStartIndex; i < static_cast<int>(sceneStack_.size()); ++i) {
            sceneStack_[i].scene->Draw();
        }
    }
}

IScene* SceneManager::GetCurrentScene() const {
    return sceneStack_.empty() ? nullptr : sceneStack_.back().scene.get();
}

const SceneManager::Key& SceneManager::GetCurrent() const { 
    static const Key emptyKey = "";
    return sceneStack_.empty() ? emptyKey : sceneStack_.back().name; 
}

// 並び順は登録順
std::vector<SceneManager::Key> SceneManager::GetRegisteredKeys() const { return order_; }

bool SceneManager::IsLoading() const {
    return (transitionPhase_ == TransitionPhase::Initializing) || engine_->IsAssetLoading();
}

void SceneManager::StartAsyncInitialize(const Key& next) {
    auto it = factories_.find(next);
    if (it == factories_.end()) { return; }

    Factory factory = it->second;
    isAsyncInitializing_.store(true);
    isInitializing_ = true;
    wasLoading_ = false;
    engine_->SetTimeScale(1.0f); // 時間の進みをリセット
    
    // --- 【重要】---
    // シーン破棄前に、現在溜まっている描画・コンピュートタスクを破棄する。
    engine_->GetDrawManager()->ClearAllQueues();
    
    // GPU処理の完了を待つ (リソース解放中のアクセス違反を防ぐ)
    engine_->GetDirectXCommon()->WaitForGPU();
    
    // 現在のシーンスタックの全シーンを終了させて破棄（メインスレッドで実行）
    for (auto it = sceneStack_.rbegin(); it != sceneStack_.rend(); ++it) {
        it->scene->OnExit();
        it->scene->Finalize();
    }
    sceneStack_.clear();

    // シーン切り替え時にポストプロセスのパラメータを自動リセット
    if (engine_->GetPostProcessManager()) {
        engine_->GetPostProcessManager()->ResetAllParams();
    }
    
    initFuture_ = std::async(std::launch::async, [this, factory, next]() {
        // 新しいシーンを生成
        auto newScene = factory();

        // データがあればロード
        bool hasData = SceneSerializer::Load(newScene.get(), next);

        // 初期化（裏ロード）
        newScene->Initialize(engine_);

        // データがなくてエディタモードなら、初期状態を自動生成
#ifdef EditorMode
        if (!hasData) {
            SceneSerializer::Save(newScene.get(), next);
        }
#endif
        
        // メインスレッドへ渡すための準備
        {
            std::lock_guard<std::mutex> lock(nextSceneMutex_);
            nextScene_ = std::move(newScene);
        }
        
        isAsyncInitializing_.store(false);
    });
}