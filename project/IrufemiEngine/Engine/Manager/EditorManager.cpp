#include "EditorManager.h"

#ifdef EditorMode
#include <filesystem>
#include "imgui/imgui.h"
#include "Engine/IrufemiEngine.h"
#include "Engine/Graphics/DirectX/RenderTexture.h"
#include "Framework/SceneManager.h"
#include "Framework/IScene.h"
#include "Framework/GameObject.h"
#include "Framework/BaseScene.h"
#include "Framework/SceneSerializer.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Renderer/PrimitiveRendererComponent.h"
#include "Framework/Component/Renderer/MeshRendererComponent.h"
#include "Framework/Component/Renderer/SpriteRendererComponent.h"
#include "Engine/Manager/CollisionManager.h"
#include "Engine/Manager/DrawManager.h"
#include "Engine/Graphics/Pipeline/RenderGraph/RenderGraph.h"

// 分離したエディタパネル群
#include "Engine/Editor/IEditorPanel.h"
#include "Engine/Editor/Panel/SceneViewPanel.h"
#include "Engine/Editor/Panel/HierarchyPanel.h"
#include "Engine/Editor/Panel/InspectorPanel.h"
#include "Engine/Editor/Panel/ProjectBrowserPanel.h"

// Editor Core
#include "Engine/Editor/Core/EditorActionManager.h"
#include "Engine/Editor/Core/EditorShortcutManager.h"
#include "Engine/Editor/Core/ComponentEditorRegistry.h"

// FontAwesome 用のヘッダーを含める
#include "../../EngineResources/FontAwesome/IconsFontAwesome6.h"

EditorManager::EditorManager() = default;
EditorManager::~EditorManager() = default;

void EditorManager::Initialize(IrufemiEngine* engine) {
    engine_ = engine;

    actionManager_ = std::make_unique<EditorActionManager>(this);
    shortcutManager_ = std::make_unique<EditorShortcutManager>(this, actionManager_.get());

    componentEditorRegistry_ = std::make_unique<ComponentEditorRegistry>();
    componentEditorRegistry_->RegisterAllEditors();

    // 各パネルの生成と初期化
    panels_.push_back(std::make_unique<SceneViewPanel>());
    panels_.push_back(std::make_unique<HierarchyPanel>());
    panels_.push_back(std::make_unique<InspectorPanel>());
    panels_.push_back(std::make_unique<ProjectBrowserPanel>());

    for (auto& panel : panels_) {
        panel->Initialize(this);
    }
}

void EditorManager::Update() {
    if (shortcutManager_) {
        shortcutManager_->Update();
    }
}

void EditorManager::EnterPlayMode() {
    if (!engine_ || !engine_->GetSceneManager()) return;
    auto scene = engine_->GetSceneManager()->GetCurrentScene();
    if (!scene) return;

    std::string currentSceneName = engine_->GetSceneManager()->GetCurrent();
    
    // Play押下時に現在のシーンを実ファイルにも保存する
    if (!currentSceneName.empty()) {
        SceneSerializer::Save(scene, currentSceneName);
    }

    // 現在のシーン状態をバックアップ
    SceneSerializer::Save(scene, ".temp_playmode");
    playModeStartSceneName_ = currentSceneName; // 開始時のシーンを記憶
    currentMode_ = EditorModeState::Play;
}

void EditorManager::ExitPlayMode() {
    if (!engine_ || !engine_->GetSceneManager()) return;
    
    std::string currentSceneName = engine_->GetSceneManager()->GetCurrent();

    // プレイモード中にシーンが変わっていた場合は元のシーンに戻す
    if (!playModeStartSceneName_.empty() && currentSceneName != playModeStartSceneName_) {
        engine_->GetSceneManager()->TransitionTo(playModeStartSceneName_, SceneTransition::Type::Fade, 0.0f);
        // 非同期でシーンが切り替わるため、この時点での復元は一旦諦めるか、SceneManagerのコールバックで処理する必要がある
        // 今回はとりあえず元のシーンのファイルをロードし直すことで「保存された状態」に戻るようにする
        currentMode_ = EditorModeState::Edit;
        return;
    }

    auto scene = engine_->GetSceneManager()->GetCurrentScene();
    if (!scene) return;

    // プレイモード中の選択状態をクリア
    ClearSelectedObject();

    if (auto baseScene = dynamic_cast<BaseScene*>(scene)) {
        baseScene->ClearGameObjects();
    }
    
    // バックアップから復元
    SceneSerializer::Load(scene, ".temp_playmode");
    currentMode_ = EditorModeState::Edit;
}

void EditorManager::DrawEditorUI() {
    if (!engine_ || !engine_->GetMainRenderTexture()) return;

    Update(); // ここでショートカット処理を呼ぶ

    // 1. 全画面を覆う DockSpace の背景ウィンドウを作成
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    
    // 背景ウィンドウの装飾を全て消すフラグ
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking | 
                                   ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | 
                                   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | 
                                   ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("Editor DockSpace", nullptr, windowFlags);
    ImGui::PopStyleVar(3);

    // DockSpace 機能の起動
    ImGuiID dockspaceId = ImGui::GetID("MyDockSpace");
    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    // メニューバー
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            // 現在のシーン名を取得して保存/読込
            std::string currentSceneName = "";
            if (engine_ && engine_->GetSceneManager()) {
                currentSceneName = engine_->GetSceneManager()->GetCurrent();
            }

            if (ImGui::MenuItem("Save Scene")) {
                if (engine_ && engine_->GetSceneManager() && !currentSceneName.empty()) {
                    SceneSerializer::Save(engine_->GetSceneManager()->GetCurrentScene(), currentSceneName);
                }
            }
            if (ImGui::MenuItem("Load Scene")) {
                if (engine_ && engine_->GetSceneManager() && !currentSceneName.empty()) {
                    auto* scene = engine_->GetSceneManager()->GetCurrentScene();
                    if (scene) {
                        // 既存のオブジェクトを消してからロードしたい場合はここで処理が必要
                        SceneSerializer::Load(scene, currentSceneName);
                        selectedObject_.reset(); // ロードしたら選択を解除
                    }
                }
            }

            if (ImGui::MenuItem("Exit")) {
                // 終了処理（PostQuitMessage）
                PostQuitMessage(0);
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Window")) {
            if (ImGui::BeginMenu("Layout")) {
                if (ImGui::MenuItem("Load Default Layout")) {
                    const char* presetPath = "../IrufemiEngine/EngineResources/default_imgui.ini";
                    if (std::filesystem::exists(presetPath)) {
                        ImGui::LoadIniSettingsFromDisk(presetPath);
                    }
                }
                if (ImGui::MenuItem("Save Current as Default")) {
                    ImGui::SaveIniSettingsToDisk(ImGui::GetIO().IniFilename);
                    const char* presetPath = "../IrufemiEngine/EngineResources/default_imgui.ini";
                    if (std::filesystem::exists("imgui.ini")) {
                        std::error_code ec;
                        std::filesystem::copy_file("imgui.ini", presetPath, std::filesystem::copy_options::overwrite_existing, ec);
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // ツールバー (Play / Stop)
    ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize);
    if (currentMode_ == EditorModeState::Edit) {
        if (ImGui::Button(ICON_FA_PLAY " Play")) {
            EnterPlayMode();
        }
    } else {
        if (ImGui::Button(ICON_FA_STOP " Stop")) {
            ExitPlayMode();
        }
    }
    ImGui::End();

    // 2. 各種エディタパネルの描画
    for (auto& panel : panels_) {
        panel->Draw();
    }

#ifdef USE_IMGUI
    // 描画呼び出しをDebugUI.cppに移動しました
#endif // USE_IMGUI

    ImGui::End(); // Editor DockSpace
}

#endif // EditorMode
