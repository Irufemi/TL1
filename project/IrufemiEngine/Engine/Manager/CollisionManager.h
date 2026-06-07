#pragma once
#include <vector>
#include <set>
#include <memory>
#include <string>
#include <nlohmann/json.hpp>
#include "Renderer/LineInstanced/LineClass.h"
#include "Engine/Core/Math/Vector3.h"
#include "Engine/Core/Shape/LinePrimitive.h"

class ColliderComponent;
class GameObject;

/// @brief レイキャストの結果を格納する構造体
struct RaycastHit {
    bool isHit = false;
    GameObject* hitObject = nullptr;
    ColliderComponent* hitCollider = nullptr;
    Vector3 hitPoint;
    float distance = 0.0f;
};

/**
 * @class CollisionManager
 * @brief シーン内のすべての当たり判定を管理し、衝突判定を処理するマネージャ
 */
class CollisionManager {
public:
    static CollisionManager& GetInstance() {
        static CollisionManager instance;
        return instance;
    }

    /// @brief 初期化
    void Initialize();

    /// @brief 登録されたコライダーを全てクリアする（シーン切り替え時などに呼ぶ）
    void Clear();
    
    /// @brief コライダーを登録する
    void RegisterCollider(ColliderComponent* collider);
    
    /// @brief コライダーの登録を解除する
    void UnregisterCollider(ColliderComponent* collider);

    /// @brief 毎フレーム呼ばれ、登録された全ペアの判定を行う
    void CheckAllCollisions();

    /// @brief 全コライダーのデバッグ線を描画する
    void DrawDebug(GameObject* selectedObject = nullptr);

    /// @brief デバッグ描画フラグのポインタを取得する（ImGui用）
    bool* GetIsDrawDebugLinePtr() { return &isDrawDebugLine_; }

    // --- 動的レイヤー管理 ---
    void LoadLayers(const std::string& filepath);
    void SaveLayers(const std::string& filepath);
    std::vector<std::string>& GetLayerNames() { return layerNames_; }
    void AddLayer(const std::string& name);
    void RemoveLayer(int index);
    void RenameLayer(int index, const std::string& name);

    // --- レイキャスト ---
    /// @brief シーン内の全コライダーに対してレイを飛ばし、最も近いオブジェクトを返す
    /// @param ray 飛ばすレイ
    /// @param hitInfo 結果が格納される構造体
    /// @param maxDistance 判定する最大距離
    /// @param layerMask 判定対象とするレイヤーのビットマスク
    /// @param ignoreObject 判定から除外するオブジェクト（自分自身を無視するためなど）
    /// @return 何かに当たった場合はtrue
    bool Raycast(const Ray& ray, RaycastHit& hitInfo, float maxDistance = 1000.0f, uint32_t layerMask = 0xFFFFFFFF, GameObject* ignoreObject = nullptr);

    /// @brief デバッグ用のレイを描画キューに追加する
    void DrawDebugRay(const Ray& ray, float distance, const Vector4& color = {1,0,0,1});

private:
    CollisionManager() = default;
    ~CollisionManager();
    CollisionManager(const CollisionManager&) = delete;
    CollisionManager& operator=(const CollisionManager&) = delete;

    std::vector<ColliderComponent*> colliders_;
    std::unique_ptr<Line3DRegion> debugLine_;
    
    // レイヤー名（最大32個）
    std::vector<std::string> layerNames_;
    std::string layerConfigFilePath_ = "resources/config/layers.json";

    bool isDrawDebugLine_ = true;

    // Raycast描画キャッシュ
    struct DebugRayInfo {
        Ray ray;
        float distance;
        Vector4 color;
    };
    std::vector<DebugRayInfo> debugRays_;

    // 前フレームの衝突ペアを保持（Enter / Stay / Exit 用）
    std::set<std::pair<ColliderComponent*, ColliderComponent*>> previousCollisions_;
};
