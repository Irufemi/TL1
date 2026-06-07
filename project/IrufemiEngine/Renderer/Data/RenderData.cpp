#include "Renderer/Data/RenderData.h"
#include "Engine/Manager/PrimitiveManager.h"
#include "Resource/Texture/TextureManager.h"
#include "Engine/Graphics/Camera/Camera.h"

// --- TransformComponent ---

void PrimitiveTransform::UpdateTransform(Object3DResource* resource, const Camera& camera) {
    if (!resource) return;

    // 行列の更新
    // 既存の Object3DResource::UpdateTransform は内部で world 行列を再計算するため、
    // ここでは Transform の値を resource に同期させるだけで済む
    resource->transform_ = transform;
    resource->UpdateTransform(camera);

    isDirty = false;
}

// --- MeshComponent ---

void MeshDesc::ChangeMesh(PrimitiveType newType) {
    type = newType;

    // PrimitiveManager から標準リソースを取得
    const auto& primitiveResource = PrimitiveManager::GetInstance()->GetStandardResource(type);

    bool isNewResource = false;
    if (!resource) {
        resource = std::make_unique<Object3DResource>();
        isNewResource = true;
    }

    // リソースの共有設定
    resource->vertexBufferView_ = primitiveResource.vertexBufferView;
    resource->indexBufferView_ = primitiveResource.indexBufferView;
    resource->indexCount_ = primitiveResource.indexCount;

    // 定数バッファ等の生成は最初だけ行う
    if (isNewResource) {
        resource->CreateResource();
        resource->Map();
    }
}

void MeshDesc::ChangeMesh(const PrimitiveData& data) {
    if (!resource) {
        resource = std::make_unique<Object3DResource>();
        resource->CreateResource();
        resource->Map();
    }

    // 動的にリソースを生成し、自身で所有権を持つ
    PrimitiveResource customResource;
    PrimitiveManager::GetInstance()->CreateGPUResource(data, customResource);

    resource->vertexResource_ = customResource.vertexResource;
    resource->indexResource_ = customResource.indexResource;
    resource->vertexBufferView_ = customResource.vertexBufferView;
    resource->indexBufferView_ = customResource.indexBufferView;
    resource->indexCount_ = customResource.indexCount;
}

// --- MaterialComponent ---

void MaterialDesc::UpdateMaterial(Object3DResource* resource, TextureManager* textureManager) {
    if (!resource || !resource->GetMaterialData()) return;

    // マテリアルパラメータの反映
    resource->GetMaterialData()->color = color;
    resource->GetMaterialData()->enableLighting = enableLighting;
    resource->GetMaterialData()->lightingMode = lightingMode;
    resource->GetMaterialData()->metallic = metallic;
    resource->GetMaterialData()->roughness = roughness;
    resource->GetMaterialData()->hasTexture = !texturePath.empty();
    resource->GetMaterialData()->uvTransform = uvTransform;
    resource->GetMaterialData()->alphaReference = alphaReference;
    resource->GetMaterialData()->useClampSampler = useClampSampler;

    // テクスチャハンドルの更新
    if (textureManager && !texturePath.empty()) {
        resource->textureHandle_ = textureManager->GetTextureHandle(texturePath);
        
        // ハンドルが取得できなかった場合はテクスチャ無効にする
        if (resource->textureHandle_.ptr == 0) {
            resource->GetMaterialData()->hasTexture = false;
        }
    } else {
        resource->GetMaterialData()->hasTexture = false;
    }

    // テクスチャハンドルが無効（0）の場合は、強制的に白テクスチャを割り当てることでGPUバリデーションエラーを防ぐ
    if (resource->textureHandle_.ptr == 0 && textureManager) {
        resource->textureHandle_ = textureManager->GetTextureHandle("white");
    }
    
    resource->MarkAsDirty();
}
