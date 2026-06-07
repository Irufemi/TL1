#define NOMINMAX
#include "Circle2D.h"

#include "Resource/Texture/TextureManager.h"
#include "Engine/Manager/DrawManager.h"
#include "Engine/Manager/DebugUI.h"
#include "Engine/Core/Math/Math.h"
#include <algorithm>
#include <vector>

TextureManager* Circle2D::textureManager_ = nullptr;
DrawManager*    Circle2D::drawManager_ = nullptr;
DebugUI*        Circle2D::ui_ = nullptr;
IrufemiEngine*  Circle2D::engine_ = nullptr;

#include "Engine/IrufemiEngine.h"

void Circle2D::Initialize(const std::string& textureName, uint32_t subdiv) {
    subdivision_ = std::max<uint32_t>(3, subdiv & ~1u); // 偶数に丸め、最低3

    resource_ = std::make_unique<Object2DResource>();

    // 頂点/インデックス生成
    BuildUnitCircleFan(subdivision_);

    // GPUリソース確保
    resource_->CreateResource();
    resource_->Map();

    // 頂点/インデックスデータの転送
    std::copy(resource_->vertexDataList_.begin(), resource_->vertexDataList_.end(), resource_->vertexData_);
    std::copy(resource_->indexDataList_.begin(), resource_->indexDataList_.end(), resource_->indexData_);

    // マテリアル/WVP 初期設定
    InitMaterialAndMatrix();

    // テクスチャ設定
    if (textureManager_) {
        resource_->textureHandle_ = textureManager_->GetTextureHandle(textureName);

        // デバッグUI用のインデックス更新
        auto names = textureManager_->GetTextureNames();
        std::sort(names.begin(), names.end());
        auto it = std::find(names.begin(), names.end(), textureName);
        if (it != names.end()) {
            selectedTextureIndex_ = static_cast<int>(std::distance(names.begin(), it));
        }
    }
    resource_->GetMaterialData()->hasTexture = useTexture_;
}

void Circle2D::BuildUnitCircleFan(uint32_t subdiv) {
    // 頂点データ: position(float4), tex(float2), normal(float3)
    resource_->vertexDataList_.clear();
    resource_->indexDataList_.clear();

    // 中心頂点(UVは中心)
    resource_->vertexDataList_.push_back({
        { 0.0f, 0.0f, 0.0f, 1.0f },
        { 0.5f, 0.5f },
        { 0.0f, 0.0f, -1.0f }
    });

    // 周上頂点
    const float step = 2.0f * pi_ / static_cast<float>(subdiv);
    for (uint32_t i = 0; i <= subdiv; ++i) {
        float th = step * static_cast<float>(i);
        float x = std::cos(th);
        float y = std::sin(th);
        // UVは[-1,1] -> [0,1] へ射影(V反転込み)
        float u = 0.5f + 0.5f * x;
        float v = 0.5f - 0.5f * y;

        resource_->vertexDataList_.push_back({
            { x, y, 0.0f, 1.0f },
            { u, v },
            { 0.0f, 0.0f, -1.0f }
        });
    }

    // インデックス(三角形ファン)
    // 中心: 0, 周: [1..subdiv+1]
    for (uint32_t i = 1; i <= subdiv; ++i) {
        resource_->indexDataList_.push_back(0);
        resource_->indexDataList_.push_back(i);
        resource_->indexDataList_.push_back(i + 1);
    }
}

void Circle2D::InitMaterialAndMatrix() {
    // transform 初期値
    // resource_->transform_.scale は「係数」として扱う(SphereClass と同様)
    resource_->transform_.scale = { 1.0f, 1.0f, 1.0f };
    resource_->transform_.rotate = { 0.0f, 0.0f, 0.0f };
    resource_->transform_.translate = info_.center;

    // 実スケール = 半径 × 係数(非等方スケールを許容)
    Vector3 effectiveScale{
        info_.radius * resource_->transform_.scale.x,
        info_.radius * resource_->transform_.scale.y,
        info_.radius * resource_->transform_.scale.z
    };

    // 行列
    resource_->transformationMatrix_.world =
        Math::MakeAffineMatrix(effectiveScale, resource_->transform_.rotate, resource_->transform_.translate);
    if (engine_) {
        if (Camera* activeCam = engine_->GetCameraManager()->GetActiveCamera()) {
            resource_->transformationMatrix_.WVP =
                Math::Multiply(resource_->transformationMatrix_.world, activeCam->GetOrthographicMatrix());
        }
    }

    // マテリアル
    resource_->GetMaterialData()->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    resource_->GetMaterialData()->enableLighting = false;
    resource_->GetMaterialData()->hasTexture = true;
    resource_->GetMaterialData()->lightingMode = 2;
    resource_->GetMaterialData()->uvTransform = Math::MakeIdentity4x4();
}

void Circle2D::UpdateMatrix() {
    // transform_.scale は係数のまま(非等方を許容)
    resource_->transform_.translate = info_.center;

    // 実スケール = 半径 × 係数(各成分個別に乗算)
    Vector3 effectiveScale{
        info_.radius * resource_->transform_.scale.x,
        info_.radius * resource_->transform_.scale.y,
        info_.radius * resource_->transform_.scale.z
    };

    resource_->transformationMatrix_.world =
        Math::MakeAffineMatrix(effectiveScale, resource_->transform_.rotate, resource_->transform_.translate);
    if (engine_) {
        if (Camera* activeCam = engine_->GetCameraManager()->GetActiveCamera()) {
            resource_->transformationMatrix_.WVP =
                Math::Multiply(resource_->transformationMatrix_.world, activeCam->GetOrthographicMatrix());
        }
    }
    
    resource_->MarkAsDirty();
}

void Circle2D::Update() {

    // 毎フレーム行列更新(カメラ正射影が動く可能性があるため)
    UpdateMatrix();

    // UV 変換はSpriteと同様の意味付け(ここではIdentityのまま)
    resource_->GetMaterialData()->uvTransform = Math::MakeIdentity4x4();
}

void Circle2D::SyncBeforeDraw() {
    resource_->SyncBeforeDraw();
}

void Circle2D::Draw() {
    SyncBeforeDraw();
    if (isTopMost_) {
        drawManager_->SubmitTopMostSprite(resource_.get());
    } else {
        drawManager_->SubmitSprite(resource_.get());
    }
}

void Circle2D::SetInfo(const Circle2DInfo& info) {
    info_ = info;
    UpdateMatrix();
}

void Circle2D::SetCenter(const Vector3& center) {
    info_.center = center;
    UpdateMatrix();
}

void Circle2D::SetRadius(float radius) {
    info_.radius = radius;
    UpdateMatrix();
}

bool Circle2D::SetTextureByName(const std::string& textureName) {
    if (!textureManager_) return false;
    auto names = textureManager_->GetTextureNames();
    std::sort(names.begin(), names.end());
    auto it = std::find(names.begin(), names.end(), textureName);
    if (it == names.end()) return false;
    selectedTextureIndex_ = static_cast<int>(std::distance(names.begin(), it));
    resource_->textureHandle_ = textureManager_->GetTextureHandle(*it);
    return true;
}

void Circle2D::Debug([[maybe_unused]] const char* circleName) {
#if defined USE_IMGUI
    std::string name = std::string("Circle2D: ") + circleName;
    ImGui::Begin(name.c_str());
    ui_->DebugTransform2D(resource_->transform_);
    ui_->DebugMaterialBy2D(resource_->GetMaterialData());

    bool useTex = useTexture_;
    if (ImGui::Checkbox("UseTexture", &useTex)) {
        SetUseTexture(useTex);
    }

    if (textureManager_) {
        auto names = textureManager_->GetTextureNames();
        std::sort(names.begin(), names.end());
        if (!names.empty()) {
            const char* preview = names[selectedTextureIndex_].c_str();
            if (ImGui::BeginCombo("Texture", preview)) {
                for (int i = 0; i < static_cast<int>(names.size()); ++i) {
                    bool sel = (i == selectedTextureIndex_);
                    if (ImGui::Selectable(names[i].c_str(), sel)) {
                        selectedTextureIndex_ = i;
                        resource_->textureHandle_ = textureManager_->GetTextureHandle(names[i]);
                    }
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }
    }
    ImGui::End();
#endif
    Update();
}