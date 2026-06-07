#include "DebugEnemySpawnerComponent.h"
#include "Framework/GameObject.h"
#include "Framework/BaseScene.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Renderer/PrimitiveRendererComponent.h"
#include "Engine/IrufemiEngine.h"
#include "Engine/Platform/Input/InputManager.h"
#include "Renderer/Object3D/BaseModel/BaseModel.h"
#include "RailShooterEnemyComponent.h"
#include "Engine/Core/Math/Random/Random.h"

void DebugEnemySpawnerComponent::Initialize() {
}

void DebugEnemySpawnerComponent::Update() {
    auto input = BaseModel::GetIrufemiEngine()->GetInputManager();
    if (!input) return;

    // '2'キーで敵をスポーン
    if (input->IsKeyPressed('2')) {
        Vector3 spawnPos = {0.0f, 0.0f, 50.0f};

        auto scene = gameObject_->GetScene();
        if (scene) {
            for (auto& obj : scene->GetGameObjects()) {
                if (obj && obj->GetName() == "Player") {
                    if (auto transform = obj->GetComponent<TransformComponent>()) {
                        // プレイヤーの現在位置から Z軸前方に 50m、XYはランダムに散らす
                        spawnPos = transform->position_;
                        spawnPos.z += 50.0f;
                        spawnPos.x += Random::GeneratorFloat(-10.0f, 10.0f);
                        spawnPos.y += Random::GeneratorFloat(-5.0f, 5.0f);
                    }
                    break;
                }
            }
        }

        SpawnEnemy(spawnPos);
    }
}

void DebugEnemySpawnerComponent::SpawnEnemy(const Vector3& position) {
    auto scene = gameObject_->GetScene();
    if (!scene) return;

    auto enemy = std::make_shared<GameObject>("DebugEnemy");
    scene->AddGameObject(enemy);
    
    auto transform = enemy->AddComponent<TransformComponent>();
    transform->position_ = position;
    // プレイヤー側(Z負方向)を向くように回転
    transform->rotation_ = {0.0f, 3.14159f, 0.0f};
    transform->scale_ = {1.2f, 1.2f, 1.2f};

    auto renderer = enemy->AddComponent<PrimitiveRendererComponent>();
    // PrimitiveRendererの設定はデフォルトで一旦OK（表示されれば良い）

    auto enemyComp = enemy->AddComponent<RailShooterEnemyComponent>();
    enemyComp->Initialize(); // isAliveなど初期化
}
