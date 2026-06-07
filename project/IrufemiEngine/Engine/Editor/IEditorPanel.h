#pragma once

#ifdef EditorMode

class EditorManager;

/**
 * @class IEditorPanel
 * @brief エディタの各パネルの基底となるインターフェース
 */
class IEditorPanel {
public:
    virtual ~IEditorPanel() = default;

    /**
     * @brief パネルの初期化
     * @param[in] editorManager 親となるEditorManagerのポインタ
     */
    virtual void Initialize(EditorManager* editorManager) = 0;

    /**
     * @brief パネルの描画
     */
    virtual void Draw() = 0;
};

#endif // EditorMode
