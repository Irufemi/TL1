#include "PrimitiveManager.h"
#include "Renderer/Core/BaseResource.h"
#include "Engine/Graphics/DirectX/DirectXCommon.h"
#include "Engine/Core/Math/Vector3.h"
#include "Engine/Core/Math/Vector2.h"
#include <cmath>
#include <numbers>
#include <algorithm>
#include <utility>

const PrimitiveData& PrimitiveManager::GetPrimitiveData(PrimitiveType type) {
    auto it = cpuCache_.find(type);
    if (it != cpuCache_.end()) {
        return it->second;
    }

    PrimitiveData data;
    switch (type) {
    case PrimitiveType::Triangle: data = CreateTriangle(); break;
    case PrimitiveType::Plane:    data = CreatePlane(); break;
    case PrimitiveType::Cube:     data = CreateCube(1.0f, 1.0f, 1.0f); break;
    case PrimitiveType::Sphere:   data = CreateSphere(0.5f, 32); break;
    case PrimitiveType::Cylinder: data = CreateCylinder(0.5f, 1.0f, 32); break;
    case PrimitiveType::Tetra:    data = CreateTetra(); break;
    case PrimitiveType::Circle:   data = CreateCircle(0.5f, 32); break;
    case PrimitiveType::Ring:     data = CreateRing(0.2f, 0.5f, 0.0f, 360.0f, 32, false); break;
    case PrimitiveType::Skybox:   data = CreateCube(1.0f, 1.0f, 1.0f); break;
    case PrimitiveType::Cone:     data = CreateCone(0.5f, 1.0f, 32); break;
    case PrimitiveType::Torus:    data = CreateTorus(0.4f, 0.1f, 32, 16); break;
    case PrimitiveType::IcoSphere: data = CreateIcoSphere(0.5f, 2); break;
    case PrimitiveType::Grid:     data = CreateGrid(1.0f, 1.0f, 10, 10); break;
    default: break;
    }

    cpuCache_[type] = std::move(data);
    return cpuCache_[type];
}

const std::vector<VertexData>& PrimitiveManager::GetVertices(PrimitiveType type) {
    return GetPrimitiveData(type).vertices;
}

const PrimitiveResource& PrimitiveManager::GetStandardResource(PrimitiveType type) {
    auto it = gpuCache_.find(type);
    if (it != gpuCache_.end()) {
        return it->second;
    }

    const auto& data = GetPrimitiveData(type);
    PrimitiveResource resource;
    CreateGPUResource(data, resource);
    gpuCache_[type] = std::move(resource);
    return gpuCache_[type];
}

const PrimitiveResource& PrimitiveManager::GetCylinderResource(bool hasTop, bool hasBottom) {
    uint32_t key = (hasTop ? 2 : 0) | (hasBottom ? 1 : 0);
    auto it = cylinderGpuCache_.find(key);
    if (it != cylinderGpuCache_.end()) {
        return it->second;
    }

    PrimitiveData data = CreateCylinder(0.5f, 1.0f, 32, hasTop, hasBottom);
    PrimitiveResource resource;
    CreateGPUResource(data, resource);
    cylinderGpuCache_[key] = std::move(resource);
    return cylinderGpuCache_[key];
}

void PrimitiveManager::CreateGPUResource(const PrimitiveData& data, PrimitiveResource& resource) {
    auto* dxCommon = BaseResource::GetDirectXCommon();
    if (!dxCommon) return;

    resource.indexCount = static_cast<uint32_t>(data.indices.size());
    
    resource.vertexResource = dxCommon->CreateBufferResource(sizeof(VertexData) * data.vertices.size());
    resource.indexResource = dxCommon->CreateBufferResource(sizeof(uint32_t) * data.indices.size());

    // データの転送
    VertexData* vertexMapped = nullptr;
    resource.vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexMapped));
    std::copy(data.vertices.begin(), data.vertices.end(), vertexMapped);
    resource.vertexResource->Unmap(0, nullptr);

    uint32_t* indexMapped = nullptr;
    resource.indexResource->Map(0, nullptr, reinterpret_cast<void**>(&indexMapped));
    std::copy(data.indices.begin(), data.indices.end(), indexMapped);
    resource.indexResource->Unmap(0, nullptr);

    // View の作成
    resource.vertexBufferView.BufferLocation = resource.vertexResource->GetGPUVirtualAddress();
    resource.vertexBufferView.StrideInBytes = sizeof(VertexData);
    resource.vertexBufferView.SizeInBytes = static_cast<UINT>(sizeof(VertexData) * data.vertices.size());

    resource.indexBufferView.BufferLocation = resource.indexResource->GetGPUVirtualAddress();
    resource.indexBufferView.SizeInBytes = static_cast<UINT>(sizeof(uint32_t) * data.indices.size());
    resource.indexBufferView.Format = DXGI_FORMAT_R32_UINT;
}

PrimitiveData PrimitiveManager::CreateTriangle() {
    PrimitiveData data;
    data.vertices = {
        { {  0.0f,  0.5f, 0.0f, 1.0f }, { 0.5f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
        { {  0.5f, -0.5f, 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } },
        { { -0.5f, -0.5f, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } }
    };
    data.indices = { 0, 1, 2 };
    return data;
}

PrimitiveData PrimitiveManager::CreatePlane(float width, float height) {
    PrimitiveData data;
    float hx = width * 0.5f;
    float hy = height * 0.5f;

    data.vertices = {
        { { -hx, -hy, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } }, // v0
        { {  hx, -hy, 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } }, // v1
        { {  hx,  hy, 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } }, // v2
        { { -hx,  hy, 0.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } }, // v3
    };
    data.indices = { 0, 2, 1, 0, 3, 2 };
    return data;
}

PrimitiveData PrimitiveManager::CreateCube(float width, float height, float depth) {
    PrimitiveData data;
    const float hx = width * 0.5f;
    const float hy = height * 0.5f;
    const float hz = depth * 0.5f;

    data.vertices = {
        // 前面 (-Z)
        { { -hx, -hy, -hz, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } }, // 0
        { { -hx,  hy, -hz, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } }, // 1
        { {  hx, -hy, -hz, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } }, // 2
        { {  hx,  hy, -hz, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } }, // 3
        // 背面 (+Z)
        { {  hx, -hy,  hz, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } }, // 4
        { {  hx,  hy,  hz, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } }, // 5
        { { -hx, -hy,  hz, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } }, // 6
        { { -hx,  hy,  hz, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } }, // 7
        // 左面 (-X)
        { { -hx, -hy,  hz, 1.0f }, { 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f } }, // 8
        { { -hx,  hy,  hz, 1.0f }, { 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f } }, // 9
        { { -hx, -hy, -hz, 1.0f }, { 1.0f, 1.0f }, { -1.0f, 0.0f, 0.0f } }, // 10
        { { -hx,  hy, -hz, 1.0f }, { 1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f } }, // 11
        // 右面 (+X)
        { {  hx, -hy, -hz, 1.0f }, { 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f } }, // 12
        { {  hx,  hy, -hz, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } }, // 13
        { {  hx, -hy,  hz, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f } }, // 14
        { {  hx,  hy,  hz, 1.0f }, { 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } }, // 15
        // 下面 (-Y)
        { { -hx, -hy, -hz, 1.0f }, { 0.0f, 1.0f }, { 0.0f, -1.0f, 0.0f } }, // 16
        { {  hx, -hy, -hz, 1.0f }, { 1.0f, 1.0f }, { 0.0f, -1.0f, 0.0f } }, // 17
        { { -hx, -hy,  hz, 1.0f }, { 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f } }, // 18
        { {  hx, -hy,  hz, 1.0f }, { 1.0f, 0.0f }, { 0.0f, -1.0f, 0.0f } }, // 19
        // 上面 (+Y)
        { { -hx,  hy,  hz, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } }, // 20
        { {  hx,  hy,  hz, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } }, // 21
        { { -hx,  hy, -hz, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } }, // 22
        { {  hx,  hy, -hz, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } }  // 23
    };
    data.indices = {
        0, 1, 2, 2, 1, 3, // 前面
        4, 5, 6, 6, 5, 7, // 背面
        8, 9, 10, 10, 9, 11, // 左面
        12, 13, 14, 14, 13, 15, // 右面
        16, 17, 18, 18, 17, 19, // 下面
        20, 21, 22, 22, 21, 23  // 上面
    };
    return data;
}

PrimitiveData PrimitiveManager::CreateSphere(float radius, uint32_t subdivision) {
    PrimitiveData data;
    GenerateSphereVertices(data, radius, subdivision);
    GenerateSphereIndices(data, subdivision);
    return data;
}

void PrimitiveManager::GenerateSphereVertices(PrimitiveData& data, float radius, uint32_t subdivision) {
    const float pi = std::numbers::pi_v<float>;
    const float latEvery = pi / static_cast<float>(subdivision);
    const float lonEvery = 2.0f * pi / static_cast<float>(subdivision);

    for (uint32_t latIndex = 0; latIndex <= subdivision; ++latIndex) {
        float lat = -pi / 2.0f + latEvery * latIndex;
        for (uint32_t lonIndex = 0; lonIndex <= subdivision; ++lonIndex) {
            float lon = lonIndex * lonEvery;
            VertexData v;
            v.position = {
                radius * std::cos(lat) * std::cos(lon),
                radius * std::sin(lat),
                radius * std::cos(lat) * std::sin(lon),
                1.0f
            };
            v.texcoord = {
                static_cast<float>(lonIndex) / subdivision,
                1.0f - static_cast<float>(latIndex) / subdivision
            };
            v.normal = { v.position.x / radius, v.position.y / radius, v.position.z / radius };
            data.vertices.push_back(v);
        }
    }
}

void PrimitiveManager::GenerateSphereIndices(PrimitiveData& data, uint32_t subdivision) {
    for (uint32_t latIndex = 0; latIndex < subdivision; ++latIndex) {
        for (uint32_t lonIndex = 0; lonIndex < subdivision; ++lonIndex) {
            uint32_t base = (subdivision + 1) * latIndex + lonIndex;
            data.indices.push_back(base);
            data.indices.push_back(base + subdivision + 1);
            data.indices.push_back(base + 1);
            data.indices.push_back(base + subdivision + 1);
            data.indices.push_back(base + subdivision + 2);
            data.indices.push_back(base + 1);
        }
    }
}

PrimitiveData PrimitiveManager::CreateCylinder(float bottomRadius, float topRadius, float height, uint32_t segments, bool hasTop, bool hasBottom, bool centered) {
    PrimitiveData data;
    GenerateCylinderVertices(data, bottomRadius, topRadius, height, segments, hasTop, hasBottom, centered);
    GenerateCylinderIndices(data, segments, hasTop, hasBottom);
    return data;
}

PrimitiveData PrimitiveManager::CreateCylinder(float radius, float height, uint32_t segments, bool hasTop, bool hasBottom) {
    // 既存の実装（敵ビームなど）を壊さないため、上下同じ半径、中心原点(centered=true)として呼び戻す
    return CreateCylinder(radius, radius, height, segments, hasTop, hasBottom, true);
}

void PrimitiveManager::GenerateCylinderVertices(PrimitiveData& data, float bottomRadius, float topRadius, float height, uint32_t segments, bool hasTop, bool hasBottom, bool centered) {
    const float pi = std::numbers::pi_v<float>;
    const float radianPerDivide = 2.0f * pi / static_cast<float>(segments);

    float yBottom = centered ? -height * 0.5f : 0.0f;
    float yTop    = centered ? height * 0.5f : height;

    // 側面
    for (uint32_t i = 0; i < segments; ++i) {
        float rad = static_cast<float>(i) * radianPerDivide;
        float radNext = static_cast<float>(i + 1) * radianPerDivide;

        float s = std::sin(rad);
        float c = std::cos(rad);
        float sNext = std::sin(radNext);
        float cNext = std::cos(radNext);

        float u = static_cast<float>(i) / segments;
        float uNext = static_cast<float>(i + 1) / segments;

        // 資料に合わせて X = -sin, Z = cos のロジックに変更
        data.vertices.push_back({ { -s * bottomRadius, yBottom, c * bottomRadius, 1.0f }, { u, 1.0f }, { -s, 0.0f, c } });
        data.vertices.push_back({ { -s * topRadius,    yTop,    c * topRadius,    1.0f }, { u, 0.0f }, { -s, 0.0f, c } });
        data.vertices.push_back({ { -sNext * bottomRadius, yBottom, cNext * bottomRadius, 1.0f }, { uNext, 1.0f }, { -sNext, 0.0f, cNext } });
        data.vertices.push_back({ { -sNext * topRadius,    yTop,    cNext * topRadius,    1.0f }, { uNext, 0.0f }, { -sNext, 0.0f, cNext } });
    }

    // 上蓋
    if (hasTop) {
        data.vertices.push_back({ { 0.0f, yTop, 0.0f, 1.0f }, { 0.5f, 0.5f }, { 0.0f, 1.0f, 0.0f } }); // 中心
        for (uint32_t i = 0; i <= segments; ++i) {
            float rad = static_cast<float>(i) * radianPerDivide;
            float c = std::cos(rad);
            float s = std::sin(rad);
            data.vertices.push_back({ { -s * topRadius, yTop, c * topRadius, 1.0f }, { 0.5f - s * 0.5f, 0.5f - c * 0.5f }, { 0.0f, 1.0f, 0.0f } });
        }
    }

    // 下蓋
    if (hasBottom) {
        data.vertices.push_back({ { 0.0f, yBottom, 0.0f, 1.0f }, { 0.5f, 0.5f }, { 0.0f, -1.0f, 0.0f } }); // 中心
        for (uint32_t i = 0; i <= segments; ++i) {
            float rad = static_cast<float>(i) * radianPerDivide;
            float c = std::cos(rad);
            float s = std::sin(rad);
            data.vertices.push_back({ { -s * bottomRadius, yBottom, c * bottomRadius, 1.0f }, { 0.5f - s * 0.5f, 0.5f + c * 0.5f }, { 0.0f, -1.0f, 0.0f } });
        }
    }
}

void PrimitiveManager::GenerateCylinderIndices(PrimitiveData& data, uint32_t segments, bool hasTop, bool hasBottom) {
    // 側面
    for (uint32_t i = 0; i < segments; ++i) {
        uint32_t base = i * 4;
        data.indices.push_back(base);
        data.indices.push_back(base + 1);
        data.indices.push_back(base + 2);
        data.indices.push_back(base + 1);
        data.indices.push_back(base + 3);
        data.indices.push_back(base + 2);
    }

    uint32_t vertexOffset = segments * 4; // 側面の頂点数

    // 上蓋
    if (hasTop) {
        uint32_t centerIndex = vertexOffset;
        uint32_t perimeterBase = vertexOffset + 1;
        for (uint32_t i = 0; i < segments; ++i) {
            data.indices.push_back(centerIndex);
            data.indices.push_back(perimeterBase + i + 1); // 反転
            data.indices.push_back(perimeterBase + i);     // 反転
        }
        vertexOffset += 1 + (segments + 1);
    }

    // 下蓋
    if (hasBottom) {
        uint32_t centerIndex = vertexOffset;
        uint32_t perimeterBase = vertexOffset + 1;
        for (uint32_t i = 0; i < segments; ++i) {
            data.indices.push_back(centerIndex);
            data.indices.push_back(perimeterBase + i);     // 反転 (元が i+1, i だったので i, i+1 に戻る)
            data.indices.push_back(perimeterBase + i + 1); // 反転
        }
    }
}

PrimitiveData PrimitiveManager::CreateRing(const RingParams& params) {
    PrimitiveData data;
    GenerateRingVertices(data, params);
    GenerateRingIndices(data, params.segments);
    return data;
}

PrimitiveData PrimitiveManager::CreateRing(float innerRadius, float outerRadius, float startAngle, float endAngle, uint32_t segments, bool verticalUV) {
    RingParams params;
    params.innerRadius = innerRadius;
    params.startOuterRadius = outerRadius;
    params.endOuterRadius = outerRadius;
    params.startAngle = startAngle;
    params.endAngle = endAngle;
    params.segments = segments;
    params.verticalUV = verticalUV;
    return CreateRing(params);
}

/**
 * @brief リング（ドーナツ型）の頂点データを生成する
 * @details 学校資料に準拠し、12時方向を基準（X = -sin, Y = cos）として時計回りに頂点を生成します。
 *          必要に応じて、U/Vの割り当て方向や描画する角度の範囲を指定可能です。
 */
void PrimitiveManager::GenerateRingVertices(PrimitiveData& data, const RingParams& params) {
    const float pi = std::numbers::pi_v<float>;
    float startRad = params.startAngle * (pi / 180.0f);
    float endRad = params.endAngle * (pi / 180.0f);
    if (endRad <= startRad) endRad += 2.0f * pi;
    float arc = endRad - startRad;
    float radianPerDivide = arc / static_cast<float>(params.segments);
    float fadeRangeRad = params.fadeRangeAngle * (pi / 180.0f);

    for (uint32_t i = 0; i < params.segments; ++i) {
        float a0 = startRad + i * radianPerDivide;
        float a1 = startRad + (i + 1) * radianPerDivide;

        float s0 = std::sin(a0), c0 = std::cos(a0);
        float s1 = std::sin(a1), c1 = std::cos(a1);

        float u = static_cast<float>(i) / params.segments;
        float uNext = static_cast<float>(i + 1) / params.segments;

        // 外径の補間 (Lerp)
        float outerRad0 = params.startOuterRadius + (params.endOuterRadius - params.startOuterRadius) * u;
        float outerRad1 = params.startOuterRadius + (params.endOuterRadius - params.startOuterRadius) * uNext;

        // アルファフェードの計算
        auto calculateAlpha = [&](float currentAngleRad) {
            if (fadeRangeRad <= 0.0f) return 1.0f;
            
            float distanceFromStart = currentAngleRad - startRad;
            float distanceFromEnd = endRad - currentAngleRad;

            float alpha = 1.0f;
            if (distanceFromStart < fadeRangeRad) {
                float t = distanceFromStart / fadeRangeRad;
                alpha = params.startAlpha + (1.0f - params.startAlpha) * t;
            } else if (distanceFromEnd < fadeRangeRad) {
                float t = distanceFromEnd / fadeRangeRad;
                alpha = params.endAlpha + (1.0f - params.endAlpha) * t;
            }
            return alpha;
        };

        float alpha0 = calculateAlpha(a0);
        float alpha1 = calculateAlpha(a1);

        VertexData v0, v1, v2, v3;
        // 資料に基づき X = -sin, Y = cos と計算する（時計回りのポリゴンは維持されるためカリングへの悪影響はなし）
        v0.position = { -s0 * outerRad0, c0 * outerRad0, 0.0f, 1.0f };
        v1.position = { -s1 * outerRad1, c1 * outerRad1, 0.0f, 1.0f };
        v2.position = { -s0 * params.innerRadius, c0 * params.innerRadius, 0.0f, 1.0f };
        v3.position = { -s1 * params.innerRadius, c1 * params.innerRadius, 0.0f, 1.0f };

        if (params.verticalUV) {
            v0.texcoord = { 0.0f, u }; v1.texcoord = { 0.0f, uNext };
            v2.texcoord = { 1.0f, u }; v3.texcoord = { 1.0f, uNext };
        } else {
            v0.texcoord = { u, 0.0f }; v1.texcoord = { uNext, 0.0f };
            v2.texcoord = { u, 1.0f }; v3.texcoord = { uNext, 1.0f };
        }
        
        // 法線は従来の共通仕様に合わせて-Zを維持する
        v0.normal = v1.normal = v2.normal = v3.normal = { 0.0f, 0.0f, -1.0f };

        // 頂点カラーの設定
        v0.color = { params.outerColor.x, params.outerColor.y, params.outerColor.z, params.outerColor.w * alpha0 };
        v1.color = { params.outerColor.x, params.outerColor.y, params.outerColor.z, params.outerColor.w * alpha1 };
        v2.color = { params.innerColor.x, params.innerColor.y, params.innerColor.z, params.innerColor.w * alpha0 };
        v3.color = { params.innerColor.x, params.innerColor.y, params.innerColor.z, params.innerColor.w * alpha1 };

        data.vertices.push_back(v0); data.vertices.push_back(v1);
        data.vertices.push_back(v2); data.vertices.push_back(v3);
    }
}

void PrimitiveManager::GenerateRingIndices(PrimitiveData& data, uint32_t segments) {
    for (uint32_t i = 0; i < segments; ++i) {
        uint32_t base = i * 4;
        data.indices.push_back(base); data.indices.push_back(base + 2); data.indices.push_back(base + 1);
        data.indices.push_back(base + 1); data.indices.push_back(base + 2); data.indices.push_back(base + 3);
    }
}

PrimitiveData PrimitiveManager::CreateTetra() {
    PrimitiveData data;
    const float s = 0.5f;
    const float R = 1.0f / std::sqrt(3.0f);
    const float baseToApex = std::sqrt(2.0f / 3.0f);
    const float b = baseToApex / 4.0f;
    const float a = 3.0f * b;

    Vector3 apex = { 0.0f, a * s, 0.0f };
    Vector3 v0 = { 0.0f, -b * s, R * s };
    Vector3 v1 = { -0.5f * s, -b * s, -R * 0.5f * s };
    Vector3 v2 = { 0.5f * s, -b * s, -R * 0.5f * s };

    std::vector<std::vector<Vector3>> faces = { { v0, v1, v2 }, { apex, v0, v1 }, { apex, v1, v2 }, { apex, v2, v0 } };

    for (const auto& face : faces) {
        uint32_t base = static_cast<uint32_t>(data.vertices.size());
        Vector3 p0 = face[0], p1 = face[1], p2 = face[2];
        Vector3 e0 = { p1.x - p0.x, p1.y - p0.y, p1.z - p0.z };
        Vector3 e1 = { p2.x - p0.x, p2.y - p0.y, p2.z - p0.z };
        // Simplified Cross and Normalize
        Vector3 n = { e0.y * e1.z - e0.z * e1.y, e0.z * e1.x - e0.x * e1.z, e0.x * e1.y - e0.y * e1.x };
        float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
        n = { n.x / len, n.y / len, n.z / len };

        data.vertices.push_back({ { p0.x, p0.y, p0.z, 1.0f }, { 0.5f, 0.0f }, n });
        data.vertices.push_back({ { p1.x, p1.y, p1.z, 1.0f }, { 0.0f, 1.0f }, n });
        data.vertices.push_back({ { p2.x, p2.y, p2.z, 1.0f }, { 1.0f, 1.0f }, n });

        Vector3 centroid = { (p0.x + p1.x + p2.x) / 3.0f, (p0.y + p1.y + p2.y) / 3.0f, (p0.z + p1.z + p2.z) / 3.0f };
        if (n.x * centroid.x + n.y * centroid.y + n.z * centroid.z >= 0.0f) {
            data.indices.push_back(base); data.indices.push_back(base + 1); data.indices.push_back(base + 2);
        } else {
            data.indices.push_back(base + 2); data.indices.push_back(base + 1); data.indices.push_back(base );
        }
    }
    return data;
}

PrimitiveData PrimitiveManager::CreateCircle(float radius, uint32_t segments) {
    PrimitiveData data;
    const float pi = std::numbers::pi_v<float>;
    data.vertices.push_back({ { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.5f, 0.5f }, { 0.0f, 0.0f, -1.0f } });
    for (uint32_t i = 0; i <= segments; ++i) {
        float rad = 2.0f * pi * i / segments;
        float c = std::cos(rad), s = std::sin(rad);
        data.vertices.push_back({ { c * radius, s * radius, 0.0f, 1.0f }, { c * 0.5f + 0.5f, s * 0.5f + 0.5f }, { 0.0f, 0.0f, -1.0f } });
    }
    for (uint32_t i = 0; i < segments; ++i) {
        data.indices.push_back(0); data.indices.push_back(i + 1); data.indices.push_back(i + 2);
    }
    return data;
}

PrimitiveData PrimitiveManager::CreateCone(float radius, float height, uint32_t segments) {
    PrimitiveData data;
    const float pi = std::numbers::pi_v<float>;
    const float radianPerDivide = 2.0f * pi / static_cast<float>(segments);
    data.vertices.push_back({ { 0.0f, height * 0.5f, 0.0f, 1.0f }, { 0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f } });
    data.vertices.push_back({ { 0.0f, -height * 0.5f, 0.0f, 1.0f }, { 0.5f, 0.5f }, { 0.0f, -1.0f, 0.0f } });
    for (uint32_t i = 0; i <= segments; ++i) {
        float rad = static_cast<float>(i) * radianPerDivide;
        float s = std::sin(rad), c = std::cos(rad);
        float u = static_cast<float>(i) / segments;
        float nx = c, ny = radius / height, nz = s;
        float nlen = std::sqrt(nx * nx + ny * ny + nz * nz);
        data.vertices.push_back({ { c * radius, -height * 0.5f, s * radius, 1.0f }, { u, 1.0f }, { nx / nlen, ny / nlen, nz / nlen } });
        data.vertices.push_back({ { c * radius, -height * 0.5f, s * radius, 1.0f }, { u, 1.0f }, { 0.0f, -1.0f, 0.0f } });
    }
    for (uint32_t i = 0; i < segments; ++i) {
        uint32_t base = 2 + i * 2;
        data.indices.push_back(0); data.indices.push_back(base + 2); data.indices.push_back(base);
        data.indices.push_back(1); data.indices.push_back(base + 1); data.indices.push_back(base + 3);
    }
    return data;
}

PrimitiveData PrimitiveManager::CreateTorus(float majorRadius, float minorRadius, uint32_t majorSegments, uint32_t minorSegments) {
    PrimitiveData data;
    GenerateTorusVertices(data, majorRadius, minorRadius, majorSegments, minorSegments);
    GenerateTorusIndices(data, majorSegments, minorSegments);
    return data;
}

void PrimitiveManager::GenerateTorusVertices(PrimitiveData& data, float majorRadius, float minorRadius, uint32_t majorSegments, uint32_t minorSegments) {
    const float pi = std::numbers::pi_v<float>;
    for (uint32_t j = 0; j <= minorSegments; ++j) {
        float v = static_cast<float>(j) / minorSegments;
        float phi = v * 2.0f * pi;
        float cosPhi = std::cos(phi), sinPhi = std::sin(phi);
        for (uint32_t i = 0; i <= majorSegments; ++i) {
            float u = static_cast<float>(i) / majorSegments;
            float theta = u * 2.0f * pi;
            float cosTheta = std::cos(theta), sinTheta = std::sin(theta);
            VertexData vertex;
            vertex.position = { (majorRadius + minorRadius * cosPhi) * cosTheta, (majorRadius + minorRadius * cosPhi) * sinTheta, minorRadius * sinPhi, 1.0f };
            vertex.normal = { cosPhi * cosTheta, cosPhi * sinTheta, sinPhi };
            vertex.texcoord = { u, v };
            data.vertices.push_back(vertex);
        }
    }
}

void PrimitiveManager::GenerateTorusIndices(PrimitiveData& data, uint32_t majorSegments, uint32_t minorSegments) {
    for (uint32_t j = 0; j < minorSegments; ++j) {
        for (uint32_t i = 0; i < majorSegments; ++i) {
            uint32_t base = j * (majorSegments + 1) + i;
            data.indices.push_back(base); data.indices.push_back(base + majorSegments + 1); data.indices.push_back(base + 1);
            data.indices.push_back(base + 1); data.indices.push_back(base + majorSegments + 1); data.indices.push_back(base + majorSegments + 2);
        }
    }
}

PrimitiveData PrimitiveManager::CreateIcoSphere(float radius, uint32_t subdivision) {
    PrimitiveData data;
    const float t = (1.0f + std::sqrt(5.0f)) / 2.0f;
    std::vector<Vector3> verts = {{-1,t,0}, {1,t,0}, {-1,-t,0}, {1,-t,0}, {0,-1,t}, {0,1,t}, {0,-1,-t}, {0,1,-t}, {t,0,-1}, {t,0,1}, {-t,0,-1}, {-t,0,1}};
    for (auto& v : verts) {
        float len = std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
        v = {v.x/len*radius, v.y/len*radius, v.z/len*radius};
    }
    struct Triangle { uint32_t v1, v2, v3; };
    std::vector<Triangle> triangles = {{0,11,5},{0,5,1},{0,1,7},{0,7,10},{0,10,11},{1,5,9},{5,11,4},{11,10,2},{10,7,6},{7,1,8},{3,9,4},{3,4,2},{3,2,6},{3,6,8},{3,8,9},{4,9,5},{2,4,11},{6,2,10},{8,6,7},{9,8,1}};
    auto getMiddle = [&](uint32_t p1, uint32_t p2, std::map<uint64_t, uint32_t>& cache) {
        uint64_t key = (static_cast<uint64_t>((std::min)(p1, p2)) << 32) | (std::max)(p1, p2);
        if (cache.count(key)) return cache[key];
        Vector3 v1 = verts[p1], v2 = verts[p2];
        Vector3 m = {(v1.x+v2.x)/2,(v1.y+v2.y)/2,(v1.z+v2.z)/2};
        float l = std::sqrt(m.x*m.x+m.y*m.y+m.z*m.z);
        verts.push_back({m.x/l*radius, m.y/l*radius, m.z/l*radius});
        return cache[key] = (uint32_t)verts.size()-1;
    };
    for (uint32_t i=0; i<subdivision; ++i) {
        std::vector<Triangle> next; std::map<uint64_t, uint32_t> cache;
        for (auto& tri : triangles) {
            uint32_t a = getMiddle(tri.v1, tri.v2, cache), b = getMiddle(tri.v2, tri.v3, cache), c = getMiddle(tri.v3, tri.v1, cache);
            next.push_back({tri.v1,a,c}); next.push_back({tri.v2,b,a}); next.push_back({tri.v3,c,b}); next.push_back({a,b,c});
        }
        triangles = next;
    }
    const float pi = std::numbers::pi_v<float>;
    for (const auto& v : verts) {
        VertexData vd; vd.position = {v.x,v.y,v.z,1.0f}; vd.normal = {v.x/radius,v.y/radius,v.z/radius};
        vd.texcoord = {0.5f + std::atan2(v.z, v.x)/(2.0f*pi), 0.5f - std::asin(v.y/radius)/pi};
        data.vertices.push_back(vd);
    }
    for (const auto& tri : triangles) { data.indices.push_back(tri.v1); data.indices.push_back(tri.v2); data.indices.push_back(tri.v3); }
    return data;
}

PrimitiveData PrimitiveManager::CreateGrid(float width, float height, uint32_t xSegments, uint32_t ySegments) {
    PrimitiveData data;
    float hx = width*0.5f, hy = height*0.5f;
    float dx = width/xSegments, dy = height/ySegments;
    for (uint32_t y=0; y<=ySegments; ++y) {
        for (uint32_t x=0; x<=xSegments; ++x) {
            VertexData v; v.position = {-hx+x*dx, -hy+y*dy, 0.0f, 1.0f};
            v.texcoord = {(float)x/xSegments, 1.0f-(float)y/ySegments}; v.normal = {0.0f,0.0f,-1.0f};
            data.vertices.push_back(v);
        }
    }
    for (uint32_t y=0; y<ySegments; ++y) {
        for (uint32_t x=0; x<xSegments; ++x) {
            uint32_t base = y*(xSegments+1)+x;
            data.indices.push_back(base); data.indices.push_back(base+xSegments+2); data.indices.push_back(base+1);
            data.indices.push_back(base); data.indices.push_back(base+xSegments+1); data.indices.push_back(base+xSegments+2);
        }
    }
    return data;
}