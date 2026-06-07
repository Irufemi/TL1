#include "EditorTheme.h"

#ifdef EditorMode
#include <imgui/imgui.h>

void EditorTheme::PushDangerButtonStyle() {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
}

void EditorTheme::PopButtonStyle() {
    ImGui::PopStyleColor(3);
}

#endif // EditorMode
