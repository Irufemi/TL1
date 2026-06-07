#include "FontManager.h"
#include "Engine/IrufemiEngine.h"
#include "Engine/Graphics/DirectX/DirectXCommon.h"
#include "Engine/Core/System/ThreadPool.h"
#include "Engine/Core/Utility/Log.h"

// --- 外部ライブラリ群 ---
#include <ft2build.h>
#include FT_FREETYPE_H

#undef min
#undef max
#include <msdfgen/msdfgen.h>
#include <msdfgen/ext/import-font.h>

#define STB_RECT_PACK_IMPLEMENTATION
#include <stb/stb_rect_pack.h>

#include <DirectXTex/DirectXTex.h>
#include "Engine/Graphics/DirectX/DirectXUtils.h"
#include "Engine/Graphics/DirectX/DescriptorPool.h"

#include <unordered_map>
#include <vector>
#include <mutex>
#include <filesystem>
#include <algorithm>
#include <algorithm>

// 内部実装の定義
struct FontManager::Impl {
    msdfgen::FreetypeHandle* ftLibrary = nullptr;
    std::unordered_map<std::string, msdfgen::FontHandle*> fonts;

    // キャッシュされた文字情報: FontID -> (文字コード -> GlyphInfo)
    std::unordered_map<std::string, std::unordered_map<char32_t, GlyphInfo>> glyphCache;
    std::mutex cacheMutex;

    // アトラス管理 (stb_rect_pack用)
    stbrp_context packContext;
    std::vector<stbrp_node> packNodes;
    std::vector<uint8_t> cpuAtlasData; // RGBA
    
    // DirectX12 用の動的テクスチャリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> atlasTexture;
    D3D12_GPU_DESCRIPTOR_HANDLE atlasSrv{};

    static const int ATLAS_WIDTH = 2048;
    static const int ATLAS_HEIGHT = 2048;
    static const int GLYPH_SIZE = 32; // ベースサイズ
    static const int PADDING = 2; // エッジパディング
    static constexpr double PX_RANGE = 2.0; // 距離場スプレッド

    // 非同期生成用スレッドプールとタスクグループ
    std::unique_ptr<ThreadPool> threadPool;
    std::shared_ptr<TaskGroup> taskGroup;
};

FontManager::FontManager() : impl_(std::make_unique<Impl>()) {}

FontManager::~FontManager() {
    Finalize();
}

void FontManager::Initialize(IrufemiEngine* engine) {
    engine_ = engine;
    
    // FreeTypeの初期化
    impl_->ftLibrary = msdfgen::initializeFreetype();
    
    // CPUアトラスの初期化
    impl_->cpuAtlasData.resize(Impl::ATLAS_WIDTH * Impl::ATLAS_HEIGHT * 4, 0); // 0クリア(RGBA)
    
    // stb_rect_pack のコンテキスト初期化
    impl_->packNodes.resize(Impl::ATLAS_WIDTH);
    stbrp_init_target(&impl_->packContext, Impl::ATLAS_WIDTH, Impl::ATLAS_HEIGHT, impl_->packNodes.data(), static_cast<int>(impl_->packNodes.size()));

    // 2048x2048 のアトラステクスチャ生成 (DirectX12)
    DirectX::TexMetadata metadata{};
    metadata.width = Impl::ATLAS_WIDTH;
    metadata.height = Impl::ATLAS_HEIGHT;
    metadata.depth = 1;
    metadata.arraySize = 1;
    metadata.mipLevels = 1;
    metadata.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    metadata.dimension = DirectX::TEX_DIMENSION_TEXTURE2D;

    impl_->atlasTexture = engine_->GetDirectXCommon()->CreateTextureResource(metadata);

    // デスクリプタの確保とSRVの作成
    DescriptorPool* pool = engine_->GetDirectXCommon()->GetSrvPool();
    uint32_t srvIndex = pool->Allocate();
    pool->CreateSRVForTexture2D(srvIndex, impl_->atlasTexture.Get(), DXGI_FORMAT_R8G8B8A8_UNORM, 1);
    impl_->atlasSrv = pool->GetGPUHandle(srvIndex);

    // テクスチャ作成直後は COPY_DEST なので、後続の更新処理（GENERIC_READ -> COPY_DEST）に
    // 合わせるために一度 GENERIC_READ に遷移しておく
    engine_->GetDirectXCommon()->ExecuteUploadCommands([&](ID3D12GraphicsCommandList* cmdList) {
        DirectXUtils::TransitionBarrier(cmdList, impl_->atlasTexture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
    });

    // スレッドプールの初期化 (1スレッド) とタスクグループの初期化
    impl_->threadPool = std::make_unique<ThreadPool>(1);
    impl_->taskGroup = std::make_shared<TaskGroup>();
}

void FontManager::Finalize() {
    impl_->threadPool.reset(); // スレッドプールの停止を待機

    for (auto& pair : impl_->fonts) {
        if (pair.second) {
            msdfgen::destroyFont(pair.second);
        }
    }
    impl_->fonts.clear();

    if (impl_->ftLibrary) {
        msdfgen::deinitializeFreetype(impl_->ftLibrary);
        impl_->ftLibrary = nullptr;
    }
}

bool FontManager::LoadFont(const std::string& fontId, const std::string& ttfPath) {
    msdfgen::FontHandle* font = msdfgen::loadFont(impl_->ftLibrary, ttfPath.c_str());
    if (font) {
        std::lock_guard<std::mutex> lock(impl_->cacheMutex);
        impl_->fonts[fontId] = font;
        return true;
    }
    return false;
}

void FontManager::LoadAllFromFolder(const std::string& folderPath) {
    if (!std::filesystem::exists(folderPath)) { return; }

    for (auto& entry : std::filesystem::recursive_directory_iterator(folderPath)) {
        if (!entry.is_regular_file()) { continue; }
        
        auto p = entry.path();
        auto ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        
        if (ext == ".ttf" || ext == ".otf") {
            // ファイル名(拡張子なし)を fontId としてロードする
            std::string fontId = p.stem().string();
            std::string ttfPath = p.generic_string();
            LoadFont(fontId, ttfPath);
        }
    }
}

std::vector<std::string> FontManager::GetLoadedFontIds() const {
    std::vector<std::string> ids;
    for (const auto& pair : impl_->fonts) {
        ids.push_back(pair.first);
    }
    return ids;
}

bool FontManager::IsAllLoaded() const {
    return impl_->taskGroup->IsAllDone();
}

void FontManager::PrecacheText(const std::string& fontId, const std::wstring& text) {
    // 非同期でタスクを投げる（シーンのロード画面等で待機可能にするため）
    impl_->threadPool->Enqueue(impl_->taskGroup, [this, fontId, text]() {
        std::lock_guard<std::mutex> lock(impl_->cacheMutex);
        auto it = impl_->fonts.find(fontId);
        if (it == impl_->fonts.end()) return;
        msdfgen::FontHandle* font = it->second;

        auto& fontCache = impl_->glyphCache[fontId];

        msdfgen::FontMetrics metrics;
        msdfgen::getFontMetrics(metrics, font, msdfgen::FONT_SCALING_NONE);

        for (wchar_t c : text) {
            char32_t char32 = static_cast<char32_t>(c);
            
            auto cacheIt = fontCache.find(char32);
            // キャッシュに存在しない、またはダミー（width < 0）の場合は生成
            if (cacheIt == fontCache.end() || cacheIt->second.width < 0.0f) {
                msdfgen::Shape shape;
                double advance = 0.0;
                
                // スペースなどの空文字対策
                if (char32 == U' ') {
                    double spaceAdvance = 0.0, tabAdvance = 0.0;
                    msdfgen::getFontWhitespaceWidth(spaceAdvance, tabAdvance, font, msdfgen::FONT_SCALING_NONE);
                    GlyphInfo info{};
                    info.character = char32;
                    info.advanceX = static_cast<float>(spaceAdvance);
                    fontCache[char32] = info;
                    continue;
                }

                if (msdfgen::loadGlyph(shape, font, char32, msdfgen::FONT_SCALING_NONE, &advance)) {
                    shape.normalize();
                    shape.orientContours();
                    msdfgen::edgeColoringSimple(shape, 3.0);

                    msdfgen::Shape::Bounds bounds = shape.getBounds();
                    // フォントの高さと幅を算出
                    double width = bounds.r - bounds.l;
                    double height = bounds.t - bounds.b;

                    // スケールの決定 (GLYPH_SIZE に収まるように)
                    double scale = static_cast<double>(Impl::GLYPH_SIZE) / (metrics.emSize > 0.0 ? metrics.emSize : 1.0);
                    
                    int texWidth = static_cast<int>(width * scale) + Impl::PADDING * 2;
                    int texHeight = static_cast<int>(height * scale) + Impl::PADDING * 2;

                    if (texWidth <= 0 || texHeight <= 0) continue;

                    // オフセット計算 (テクスチャ中央に配置)
                    msdfgen::Vector2 translate(-bounds.l + (Impl::PADDING / scale), -bounds.b + (Impl::PADDING / scale));

                    // MSDF用ビットマップ
                    msdfgen::Bitmap<float, 3> msdf(texWidth, texHeight);
                    msdfgen::generateMSDF(msdf, shape, msdfgen::Projection(msdfgen::Vector2(scale), translate), Impl::PX_RANGE, msdfgen::MSDFGeneratorConfig());

                    // stbrp_pack_rects でパッキング
                    stbrp_rect rect{};
                    rect.w = texWidth;
                    rect.h = texHeight;
                    if (stbrp_pack_rects(&impl_->packContext, &rect, 1) == 1) {
                        // CPUアトラスにコピー (上下反転に対応するためY座標を反転しながらコピー)
                        for (int y = 0; y < texHeight; ++y) {
                            for (int x = 0; x < texWidth; ++x) {
                                const float* pixel = msdf(x, texHeight - 1 - y); // Y反転
                                int destX = rect.x + x;
                                int destY = rect.y + y;
                                int destIndex = (destY * Impl::ATLAS_WIDTH + destX) * 4;
                                float pxRange = static_cast<float>(Impl::PX_RANGE);
                                impl_->cpuAtlasData[destIndex + 0] = static_cast<uint8_t>(std::clamp((pixel[0] / pxRange + 0.5f) * 255.f, 0.f, 255.f)); // R
                                impl_->cpuAtlasData[destIndex + 1] = static_cast<uint8_t>(std::clamp((pixel[1] / pxRange + 0.5f) * 255.f, 0.f, 255.f)); // G
                                impl_->cpuAtlasData[destIndex + 2] = static_cast<uint8_t>(std::clamp((pixel[2] / pxRange + 0.5f) * 255.f, 0.f, 255.f)); // B
                                impl_->cpuAtlasData[destIndex + 3] = 255; // A
                            }
                        }

                        // GlyphInfo作成
                        GlyphInfo info{};
                        info.character = char32;
                        info.uvTopLeft = Vector2(static_cast<float>(rect.x) / Impl::ATLAS_WIDTH, static_cast<float>(rect.y) / Impl::ATLAS_HEIGHT);
                        info.uvBottomRight = Vector2(static_cast<float>(rect.x + rect.w) / Impl::ATLAS_WIDTH, static_cast<float>(rect.y + rect.h) / Impl::ATLAS_HEIGHT);
                        info.width = static_cast<float>(rect.w);
                        info.height = static_cast<float>(rect.h);
                        info.offsetX = static_cast<float>(bounds.l * scale) - Impl::PADDING;
                        info.offsetY = static_cast<float>((metrics.ascenderY - bounds.t) * scale) - Impl::PADDING; // ベースライン基準のオフセット
                        info.advanceX = static_cast<float>(advance * scale);

                        fontCache[char32] = info;

                        // --- VRAM(GPU)への部分転送 ---
                        // DirectX12のRowPitchは256バイト境界である必要がある
                        uint32_t alignedRowPitch = (rect.w * 4 + 255) & ~255;
                        uint32_t slicePitch = alignedRowPitch * rect.h;

                        auto intermediateResource = engine_->GetDirectXCommon()->CreateBufferResource(slicePitch);

                        uint8_t* mappedData = nullptr;
                        if (SUCCEEDED(intermediateResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedData)))) {
                            for (int y = 0; y < rect.h; ++y) {
                                int srcY = rect.y + y;
                                int srcIndex = (srcY * Impl::ATLAS_WIDTH + rect.x) * 4;
                                std::memcpy(mappedData + y * alignedRowPitch, &impl_->cpuAtlasData[srcIndex], rect.w * 4);
                            }
                            intermediateResource->Unmap(0, nullptr);

                            engine_->GetDirectXCommon()->ExecuteUploadCommands([&](ID3D12GraphicsCommandList* cmdList) {
                                DirectXUtils::TransitionBarrier(cmdList, impl_->atlasTexture.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_DEST);

                                D3D12_TEXTURE_COPY_LOCATION dst{};
                                dst.pResource = impl_->atlasTexture.Get();
                                dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                                dst.SubresourceIndex = 0;

                                D3D12_TEXTURE_COPY_LOCATION src{};
                                src.pResource = intermediateResource.Get();
                                src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                                src.PlacedFootprint.Offset = 0;
                                src.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                                src.PlacedFootprint.Footprint.Width = rect.w;
                                src.PlacedFootprint.Footprint.Height = rect.h;
                                src.PlacedFootprint.Footprint.Depth = 1;
                                src.PlacedFootprint.Footprint.RowPitch = alignedRowPitch;

                                D3D12_BOX srcBox{};
                                srcBox.left = 0;
                                srcBox.top = 0;
                                srcBox.right = rect.w;
                                srcBox.bottom = rect.h;
                                srcBox.front = 0;
                                srcBox.back = 1;

                                cmdList->CopyTextureRegion(&dst, rect.x, rect.y, 0, &src, &srcBox);

                                DirectXUtils::TransitionBarrier(cmdList, impl_->atlasTexture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
                            });

                            engine_->GetDirectXCommon()->ReleaseAfterFence(intermediateResource);
                        }
                    } else {
                        // アトラスがいっぱいの場合は、毎フレーム再生成を試みるのを防ぐため、仮の文字として登録
                        OutputDebugStringA("Font atlas is full! Could not pack glyph for character.\n");
                        GlyphInfo info{};
                        info.character = char32;
                        info.width = 0.0f; // 描画されない
                        info.advanceX = static_cast<float>(advance * scale); // 幅だけは進める
                        fontCache[char32] = info;
                    }
                }
            }
        }
    });
}

const GlyphInfo* FontManager::GetGlyph(const std::string& fontId, char32_t character) {
    {
        std::lock_guard<std::mutex> lock(impl_->cacheMutex);
        auto& fontCache = impl_->glyphCache[fontId];
        auto it = fontCache.find(character);
        
        // すでに正常な文字データが存在する場合はそれを返す
        if (it != fontCache.end() && it->second.width >= 0.0f) {
            return &it->second;
        }

        // ダミー登録 (複数スレッドからの二重生成リクエスト防止)
        if (it == fontCache.end()) {
            GlyphInfo dummy{};
            dummy.character = character;
            dummy.width = -1.0f; // 未生成状態を示すフラグとして width = -1 を使用
            fontCache[character] = dummy;
        } else if (it->second.width < 0.0f) {
            // すでに生成タスクが走っているのでダミーを返す
            return &it->second;
        }
    }

    // 非同期で生成タスクを投げる
    std::wstring singleChar;
    singleChar += static_cast<wchar_t>(character);
    
    // 既存のPrecacheTextと同様にタスクグループ経由で生成リクエスト
    impl_->threadPool->Enqueue(impl_->taskGroup, [this, fontId, singleChar]() {
        // PrecacheText 自体が Enqueue するため、直接内部処理を呼ぶか、そのままPrecacheTextを呼ぶか
        // PrecacheTextは関数全体がEnqueueになったので、ここでPrecacheTextを呼ぶとさらにEnqueueされる。
        PrecacheText(fontId, singleChar);
    });
    
    std::lock_guard<std::mutex> lock(impl_->cacheMutex);
    return &impl_->glyphCache[fontId][character];
}

D3D12_GPU_DESCRIPTOR_HANDLE FontManager::GetAtlasSRV() const {
    return impl_->atlasSrv;
}
