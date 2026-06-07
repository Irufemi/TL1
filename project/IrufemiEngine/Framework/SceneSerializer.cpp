#include "SceneSerializer.h"
#include "IScene.h"
#include "BaseScene.h"
#include "GameObject.h"
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

bool SceneSerializer::Save(IScene* scene, const std::string& sceneName) {
    if (!scene) return false;

    nlohmann::json rootArray = nlohmann::json::array();
    const auto& gameObjects = scene->GetGameObjects();

    for (const auto& obj : gameObjects) {
        // 親がいない（ルートの）オブジェクトのみをシリアライズ（子は再帰的に処理される想定）
        if (obj && !obj->GetParent()) {
            rootArray.push_back(obj->Serialize());
        }
    }

    std::string path = GetSceneFilePath(sceneName);
    
    // ディレクトリ作成
    fs::path dir = fs::path(path).parent_path();
    if (!fs::exists(dir)) {
        fs::create_directories(dir);
    }

    std::ofstream file(path);
    if (file.is_open()) {
        file << rootArray.dump(4);
        file.close();
        return true;
    }

    return false;
}

bool SceneSerializer::Load(IScene* scene, const std::string& sceneName) {
    if (!scene) return false;

    std::string path = GetSceneFilePath(sceneName);
    if (!fs::exists(path)) return false;

    std::ifstream file(path);
    if (!file.is_open()) return false;

    nlohmann::json rootArray;
    try {
        file >> rootArray;
    } catch (...) {
        return false;
    }
    file.close();

    // BaseScene の場合は既存のオブジェクトをクリア（オプション）
    // 現状は AddGameObject を通じて追加していく
    if (rootArray.is_array()) {
        for (const auto& j : rootArray) {
            auto obj = std::make_shared<GameObject>();
            obj->Deserialize(j);
            obj->Initialize();
            
            // BaseScene にキャストして追加を試みる
            if (auto* baseScene = dynamic_cast<BaseScene*>(scene)) {
                baseScene->AddGameObject(obj);
            }
        }
        return true;
    }

    return false;
}

bool SceneSerializer::Exists(const std::string& sceneName) {
    return fs::exists(GetSceneFilePath(sceneName));
}

bool SceneSerializer::SavePrefab(std::shared_ptr<GameObject> obj, const std::string& filepath) {
    if (!obj) return false;

    // 単一のオブジェクトをシリアライズ
    nlohmann::json root = obj->Serialize();

    fs::path dir = fs::path(filepath).parent_path();
    if (!fs::exists(dir)) {
        fs::create_directories(dir);
    }

    std::ofstream file(filepath);
    if (file.is_open()) {
        file << root.dump(4);
        file.close();
        return true;
    }

    return false;
}

std::shared_ptr<GameObject> SceneSerializer::LoadPrefab(const std::string& filepath) {
    if (!fs::exists(filepath)) return nullptr;

    std::ifstream file(filepath);
    if (!file.is_open()) return nullptr;

    nlohmann::json root;
    try {
        file >> root;
    } catch (...) {
        return nullptr;
    }
    file.close();

    auto obj = std::make_shared<GameObject>();
    obj->Deserialize(root);
    obj->Initialize();

    return obj;
}

std::string SceneSerializer::GetSceneFilePath(const std::string& sceneName) {
    return "resources/scenes/" + sceneName + ".json";
}
