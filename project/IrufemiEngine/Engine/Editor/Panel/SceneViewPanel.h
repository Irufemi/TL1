#pragma once

#ifdef EditorMode
#include "../IEditorPanel.h"
#include "Engine/Core/Math/Vector3.h"
#include <imgui.h>
#include "imgui/ImGuizmo.h"
#include "Engine/Editor/Utils/EditorCameraController.h"

/**
 * @class SceneViewPanel
 * @brief エディタのSceneビューを描画するパネル
 */
class SceneViewPanel : public IEditorPanel {
public:
    void Initialize(EditorManager* editorManager) override;
    void Draw() override;

private:
    EditorManager* editorManager_ = nullptr;
    EditorCameraController cameraController_;

    // --- ギズモ用状態 ---
    ImGuizmo::OPERATION currentGizmoOperation_ = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE currentGizmoMode_ = ImGuizmo::LOCAL;

    // --- 内部ヘルパーメソッド ---
    void DrawImGuizmo(ImVec2 minPos, ImVec2 size);
    void HandleDragAndDrop();
    void HandlePicking(ImVec2 mousePos, ImVec2 minPos, ImVec2 maxPos, ImVec2 size);
};

#endif // EditorMode
