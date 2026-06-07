#pragma once

#ifdef EditorMode

/**
 * @namespace EditorDragDrop
 * @brief エディタ内のドラッグ＆ドロップで使用するペイロード識別子
 */
namespace EditorDragDrop {
    constexpr const char* PayloadGameObject = "GAMEOBJECT";
    constexpr const char* PayloadAssetPath  = "DND_ASSET_PATH";
}

#endif // EditorMode
