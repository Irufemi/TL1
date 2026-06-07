#pragma once

#ifdef EditorMode

class EditorManager;
class EditorActionManager;

/**
 * @class EditorShortcutManager
 * @brief エディタ全体のショートカットキー（Deleteキーでの削除、Ctrl+Dでの複製など）を処理する
 */
class EditorShortcutManager {
public:
    EditorShortcutManager(EditorManager* editor, EditorActionManager* actionManager);

    /**
     * @brief 毎フレーム呼び出され、入力を検知してアクションを実行する
     */
    void Update();

private:
    EditorManager* editorManager_ = nullptr;
    EditorActionManager* actionManager_ = nullptr;
};

#endif // EditorMode
