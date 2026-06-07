#include "LevelDataLoader.h"
#include <fstream>
#include <cassert>
#include <iostream>

#include <nlohmann/json.hpp>

#include "Framework/BaseScene.h"
#include "Framework/GameObject.h"
#include "Framework/Component/TransformComponent.h"
// ※必要に応じてModelComponentやStaticModelObject等のコンポーネントをインクルードする
// #include "Framework/Component/Renderer/PrimitiveRendererComponent.h"

void LevelDataLoader::LoadAndDeploy(BaseScene* scene, const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        assert(false && "Failed to open level data file.");
        return;
    }

    nlohmann::json deserialized;
    file >> deserialized;

    assert(deserialized.is_object());
    assert(deserialized.contains("name"));
    assert(deserialized["name"].is_string());

    std::string name = deserialized["name"].get<std::string>();
    assert(name.compare("scene") == 0);

    LevelData levelData;

    // "objects"の全オブジェクトを走査
    if (deserialized.contains("objects")) {
        for (const nlohmann::json& object : deserialized["objects"]) {
            if (object.contains("disabled")) {
                bool disabled = object["disabled"].get<bool>();
                if (disabled) {
                    continue;
                }
            }

            LevelData::ObjectData objectData;
            ParseObjectRecursive(object, objectData);
            levelData.objects.push_back(objectData);
        }
    }

    // 既存の LevelRoot があれば破棄する（再ロード対応）
    for (const auto& obj : scene->GetGameObjects()) {
        if (obj && obj->GetName() == "LevelRoot") {
            obj->Destroy();
        }
    }

    // 新しい LevelRoot を生成し、シリアライズ（保存）対象外にする
    auto levelRoot = std::make_shared<GameObject>("LevelRoot");
    levelRoot->SetDontSave(true);
    scene->AddGameObject(levelRoot);

    // パースしたデータをもとにシーンにデプロイ
    for (const auto& objectData : levelData.objects) {
        DeployObjectRecursive(scene, objectData, levelRoot);
    }
}

void LevelDataLoader::ParseObjectRecursive(const nlohmann::json& jsonObject, LevelData::ObjectData& outData) {
    assert(jsonObject.contains("type"));
    outData.type = jsonObject["type"].get<std::string>();

    if (jsonObject.contains("name")) {
        outData.name = jsonObject["name"].get<std::string>();
    }

    if (jsonObject.contains("file_name")) {
        outData.fileName = jsonObject["file_name"].get<std::string>();
    }

    // トランスフォームのパラメータ読み込み
    if (jsonObject.contains("transform")) {
        const nlohmann::json& transform = jsonObject["transform"];
        
        // 平行移動 (Blender -> Game: YとZを入れ替え)
        outData.translation.x = (float)transform["translation"][0];
        outData.translation.y = (float)transform["translation"][2];
        outData.translation.z = (float)transform["translation"][1];

        // 回転角 (Blender -> Game: マイナスをつける)
        outData.rotation.x = -(float)transform["rotation"][0];
        outData.rotation.y = -(float)transform["rotation"][2];
        outData.rotation.z = -(float)transform["rotation"][1];

        // スケーリング
        outData.scaling.x = (float)transform["scaling"][0];
        outData.scaling.y = (float)transform["scaling"][2];
        outData.scaling.z = (float)transform["scaling"][1];
    } else {
        outData.translation = { 0.0f, 0.0f, 0.0f };
        outData.rotation = { 0.0f, 0.0f, 0.0f };
        outData.scaling = { 1.0f, 1.0f, 1.0f };
    }

    // 子オブジェクトの走査
    if (jsonObject.contains("children")) {
        for (const nlohmann::json& child : jsonObject["children"]) {
            if (child.contains("disabled")) {
                bool disabled = child["disabled"].get<bool>();
                if (disabled) {
                    continue;
                }
            }

            LevelData::ObjectData childData;
            ParseObjectRecursive(child, childData);
            outData.children.push_back(childData);
        }
    }
}

std::shared_ptr<GameObject> LevelDataLoader::DeployObjectRecursive(BaseScene* scene, const LevelData::ObjectData& objectData, const std::shared_ptr<GameObject>& parent) {
    // 新しいGameObjectの生成
    auto newObject = std::make_shared<GameObject>(objectData.name);
    
    // TransformComponentの取得、なければ追加
    auto transform = newObject->GetComponent<TransformComponent>();
    if (!transform) {
        transform = newObject->AddComponent<TransformComponent>().get();
    }

    transform->position_ = objectData.translation;
    // Blenderの回転角(度数法)をエンジンの弧度法(Radian)に変換
    transform->rotation_.x = Math::ToRadians(objectData.rotation.x);
    transform->rotation_.y = Math::ToRadians(objectData.rotation.y);
    transform->rotation_.z = Math::ToRadians(objectData.rotation.z);
    transform->scale_ = objectData.scaling;

    // 親子関係の設定
    if (parent) {
        newObject->SetParent(parent);
    } else {
        // ルートオブジェクトのみシーンに追加
        scene->AddGameObject(newObject);
    }

    // --- オブジェクトのタイプに応じたコンポーネントの付与 ---
    if (objectData.type == "MESH") {
        auto mesh = newObject->AddComponent<MeshRendererComponent>();
        // fileName プロパティがあればそのモデルを読み込む
        if (!objectData.fileName.empty()) {
            mesh->LoadModel(objectData.fileName);
        }
    } else if (objectData.type == "CAMERA") {
        newObject->AddComponent<CameraComponent>();
    }

    // 子オブジェクトがあれば再帰的にデプロイ
    for (const auto& childData : objectData.children) {
        DeployObjectRecursive(scene, childData, newObject);
    }

    return newObject;
}
