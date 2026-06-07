#include "BaseScene.h"
#include "Engine/IrufemiEngine.h"
#include "Engine/Platform/Input/InputManager.h"
#include "Engine/Graphics/Camera/CameraManager.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Graphics/Camera/DebugCamera.h"
#include "Engine/Graphics/Data/CameraForGPU.h"
#include "Engine/Graphics/Data/PointLight.h"
#include "Engine/Graphics/Data/SpotLight.h"
#include "Engine/Graphics/Data/DirectionalLight.h"
#include "Engine/Graphics/Data/AreaLight.h"
#include "GameObject.h"
#include "Engine/Manager/CollisionManager.h"
#include "Engine/Manager/EditorManager.h"

#include "SceneSerializer.h"
#include "Component/TransformComponent.h"
#include <fstream>
#include <nlohmann/json.hpp>

#ifdef USE_IMGUI
#include "Engine/Manager/DebugUI.h"
#endif

BaseScene::BaseScene() = default;
BaseScene::~BaseScene() = default;

void BaseScene::Initialize(IrufemiEngine* engine) {
    engine_ = engine;

    // --- カメラマネージャーの初期化はエンジン側で行われるため、ここではメインカメラの登録のみ行う ---
    auto mainCamera = std::make_shared<Camera>();
    mainCamera->Initialize(engine_->GetClientWidth(), engine_->GetClientHeight());
    mainCamera->SetTranslate({ 0.0f, 0.0f, -50.0f });
    mainCamera->UpdateMatrix();
    engine_->GetCameraManager()->AddCamera("Main", mainCamera);

    debugCamera_ = std::make_unique<DebugCamera>();
    debugCamera_->Initialize(engine_->GetInputManager(), engine_->GetClientWidth(), engine_->GetClientHeight());

    // --- デフォルトライティングの初期化 ---
    directionalLight_ = std::make_unique<DirectionalLight>();
    directionalLight_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    directionalLight_->direction = { 0.5f, -0.7f, 1.0f };
    directionalLight_->intensity = 1.0f;
    
    CollisionManager::GetInstance().Initialize();
    CollisionManager::GetInstance().Clear();
}

void BaseScene::Update() {
    // デバッグカメラのトグル機能などをここに入れることも可能
    // 今回は各シーンが個別に実装しているケースを考慮し、Updateでのカメラ行列上書き処理を共通化
    if (isDebugCameraMode_) {
        debugCamera_->Update();
        Camera* activeCam = engine_->GetCameraManager()->GetActiveCamera();
        if (activeCam) {
            const Camera& dbgCam = debugCamera_->GetCamera();
            activeCam->SetViewMatrix(dbgCam.GetViewMatrix());
            activeCam->SetTranslate(dbgCam.GetTranslate());
            activeCam->SetPerspectiveFovMatrix(dbgCam.GetPerspectiveFovMatrix());
        }
    } else {
        engine_->GetCameraManager()->Update();
    }

    bool isPlayMode = true;
#ifdef EditorMode
    if (engine_ && engine_->GetEditorManager()) {
        isPlayMode = engine_->GetEditorManager()->IsPlayMode();
    }
#endif

    // GameObject の更新
    for (size_t i = 0; i < gameObjects_.size(); ++i) {
        auto obj = gameObjects_[i];
        if (obj && !obj->GetParent() && !obj->IsDestroyed()) {
            obj->Update(isPlayMode);
        }
    }
    
    // PlayMode 時のみ衝突判定（イベント発火など）を行う
    if (isPlayMode) {
        CollisionManager::GetInstance().CheckAllCollisions();
    }

    // 破棄フラグが立ったオブジェクトを一括削除 (GC)
    gameObjects_.erase(std::remove_if(gameObjects_.begin(), gameObjects_.end(),
        [](const std::shared_ptr<GameObject>& obj) {
            return !obj || obj->IsDestroyed();
        }), gameObjects_.end());

    SubmitFrameData();
}

void BaseScene::Draw() {
    // GameObject の描画
    for (size_t i = 0; i < gameObjects_.size(); ++i) {
        auto obj = gameObjects_[i];
        if (obj && !obj->GetParent()) obj->Draw();
    }
    
#ifdef EditorMode
    GameObject* selectedObj = nullptr;
    if (engine_ && engine_->GetEditorManager()) {
        auto sel = engine_->GetEditorManager()->GetSelectedObject();
        if (sel) {
            selectedObj = sel.get();
        }
    }
    
    // 選択中のオブジェクトに対してアウトラインマスク用の描画コマンドを発行
    if (selectedObj) {
        selectedObj->DrawOutlineMask();
    }
    
    CollisionManager::GetInstance().DrawDebug(selectedObj);
#else
    CollisionManager::GetInstance().DrawDebug();
#endif
}

void BaseScene::AddGameObject(std::shared_ptr<GameObject> obj) {
    if (obj) {
        obj->SetScene(this);
        gameObjects_.push_back(obj);
    }
}

void BaseScene::InsertGameObject(std::shared_ptr<GameObject> obj, size_t index) {
    if (!obj) return;
    obj->SetScene(this);
    if (index >= gameObjects_.size()) {
        gameObjects_.push_back(obj);
    } else {
        gameObjects_.insert(gameObjects_.begin() + index, obj);
    }
}

void BaseScene::RemoveGameObject(std::shared_ptr<GameObject> obj) {
    auto it = std::find(gameObjects_.begin(), gameObjects_.end(), obj);
    if (it != gameObjects_.end()) {
        gameObjects_.erase(it);
    }
}

void BaseScene::ClearGameObjects() {
    gameObjects_.clear();
}

size_t BaseScene::GetGameObjectIndex(std::shared_ptr<GameObject> obj) const {
    auto it = std::find(gameObjects_.begin(), gameObjects_.end(), obj);
    if (it != gameObjects_.end()) {
        return std::distance(gameObjects_.begin(), it);
    }
    return (size_t)-1;
}

void BaseScene::SubmitFrameData() {
    Camera* activeCam = engine_->GetCameraManager()->GetActiveCamera();
    if (!activeCam) return;

    CameraForGPU cameraForGpu;
    cameraForGpu.view = activeCam->GetViewMatrix();
    cameraForGpu.projection = activeCam->GetPerspectiveFovMatrix();
    cameraForGpu.worldPosition = activeCam->GetTranslate();

    std::vector<PointLight*> pLights;
    for (const auto& light : pointLights_) {
        pLights.push_back(light.get());
    }
    std::vector<SpotLight*> sLights;
    for (const auto& light : spotLights_) {
        sLights.push_back(light.get());
    }
    std::vector<AreaLight*> aLights;
    for (const auto& light : areaLights_) {
        aLights.push_back(light.get());
    }

    if (directionalLight_) {
        engine_->GetDrawManager()->SetFrameData(cameraForGpu, engine_->GetTotalTime(), engine_->GetDeltaTime(), *directionalLight_, pLights, sLights, aLights);
    }
}

void BaseScene::DrawDebugTab() {
#ifdef USE_IMGUI
    if (ImGui::BeginTabItem("Camera & Lights")) {
        ImGui::Checkbox("Debug Camera Mode", &isDebugCameraMode_);
        if (isDebugCameraMode_ && debugCamera_) {
            Camera* activeCam = engine_->GetCameraManager()->GetActiveCamera();
            if (activeCam) {
                if (ImGui::Button("Top-Down")) debugCamera_->SetPreset(DebugCamera::Preset::TopDown, *activeCam);
                ImGui::SameLine();
                if (ImGui::Button("Diagonal")) debugCamera_->SetPreset(DebugCamera::Preset::Diagonal, *activeCam);
                ImGui::SameLine();
                if (ImGui::Button("Front")) debugCamera_->SetPreset(DebugCamera::Preset::Front, *activeCam);
                ImGui::SameLine();
                if (ImGui::Button("Snap to Current")) debugCamera_->SetPreset(DebugCamera::Preset::Current, *activeCam);
            }
            ImGui::Separator();
            ImGui::Text("Debug Camera Controls");
            debugCamera_->GetCamera().DrawDebugContents();
            float dist = debugCamera_->GetDistance();
            if (ImGui::DragFloat("Orbit Distance", &dist, 0.1f, 1.0f, 1000.0f)) {
                debugCamera_->SetDistance(dist);
            }
        } else {
            Camera* activeCam = engine_->GetCameraManager()->GetActiveCamera();
            if (activeCam) {
                activeCam->DrawDebugContents();
            }
        }
        ImGui::EndTabItem();
    }
    DebugUI::DebugLights(directionalLight_.get(), pointLights_, spotLights_, areaLights_);
#endif
}

// ── 入力ヘルパ ──
bool BaseScene::DownVK(uint8_t vk) const { return engine_->GetInputManager()->IsKeyDown(vk); }
bool BaseScene::PressedVK(uint8_t vk) const { return engine_->GetInputManager()->IsKeyPressed(vk); }
bool BaseScene::ReleasedVK(uint8_t vk) const { return engine_->GetInputManager()->IsKeyReleased(vk); }

std::shared_ptr<GameObject> BaseScene::InstantiatePrefab(const std::string& prefabPath, const Vector3& position) {
    auto obj = SceneSerializer::LoadPrefab(prefabPath);
    if (obj) {
        if (auto transform = obj->GetComponent<TransformComponent>()) {
            transform->position_ = position;
        }
        AddGameObject(obj);
    }
    return obj;
}


bool BaseScene::DownDIK(uint8_t dik) const { return engine_->GetInputManager()->IsKeyDownDIK(dik); }
bool BaseScene::PressedDIK(uint8_t dik) const { return engine_->GetInputManager()->IsKeyPressedDIK(dik); }
bool BaseScene::ReleasedDIK(uint8_t dik) const { return engine_->GetInputManager()->IsKeyReleasedDIK(dik); }

bool BaseScene::IsButtonDown(unsigned short button) const { return engine_->GetInputManager()->IsButtonDown(button); }
bool BaseScene::IsButtonPressed(unsigned short button) const { return engine_->GetInputManager()->IsButtonPressed(button); }
