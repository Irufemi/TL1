#pragma once

#include <string>
#include <vector>
#include <memory>
#include <d3d12.h>
#include "Engine/Core/Math/Vector2.h"
#include "Engine/Core/System/TaskGroup.h"

class IrufemiEngine;

/**
 * @struct GlyphInfo
 * @brief フォントの1文字（グリフ）の描画に必要な情報
 */
struct GlyphInfo {
    char32_t character;
    Vector2 uvTopLeft;
    Vector2 uvBottomRight;
    float width;
    float height;
    float offsetX;    // 描画基準点からのXズレ (Bearing X)
    float offsetY;    // 描画基準点からのYズレ (Bearing Y)
    float advanceX;   // 次の文字へのカーソル移動量
};

/**
 * @class FontManager
 * @brief TTFフォントから実行時にMSDFを動的生成し、アトラスとして管理するクラス
 */
class FontManager {
public:
    FontManager();
    ~FontManager();

    // 初期化 (FreeTypeなどの初期化、アトラステクスチャの作成)
    void Initialize(IrufemiEngine* engine);
    void Finalize();

    // TTFフォントをロードし、管理対象として登録する
    bool LoadFont(const std::string& fontId, const std::string& ttfPath);

    // 指定フォルダ配下を再帰的に走査し、.ttf / .otf を一括ロードする
    void LoadAllFromFolder(const std::string& folderPath);

    // ロード済みのフォントIDリストを取得する
    std::vector<std::string> GetLoadedFontIds() const;

    // 文字列を受け取り、まだ生成されていない文字があればMSDFを生成してアトラスに追加する
    void PrecacheText(const std::string& fontId, const std::wstring& text);

    // 文字のグリフ情報を取得する (キャッシュにない場合は非同期または即座に生成する)
    const GlyphInfo* GetGlyph(const std::string& fontId, char32_t character);

    // シェーダに渡すための動的アトラスSRV(テクスチャ)を取得
    D3D12_GPU_DESCRIPTOR_HANDLE GetAtlasSRV() const;

    // 非同期生成タスクがすべて完了しているかを取得
    bool IsAllLoaded() const;

private:
    // Pimplイディオム：FreeType, msdfgen, stb_rect_pack などの依存を隠蔽する
    struct Impl;
    std::unique_ptr<Impl> impl_;
    
    IrufemiEngine* engine_ = nullptr;
};
