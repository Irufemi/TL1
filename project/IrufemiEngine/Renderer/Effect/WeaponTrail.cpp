#include "WeaponTrail.h"
#include "Renderer/Object3D/Object3DResource.h"
#include "Engine/Manager/DrawManager.h"
#include "Resource/Texture/TextureManager.h"
#include "Engine/IrufemiEngine.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Graphics/Camera/CameraManager.h"
#include "Engine/Graphics/Camera/Camera.h"

WeaponTrail::WeaponTrail() = default;
WeaponTrail::~WeaponTrail() = default;

void WeaponTrail::Initialize(IrufemiEngine* engine, const std::string& texturePath, const Vector4& color) {
    engine_ = engine;
    texturePath_ = texturePath;
    baseColor_ = color;
    isStopped_ = true;
    points_.clear();

    resource_ = std::make_unique<Object3DResource>();
    
    int numLayers = 3; // カメラ追従型のビルボード厚み（中央、左、右）
    int maxVertices = kMaxPoints * 2 * numLayers;
    int maxIndices = (kMaxPoints - 1) * 12 * numLayers; // 両面描画のためインデックスは12個

    resource_->vertexDataList_.resize(maxVertices);
    resource_->indexDataList_.resize(maxIndices);
    resource_->CreateResource();

    if (engine_) {
        if (auto tm = engine_->GetTextureManager()) {
            resource_->textureHandle_ = tm->GetTextureHandle(texturePath_);
        }
    }
    
    resource_->GetMaterialData()->color = baseColor_;
    resource_->GetMaterialData()->enableLighting = false; // エフェクトのためライティング無効
    resource_->GetMaterialData()->useClampSampler = 0; // Wrap
    resource_->GetMaterialData()->hasTexture = true;
    
    resource_->transform_.scale = {1,1,1};
    resource_->transform_.rotate = {0,0,0};
    resource_->transform_.translate = {0,0,0};
    resource_->MarkAsDirty();

    resource_->Map();
}

void WeaponTrail::AddPoint(const Vector3& basePos, const Vector3& tipPos) {
    if (isStopped_) {
        points_.clear();
        isStopped_ = false;
    }

    TrailPoint p;
    p.basePos = basePos;
    p.tipPos = tipPos;
    p.age = kMaxLifeTime;

    points_.push_back(p);

    if (points_.size() > kMaxPoints) {
        points_.erase(points_.begin());
    }
}

void WeaponTrail::StopTrail() {
    isStopped_ = true;
}

void WeaponTrail::Update() {
    if (points_.empty()) return;

    // 寿命を減らす
    for (auto it = points_.begin(); it != points_.end(); ) {
        it->age--;
        if (it->age <= 0) {
            it = points_.erase(it);
        } else {
            ++it;
        }
    }

    if (points_.size() < 2) {
        resource_->indexCount_ = 0;
        return;
    }

    // 頂点とインデックスを更新
    if (!resource_->vertexData_) {
        resource_->Map();
    }

    if (resource_->vertexData_ && resource_->indexData_) {
        int vIndex = 0;
        int iIndex = 0;
        
        // カメラの情報を取得してビルボード的な厚みを計算する準備
        auto cam = engine_->GetCameraManager()->GetActiveCamera();
        Vector3 camPos = cam ? cam->GetTranslate() : Vector3{0, 0, -10};

        // ミルフィーユ状に3枚の平面（リボン）を生成して、カメラから見て常に厚みが見えるようにする
        for (int layer = 0; layer < 3; ++layer) {
            int pIndex = 0;
            int layerStartIndex = vIndex;
            float numSegments = static_cast<float>(points_.size() - 1);

            for (const auto& pt : points_) {
                float alpha = static_cast<float>(pt.age) / static_cast<float>(kMaxLifeTime);
                Vector4 color = baseColor_;
                color.w *= alpha;
                
                // 周辺の層は少し薄くすることで、中心が一番濃い立体感を出す
                if (layer > 0) {
                    color.w *= 0.5f; 
                }

                float v = 1.0f - alpha;

                // 1つ新しい点（または古い点）との差分から「武器の移動方向(moveDir)」を計算する
                Vector3 moveDir = {0, 0, 0};
                if (pIndex < points_.size() - 1) {
                    moveDir = Math::Subtract(points_[pIndex + 1].tipPos, pt.tipPos);
                } else if (pIndex > 0) {
                    moveDir = Math::Subtract(pt.tipPos, points_[pIndex - 1].tipPos);
                } else {
                    moveDir = {0, 1, 0}; // 点が1つしかない場合のフォールバック
                }
                
                if (Math::Length(moveDir) < 0.001f) moveDir = {0, 1, 0};
                else moveDir = Math::Normalize(moveDir);

                // 武器自体の向き(weaponDir)
                Vector3 weaponDir = Math::Subtract(pt.tipPos, pt.basePos);
                if (Math::Length(weaponDir) < 0.001f) weaponDir = {1, 0, 0};
                else weaponDir = Math::Normalize(weaponDir);

                // スイング平面の法線（武器の向きと移動方向の外積）
                // これにより、横振りなら上下(Y)、縦振りなら左右(X)に厚みが出るようになり、
                // カメラの位置に関わらず絶対にねじれ（交差）が発生しなくなる
                Vector3 normal = Math::Cross(weaponDir, moveDir);
                if (Math::Length(normal) < 0.001f) normal = {0, 1, 0};
                else normal = Math::Normalize(normal);

                float offsetMag = 0.0f;
                if (layer == 1) offsetMag = thickness_;
                else if (layer == 2) offsetMag = -thickness_;

                // 常にスイング平面に垂直な方向へ層をズラす
                Vector3 offset = Math::Multiply(offsetMag, normal);

                // 根本 (Base)
                resource_->vertexData_[vIndex].position = { pt.basePos.x + offset.x, pt.basePos.y + offset.y, pt.basePos.z + offset.z, 1.0f };
                resource_->vertexData_[vIndex].texcoord = { 0.0f, v };
                resource_->vertexData_[vIndex].normal = { 0.0f, 1.0f, 0.0f };
                resource_->vertexData_[vIndex].color = color;
                vIndex++;

                // 先端 (Tip)
                resource_->vertexData_[vIndex].position = { pt.tipPos.x + offset.x, pt.tipPos.y + offset.y, pt.tipPos.z + offset.z, 1.0f };
                resource_->vertexData_[vIndex].texcoord = { 1.0f, v };
                resource_->vertexData_[vIndex].normal = { 0.0f, 1.0f, 0.0f };
                resource_->vertexData_[vIndex].color = color;
                vIndex++;

                pIndex++;
            }

            for (size_t i = 0; i < points_.size() - 1; ++i) {
                uint32_t bottomLeft = layerStartIndex + static_cast<uint32_t>(i * 2);
                uint32_t bottomRight = layerStartIndex + static_cast<uint32_t>(i * 2 + 1);
                uint32_t topLeft = layerStartIndex + static_cast<uint32_t>((i + 1) * 2);
                uint32_t topRight = layerStartIndex + static_cast<uint32_t>((i + 1) * 2 + 1);

                // Triangle 1 (Front)
                resource_->indexData_[iIndex++] = bottomLeft;
                resource_->indexData_[iIndex++] = topLeft;
                resource_->indexData_[iIndex++] = bottomRight;

                // Triangle 2 (Front)
                resource_->indexData_[iIndex++] = bottomRight;
                resource_->indexData_[iIndex++] = topLeft;
                resource_->indexData_[iIndex++] = topRight;

                // Triangle 1 (Back - 逆順)
                resource_->indexData_[iIndex++] = bottomLeft;
                resource_->indexData_[iIndex++] = bottomRight;
                resource_->indexData_[iIndex++] = topLeft;

                // Triangle 2 (Back - 逆順)
                resource_->indexData_[iIndex++] = bottomRight;
                resource_->indexData_[iIndex++] = topRight;
                resource_->indexData_[iIndex++] = topLeft;
            }
        }

        resource_->indexCount_ = static_cast<uint32_t>(iIndex);
    }
}

void WeaponTrail::SyncBeforeDraw() {
    if (resource_ && points_.size() >= 2) {
        // Transformがダミーでも、World行列計算のため必要
        resource_->UpdateTransform(*engine_->GetCameraManager()->GetActiveCamera());
        resource_->SyncBeforeDraw();
    }
}

void WeaponTrail::Draw() {
    if (resource_ && points_.size() >= 2 && engine_) {
        // 描画実行直前のバッファ同期
        SyncBeforeDraw();

        // ステートを保存
        BlendMode prevBlend = engine_->currentBlend_;
        PSOManager::DepthWrite prevDepth = engine_->currentDepth_;
        PSOManager::CullMode prevCull = engine_->currentCull_;

        // エフェクト用のステート設定
        engine_->SetBlend(BlendMode::kBlendModeAdd);
        engine_->SetDepthWrite(PSOManager::DepthWrite::Disable);
        engine_->SetCull(PSOManager::CullMode::None); // 両面描画

        if (auto dm = engine_->GetDrawManager()) {
            dm->SubmitStandard3D(resource_.get(), nullptr, false);
        }

        // ステート復元
        engine_->SetBlend(prevBlend);
        engine_->SetDepthWrite(prevDepth);
        engine_->SetCull(prevCull);
    }
}
