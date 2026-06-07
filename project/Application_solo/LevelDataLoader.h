#pragma once

#include <string>
#include <memory>
#include "Core/Math/Vector3.h"
#include <nlohmann/json.hpp>

// MESHやCAMERA等のコンポーネント追加に必要なヘッダー
#include "Framework/Component/Renderer/MeshRendererComponent.h"
#include "Framework/Component/Camera/CameraComponent.h"
#include "Engine/Core/Math/MathFunction.h"

// 前方宣言
class BaseScene;
class GameObject;

/**
 * @struct LevelData
 * @brief パースしたJSONの階層構造を保持するデータ
 */
struct LevelData {
    struct ObjectData {
        std::string name;
        std::string type;
        std::string fileName;
        Vector3 translation;
        Vector3 rotation;
        Vector3 scaling;
        
        // 子オブジェクト
        std::vector<ObjectData> children;
    };

    std::vector<ObjectData> objects;
};

/**
 * @class LevelDataLoader
 * @brief レベルエディタから出力されたJSONを読み込み、シーンにオブジェクトを配置する
 */
class LevelDataLoader {
public:
    /**
     * @brief JSONファイルを読み込んでシーンにGameObject群を展開する
     * @param scene 配置対象のシーン
     * @param filepath JSONファイルのパス
     */
    static void LoadAndDeploy(BaseScene* scene, const std::string& filepath);

private:
    // 再帰的にJSONオブジェクトをパースする
    static void ParseObjectRecursive(const nlohmann::json& jsonObject, LevelData::ObjectData& outData);
    
    // 再帰的にGameObjectを生成して配置する
    static std::shared_ptr<GameObject> DeployObjectRecursive(BaseScene* scene, const LevelData::ObjectData& objectData, const std::shared_ptr<GameObject>& parent);
};
