#pragma once

#ifdef EditorMode
#include "../IEditorPanel.h"

/**
 * @class HierarchyPanel
 * @brief シーン内のGameObjectの階層構造を描画・編集するパネル
 */
class HierarchyPanel : public IEditorPanel {
public:
    void Initialize(EditorManager* editorManager) override;
    void Draw() override;

private:
    EditorManager* editorManager_ = nullptr;
};

#endif // EditorMode
