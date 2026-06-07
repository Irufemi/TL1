#pragma once

#ifdef EditorMode
#include "../IEditorPanel.h"

/**
 * @class InspectorPanel
 * @brief 選択されたGameObjectのコンポーネントを描画するパネル
 */
class InspectorPanel : public IEditorPanel {
public:
    void Initialize(EditorManager* editorManager) override;
    void Draw() override;

private:
    EditorManager* editorManager_ = nullptr;
};

#endif // EditorMode
