#include "Text.h"
#include "Engine/Graphics/Font/FontManager.h"
#include "Engine/Manager/DrawManager.h"
#include "Engine/Graphics/Camera/Camera.h"
#include "Engine/Graphics/Camera/CameraManager.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Manager/DebugUI.h"
#include <algorithm>

FontManager* Text::fontManager_ = nullptr;
DrawManager* Text::drawManager_ = nullptr;
CameraManager* Text::cameraManager_ = nullptr;
DebugUI* Text::ui_ = nullptr;

Text::Text() {}

void Text::Initialize(const std::string& fontId) {
    resource_ = std::make_unique<Object2DResource>();
    fontId_ = fontId;
    isTextDirty_ = true;

    // マテリアルの初期化
    if (resource_->GetMaterialData()) {
        resource_->GetMaterialData()->color = { 1.0f,1.0f,1.0f,1.0f };
        resource_->GetMaterialData()->enableLighting = false;
        resource_->GetMaterialData()->hasTexture = true;
        resource_->GetMaterialData()->lightingMode = 2; // Unlit相当
        resource_->GetMaterialData()->uvTransform = Math::MakeIdentity4x4();
    }
}

void Text::SetText(const std::wstring& text) {
    if (text_ != text) {
        text_ = text;
        isTextDirty_ = true;
        isDirty_ = true;
    }
}

void Text::SetFontId(const std::string& fontId) {
    if (fontId_ != fontId) {
        fontId_ = fontId;
        isTextDirty_ = true;
        isDirty_ = true;
    }
}

void Text::GenerateVertices() {
    if (!fontManager_) return;

    // 文字が存在しない可能性があるため、まずは非同期生成要求をかける
    fontManager_->PrecacheText(fontId_, text_);

    resource_->vertexDataList_.clear();
    resource_->indexDataList_.clear();

    if (text_.empty()) {
        localBoundsMin_ = {0.0f, 0.0f};
        localBoundsMax_ = {0.0f, 0.0f};
        return;
    }

    std::vector<float> lineWidths;
    float currentLineWidth = 0.0f;
    float scaleFactor = baseScale_ / 32.0f; // GLYPH_SIZE(32)を基準にスケール
    
    bool hasPendingGlyphs = false;

    for (wchar_t c : text_) {
        if (c == L'\n') {
            lineWidths.push_back(currentLineWidth);
            currentLineWidth = 0.0f;
            continue;
        }
        const auto* glyph = fontManager_->GetGlyph(fontId_, c);
        if (glyph) {
            if (glyph->width < 0.0f) {
                hasPendingGlyphs = true;
            }
            currentLineWidth += glyph->advanceX * scaleFactor;
        }
    }
    lineWidths.push_back(currentLineWidth);

    auto getStartX = [&](int lIndex) {
        if (alignment_ == TextAlignment::Center) {
            return -lineWidths[lIndex] * 0.5f;
        } else if (alignment_ == TextAlignment::Right) {
            return -lineWidths[lIndex];
        }
        return 0.0f; // Left
    };

    int lineIndex = 0;
    float currentX = getStartX(lineIndex);
    float currentY = 0.0f;

    float minX = (std::numeric_limits<float>::max)();
    float maxX = std::numeric_limits<float>::lowest();
    float minY = (std::numeric_limits<float>::max)();
    float maxY = std::numeric_limits<float>::lowest();

    for (wchar_t c : text_) {
        if (c == L'\n') {
            lineIndex++;
            currentX = getStartX(lineIndex);
            currentY += scaleFactor * 32.0f * 1.2f; // 行間 (Y下方向が正)
            continue;
        }

        const auto* glyph = fontManager_->GetGlyph(fontId_, c);
        if (!glyph) {
            continue; // 未知の文字
        }

        if (glyph->width < 0.0f) {
            // ダミーキャッシュ(非同期生成中)
            hasPendingGlyphs = true;
            currentX += glyph->advanceX * scaleFactor;
            continue;
        } else if (glyph->width == 0.0f || glyph->height == 0.0f) {
            // スペースなどの空白文字
            currentX += glyph->advanceX * scaleFactor;
            continue;
        }

        // MSDFのbitmapはFontManagerで上下反転してAtlasに格納されているため、
        // uvTopLeft.y が「文字の上端」、uvBottomRight.y が「文字の下端」になります。
        // Screen座標はYが下が正なので、top < bottom となります。
        float left = currentX + glyph->offsetX * scaleFactor;
        float right = left + glyph->width * scaleFactor;
        float top = currentY + glyph->offsetY * scaleFactor;
        float bottom = top + glyph->height * scaleFactor;

        minX = (std::min)(minX, left);
        maxX = (std::max)(maxX, right);
        minY = (std::min)(minY, top);
        maxY = (std::max)(maxY, bottom);

        uint32_t startIndex = static_cast<uint32_t>(resource_->vertexDataList_.size());

        // 頂点の並び: 左下(0), 左上(1), 右下(2), 右上(3)
        // Spriteコンポーネントに合わせて法線は Z=-1
        resource_->vertexDataList_.push_back({ { left,  bottom, 0.0f, 1.0f }, { glyph->uvTopLeft.x, glyph->uvBottomRight.y }, {0.0f,0.0f,-1.0f}, {1.0f, 1.0f, 1.0f, 1.0f} });
        resource_->vertexDataList_.push_back({ { left,  top,    0.0f, 1.0f }, { glyph->uvTopLeft.x, glyph->uvTopLeft.y },     {0.0f,0.0f,-1.0f}, {1.0f, 1.0f, 1.0f, 1.0f} });
        resource_->vertexDataList_.push_back({ { right, bottom, 0.0f, 1.0f }, { glyph->uvBottomRight.x, glyph->uvBottomRight.y }, {0.0f,0.0f,-1.0f}, {1.0f, 1.0f, 1.0f, 1.0f} });
        resource_->vertexDataList_.push_back({ { right, top,    0.0f, 1.0f }, { glyph->uvBottomRight.x, glyph->uvTopLeft.y },     {0.0f,0.0f,-1.0f}, {1.0f, 1.0f, 1.0f, 1.0f} });

        resource_->indexDataList_.push_back(startIndex + 0);
        resource_->indexDataList_.push_back(startIndex + 1);
        resource_->indexDataList_.push_back(startIndex + 2);
        resource_->indexDataList_.push_back(startIndex + 1);
        resource_->indexDataList_.push_back(startIndex + 3);
        resource_->indexDataList_.push_back(startIndex + 2);

        currentX += glyph->advanceX * scaleFactor;
    }

    if (minX <= maxX && minY <= maxY) {
        localBoundsMin_ = { minX, minY };
        localBoundsMax_ = { maxX, maxY };
    } else {
        localBoundsMin_ = { 0.0f, 0.0f };
        localBoundsMax_ = { 0.0f, 0.0f };
    }

    if (hasPendingGlyphs) {
        // 次のフレームで再試行するためにフラグを立てる
        isTextDirty_ = true;
    }

    // SRVを設定
    resource_->textureHandle_ = fontManager_->GetAtlasSRV();

    // 頂点がなければ終了
    if (resource_->vertexDataList_.empty()) return;

    // GPUリソースの再生成(文字数によって頂点数が可変なため、毎回バッファを作り直すか拡張する)
    resource_->CreateResource();
    resource_->Map();
    if (resource_->vertexData_) {
        std::copy(resource_->vertexDataList_.begin(), resource_->vertexDataList_.end(), resource_->vertexData_);
    }
    if (resource_->indexData_) {
        std::copy(resource_->indexDataList_.begin(), resource_->indexDataList_.end(), resource_->indexData_);
    }
}

void Text::Update() {
    if (!resource_ || !cameraManager_ || !fontManager_) return;
    
    // AtlasのSRVが変わったか(リビルドされた等)、テキストに変更があった場合は再生成
    D3D12_GPU_DESCRIPTOR_HANDLE currentAtlas = fontManager_->GetAtlasSRV();
    if (lastAtlasSrv_.ptr != currentAtlas.ptr) {
        isTextDirty_ = true;
        lastAtlasSrv_ = currentAtlas;
    }

    if (isTextDirty_) {
        isTextDirty_ = false;
        GenerateVertices();
        isDirty_ = true;
    }

    Camera* activeCam = cameraManager_->GetActiveCamera();
    if (!activeCam) return;

    if (isDirty_) {
        resource_->UpdateTransform(*activeCam);
        isDirty_ = false;
    }

    lastViewMatrix_ = activeCam->GetViewMatrix();
    lastProjectionMatrix_ = activeCam->GetOrthographicMatrix();
}

void Text::SyncBeforeDraw() {
    if (resource_) {
        resource_->SyncBeforeDraw();
    }
}

void Text::Draw() {
    if (!resource_ || !drawManager_ || !cameraManager_) return;
    Camera* activeCam = cameraManager_->GetActiveCamera();
    if (!activeCam) return;

    bool cameraChanged = (std::memcmp(&lastViewMatrix_, &activeCam->GetViewMatrix(), sizeof(Matrix4x4)) != 0 ||
                          std::memcmp(&lastProjectionMatrix_, &activeCam->GetOrthographicMatrix(), sizeof(Matrix4x4)) != 0);

    if (isDirty_ || cameraChanged || isTextDirty_) {
        Update();
    }
    
    SyncBeforeDraw();

    if (resource_->vertexDataList_.empty()) return; // 描画するものがなければスキップ

    // TextRenderer用の描画キューに送信
    if (isTopMost_) {
        drawManager_->SubmitTopMostText(resource_.get());
    } else {
        drawManager_->SubmitText(resource_.get());
    }
}

void Text::DrawOutlineMask() {
    if (!resource_ || !drawManager_ || !cameraManager_) return;
    Camera* activeCam = cameraManager_->GetActiveCamera();
    if (!activeCam) return;

    if (isDirty_ || isTextDirty_) {
        Update();
    }
    SyncBeforeDraw();

    if (resource_->vertexDataList_.empty()) return;

    drawManager_->SubmitTextOutlineMask(resource_.get());
}

