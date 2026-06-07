#pragma once

#include "../Graphics/Data/VertexData.h"
#include "../Core/Type/PrimitiveType.h"

#include "../Core/Pattern/Singleton.h"
#include <cstdint>
#include <vector>
#include <map>
#include <string>
#include <memory>
#include <d3d12.h>
#include <wrl.h>

struct PrimitiveData {
    std::vector<VertexData> vertices;
    std::vector<uint32_t> indices;
};

struct PrimitiveResource {
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW indexBufferView;
    uint32_t indexCount;
};

/**
 * @struct RingParams
 * @brief Ring（ドーナツ型・三日月型）形状生成のための詳細パラメータ構造体
 */
struct RingParams {
    float innerRadius = 0.2f;            ///< 内径
    float startOuterRadius = 1.0f;       ///< 開始地点の外径
    float endOuterRadius = 1.0f;         ///< 終了地点の外径
    float startAngle = 0.0f;             ///< 開始角度(度数法)
    float endAngle = 360.0f;             ///< 終了角度(度数法)
    uint32_t segments = 32;              ///< 分割数
    bool verticalUV = false;             ///< UVをV方向に変更するかどうか
    Vector4 innerColor = {1.0f, 1.0f, 1.0f, 1.0f}; ///< 内側の頂点カラー
    Vector4 outerColor = {1.0f, 1.0f, 1.0f, 1.0f}; ///< 外側の頂点カラー
    float startAlpha = 0.0f;             ///< 開始地点のアルファ値（フェード用）
    float endAlpha = 0.0f;               ///< 終了地点のアルファ値（フェード用）
    float fadeRangeAngle = 0.0f;         ///< フェードにかかる角度の範囲(度数法)
};

/**
 * @class PrimitiveManager
 * @brief プリミティブ形状（球、立方体、平面等）のメッシュデータを管理するクラス
 * @details 頻繁に使用される標準的な形状の CPU データおよび GPU リソース（頂点/インデックスバッファ）をキャッシュし、
 *          効率的な再利用を可能にします。
 */
class PrimitiveManager : public Singleton<PrimitiveManager>
{
    friend class Singleton<PrimitiveManager>;

public:
    /** @name キャッシュデータの取得 */
    ///@{
    /**
     * @brief 指定した形状のプリミティブデータを取得する（CPUキャッシュ）
     * @param[in] type 形状のタイプ
     * @return 頂点とインデックスのリストを含む PrimitiveData
     */
    const PrimitiveData& GetPrimitiveData(PrimitiveType type);

    /**
     * @brief 指定した形状の頂点データのみを取得する
     */
    const std::vector<VertexData>& GetVertices(PrimitiveType type);

    /**
     * @brief 指定した形状の GPU リソース（BufferView）を取得する（GPUキャッシュ）
     * @details 標準設定（サイズ1.0等）のバッファを共有します。
     */
    const PrimitiveResource& GetStandardResource(PrimitiveType type);

    /**
     * @brief シリンダー形状専用の GPU リソースを取得する（蓋の有無を考慮したGPUキャッシュ）
     * @param[in] hasTop 上蓋を描画するか
     * @param[in] hasBottom 下蓋を描画するか
     * @return 蓋の有無に対応する PrimitiveResource
     */
    const PrimitiveResource& GetCylinderResource(bool hasTop, bool hasBottom);
    ///@}

    /** @name 形状生成メソッド（静的） */
    ///@{
    // 個別生成用（キャッシュしない。特殊なパラメータが必要な場合用）
    static PrimitiveData CreateSphere(float radius, uint32_t subdivision);
    static PrimitiveData CreateCube(float width, float height, float depth);
    static PrimitiveData CreateCylinder(float bottomRadius, float topRadius, float height, uint32_t segments, bool hasTop = true, bool hasBottom = true, bool centered = true);
    static PrimitiveData CreateCylinder(float radius, float height, uint32_t segments, bool hasTop = true, bool hasBottom = true);
    static PrimitiveData CreateCone(float radius, float height, uint32_t segments);
    static PrimitiveData CreateTorus(float majorRadius, float minorRadius, uint32_t majorSegments, uint32_t minorSegments);
    static PrimitiveData CreateIcoSphere(float radius, uint32_t subdivision);
    static PrimitiveData CreateGrid(float width, float height, uint32_t xSegments, uint32_t ySegments);
    static PrimitiveData CreateRing(const RingParams& params);
    static PrimitiveData CreateRing(float innerRadius, float outerRadius, float startAngle, float endAngle, uint32_t segments, bool verticalUV);
    static PrimitiveData CreatePlane(float width = 1.0f, float height = 1.0f);
    static PrimitiveData CreateTriangle();
    static PrimitiveData CreateTetra();
    static PrimitiveData CreateCircle(float radius, uint32_t segments);
    ///@}

private:
    PrimitiveManager() = default;
    ~PrimitiveManager() = default;
    PrimitiveManager(const PrimitiveManager&) = delete;
    PrimitiveManager& operator=(const PrimitiveManager&) = delete;

public:
    /**
     * @brief GPUリソースの生成補助
     */
    void CreateGPUResource(const PrimitiveData& data, PrimitiveResource& resource);

private:
    // --- 頂点生成・インデックス生成の分割ヘルパー ---
    static void GenerateSphereVertices(PrimitiveData& data, float radius, uint32_t subdivision);
    static void GenerateSphereIndices(PrimitiveData& data, uint32_t subdivision);
    
    static void GenerateCylinderVertices(PrimitiveData& data, float bottomRadius, float topRadius, float height, uint32_t segments, bool hasTop, bool hasBottom, bool centered);
    static void GenerateCylinderIndices(PrimitiveData& data, uint32_t segments, bool hasTop, bool hasBottom);

    static void GenerateRingVertices(PrimitiveData& data, const RingParams& params);
    static void GenerateRingIndices(PrimitiveData& data, uint32_t segments);

    static void GenerateTorusVertices(PrimitiveData& data, float majorRadius, float minorRadius, uint32_t majorSegments, uint32_t minorSegments);
    static void GenerateTorusIndices(PrimitiveData& data, uint32_t majorSegments, uint32_t minorSegments);

private:
    std::map<PrimitiveType, PrimitiveData> cpuCache_;
    std::map<PrimitiveType, PrimitiveResource> gpuCache_;
    std::map<uint32_t, PrimitiveResource> cylinderGpuCache_;
};

