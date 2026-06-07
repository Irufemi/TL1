#pragma once

#include "../../../Engine/Core/Math/Vector2.h"
#include "../../../Engine/Core/Math/Vector3.h"
#include "../../../Engine/Core/Math/Vector4.h"
#include "../../../Engine/Core/Math/Matrix4x4.h"
#include "../../../Engine/Graphics/Data/VertexData.h"
#include "ModelData.h"
#include "Node.h"             
#include "../../../Engine/Core/Math/Math.h"
#include "JointWeightData.h"
#include "../../../Engine/Core/Shape/Sphere.h"
#include "../../../Engine/Core/Math/Geometry/AABB.h"
#include <string>
#include <vector>
#include <map>

struct ObjMaterial {
    // Kd
    Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
    // Ka
    Vector3 ambient = { 0.1f, 0.1f, 0.1f };
    // Ks
    Vector3 specular = { 1.0f, 1.0f, 1.0f };
    
    float roughness = 0.5f;  
    float metallic = 0.0f;
    float alpha = 1.0f;    

    bool enableLighting = true;

    // ライティングモード (0:None, 1:Lambert, 2:Half-Lambert, 3:PBR)
    int32_t lightingMode = 3;

    // サンプラー設定 (0:WRAP, 1:CLAMP)
    int32_t useClampSampler = 0;

    // 環境マップの映り込み係数
    float environmentCoefficient = 0.0f;

    // アルファテスト用閾値 (0.0f = すべて通す, 1.0f = すべて棄却)
    float alphaReference = 0.5f;

    Matrix4x4 uvTransform = Math::MakeIdentity4x4();

    std::string textureFilePath = "";
    std::string normalMapFilePath = "";
};

struct ObjMesh {

    std::vector<VertexData> vertices;
    std::vector<uint32_t> indices; // 追加
    ObjMaterial material;
    std::string nodeName; // 追加: このメッシュが属するノード名
};

// 階層(Node)を統合した拡張版 ObjModel
struct ObjModel {
    std::vector<ObjMesh> meshes;
    Node rootNode; // 追加: シーン階層ルート
    std::map<std::string, JointWeightData> skinClusterData;
    Sphere boundingSphere; // 追加: モデル全体の境界球
    AABB boundingBox; // 追加: モデル全体のローカルAABB（高精度ピッキング用）
};