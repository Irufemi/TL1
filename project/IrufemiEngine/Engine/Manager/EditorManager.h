#pragma once

#ifdef EditorMode
#include <memory>
#include <vector>
#include <string>

class IrufemiEngine;
class GameObject;
class IEditorPanel;
class EditorActionManager;
class EditorShortcutManager;
class ComponentEditorRegistry;



/**
 * @brief エディタの現在の動作モード
 */
enum class EditorModeState {
    Edit,
    Play
};

/**
 * @class EditorManager
 * @brief エディタのUIレイアウト（DockSpace、SceneViewなど）を統括するマネージャ
 */
class EditorManager {
public:
    EditorManager();
    ~EditorManager();

    void Initialize(IrufemiEngine* engine);
    void Update();
    void DrawEditorUI();

    /** @name 各パネルからアクセスするための状態管理 Getter/Setter */
    ///@{
    IrufemiEngine* GetEngine() const { return engine_; }
    
    EditorActionManager* GetActionManager() const { return actionManager_.get(); }
    EditorShortcutManager* GetShortcutManager() const { return shortcutManager_.get(); }
    ComponentEditorRegistry* GetComponentEditorRegistry() const { return componentEditorRegistry_.get(); }
    
    std::shared_ptr<GameObject> GetSelectedObject() const { return selectedObject_.lock(); }
    void SetSelectedObject(std::shared_ptr<GameObject> obj) { selectedObject_ = obj; }
    void ClearSelectedObject() { selectedObject_.reset(); }
    
    EditorModeState GetCurrentMode() const { return currentMode_; }
    bool IsPlayMode() const { return currentMode_ == EditorModeState::Play; }
    ///@}

    /**
     * @brief 編集モードからプレイモードへ移行し、現在のシーン状態を一時保存する
     */
    void EnterPlayMode();

    /**
     * @brief プレイモードから編集モードへ戻り、シーン状態を一時保存から復元する
     */
    void ExitPlayMode();

private:

    IrufemiEngine* engine_ = nullptr;
    std::weak_ptr<GameObject> selectedObject_;
    EditorModeState currentMode_ = EditorModeState::Edit;
    std::string playModeStartSceneName_ = "";

    std::unique_ptr<EditorActionManager> actionManager_;
    std::unique_ptr<EditorShortcutManager> shortcutManager_;
    std::unique_ptr<ComponentEditorRegistry> componentEditorRegistry_;

    // 各エディタパネル
    std::vector<std::unique_ptr<IEditorPanel>> panels_;
};

#endif // EditorMode
