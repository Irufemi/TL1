#include "DebrisManagerComponent.h"
#include "Framework/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Renderer/PrimitiveRendererComponent.h"
#include "Framework/SceneSerializer.h"
#include "Framework/BaseScene.h"
#include "DebrisComponent.h"
#include "Engine/IrufemiEngine.h"
#include "Engine/Platform/Input/InputManager.h"
#include "Renderer/Object3D/BaseModel/BaseModel.h"
#include "Engine/Core/Math/Random/Random.h"
#include "Engine/Graphics/Camera/CameraManager.h"
#include "Engine/Graphics/Camera/Camera.h"

void DebrisManagerComponent::OnRegisterProperties() {
    RegisterProperty("Pool Size", &poolSize_);
}

void DebrisManagerComponent::Initialize() {
    // Sceneへの追加を確実に行うため、プールの生成は最初のUpdateで行う
    isPoolInitialized_ = false;
}

void DebrisManagerComponent::Update() {
    if (!isPoolInitialized_) {
        // ガレキのプレハブを生成するファクトリ関数
        auto debrisFactory = [this]() -> std::shared_ptr<GameObject> {
            auto obj = std::make_shared<GameObject>("Debris");
            obj->AddComponent<TransformComponent>();
            
            // とりあえず目視できるようにキューブをアタッチ
            auto renderer = obj->AddComponent<PrimitiveRendererComponent>();
            renderer->SetShape(PrimitiveType::Cube);
            // 少し小さめに設定
            obj->GetComponent<TransformComponent>()->scale_ = { 0.5f, 0.5f, 0.5f };
            
            obj->AddComponent<DebrisComponent>();
            
            // プール内にある間は非アクティブにしておく
            obj->SetIsActive(false);
            
            // シーンの Update 中の配列破壊を防ぐため、Manager の子オブジェクトとして登録する
            if (gameObject_) {
                gameObject_->AddChild(obj);
            }
            return obj;
        };

        pool_ = std::make_unique<ObjectPool<GameObject>>(poolSize_, debrisFactory);
        isPoolInitialized_ = true;
    }

    auto input = BaseModel::GetIrufemiEngine()->GetInputManager();
    // デバッグ用: 1キーを押したら10個ランダムな場所にスポーンさせる
    if (input->IsKeyPressed('1')) {
        Vector3 spawnBase = {0.0f, 0.0f, 0.0f};
        Vector3 forward = {0.0f, 0.0f, 1.0f};
        Vector3 right = {1.0f, 0.0f, 0.0f};
        
        auto scene = gameObject_->GetScene();
        if (scene) {
            for (auto& obj : scene->GetGameObjects()) {
                if (obj && obj->GetName() == "Player") {
                    if (auto t = obj->GetComponent<TransformComponent>()) {
                        spawnBase = t->position_;
                        float yaw = t->rotation_.y;
                        forward = { std::sin(yaw), 0.0f, std::cos(yaw) };
                        right = { std::cos(yaw), 0.0f, -std::sin(yaw) };
                    }
                    break;
                }
            }
        }

        for (int i = 0; i < 10; ++i) {
            VirtualDebris vd;
            vd.id = nextVirtualId_++;
            
            float distFwd = Random::GeneratorFloat(30.0f, 80.0f);
            float distRight = Random::GeneratorFloat(-20.0f, 20.0f);
            float height = Random::GeneratorFloat(-5.0f, 15.0f);
            
            vd.position = {
                spawnBase.x + forward.x * distFwd + right.x * distRight,
                spawnBase.y + height,
                spawnBase.z + forward.z * distFwd + right.z * distRight
            };
            vd.isSpawned = false;
            vd.isDestroyed = false;
            vd.instance = nullptr;
            
            virtualDebrisList_.push_back(vd);
        }
        
        // --- プール枯渇の完全防止（古いデータの強制パージ） ---
        // 1キーを連打して仮想データがプール上限(poolSize_)を超えた場合、
        // もっとも古い（リスト先頭の）データを強制的に消去して実体を空ける
        while (virtualDebrisList_.size() > static_cast<size_t>(poolSize_)) {
            auto& oldVd = virtualDebrisList_.front();
            if (oldVd.isSpawned && oldVd.instance) {
                ReleaseDebris(oldVd.instance);
            }
            virtualDebrisList_.erase(virtualDebrisList_.begin());
        }
    }

    UpdateStreaming();
}

std::shared_ptr<GameObject> DebrisManagerComponent::AcquireDebris() {
    if (!pool_) return nullptr;
    auto obj = pool_->Acquire();
    if (obj) {
        obj->SetIsActive(true);
    }
    return obj;
}

void DebrisManagerComponent::ReleaseDebris(std::shared_ptr<GameObject> debris) {
    if (!pool_ || !debris) return;
    debris->SetIsActive(false);
    pool_->Release(debris);
}

void DebrisManagerComponent::UpdateStreaming() {
    auto activeCam = BaseModel::GetIrufemiEngine()->GetCameraManager()->GetActiveCamera();
    if (!activeCam) return;
    
    Vector3 camPos = activeCam->GetTranslate();
    Matrix4x4 viewMat = activeCam->GetViewMatrix();
    Vector3 camFwd = { viewMat.m[0][2], viewMat.m[1][2], viewMat.m[2][2] };
    
    float len = std::sqrt(camFwd.x*camFwd.x + camFwd.y*camFwd.y + camFwd.z*camFwd.z);
    if (len > 0.001f) {
        camFwd.x /= len; camFwd.y /= len; camFwd.z /= len;
    } else {
        camFwd = {0.0f, 0.0f, 1.0f};
    }
    
    const float SpawnDistanceSq = 150.0f * 150.0f; // 150m以内なら表示
    
    for (auto& vd : virtualDebrisList_) {
        if (vd.isDestroyed) continue;
        
        float dx = vd.position.x - camPos.x;
        float dy = vd.position.y - camPos.y;
        float dz = vd.position.z - camPos.z;
        float distSq = dx*dx + dy*dy + dz*dz;
        
        // カメラの前方方向への距離（内積）
        float dot = dx*camFwd.x + dy*camFwd.y + dz*camFwd.z;
        
        // 距離が150m以内で、かつカメラより後方60m以内に収まっている場合は表示
        bool inRange = (distSq <= SpawnDistanceSq && dot > -60.0f);
        
        if (inRange) {
            // 表示範囲内
            if (!vd.isSpawned) {
                vd.instance = AcquireDebris();
                if (vd.instance) {
                    vd.isSpawned = true;
                    auto transform = vd.instance->GetComponent<TransformComponent>();
                    if (transform) transform->position_ = vd.position;
                    
                    auto comp = vd.instance->GetComponent<DebrisComponent>();
                    if (comp) {
                        comp->Initialize();
                        comp->SetVirtualId(vd.id);
                        comp->SetManager(this);
                        comp->SetState(DebrisState::Idle);
                    }
                }
            }
        } else {
            // 表示範囲外
            if (vd.isSpawned) {
                bool canRelease = true;
                if (vd.instance) {
                    auto comp = vd.instance->GetComponent<DebrisComponent>();
                    if (comp && (comp->GetState() == DebrisState::Pulled || comp->GetState() == DebrisState::Orbiting || comp->GetState() == DebrisState::Thrown)) {
                        canRelease = false; // プレイヤーに保持されている、または投げられたものは消さない
                    }
                }
                
                if (canRelease) {
                    ReleaseDebris(vd.instance);
                    vd.instance = nullptr;
                    vd.isSpawned = false;
                }
            }
        }
    }
}

void DebrisManagerComponent::NotifyDestroyed(int id) {
    for (auto& vd : virtualDebrisList_) {
        if (vd.id == id) {
            vd.isDestroyed = true;
            vd.instance = nullptr;
            vd.isSpawned = false;
            break;
        }
    }
}
