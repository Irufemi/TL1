#pragma once

#include <cstdint>

// 前方宣言
class IrufemiEngine;
class GameObject;
#include <memory>
#include <vector>

/// <summary>
/// Scene系クラスに継承する基底クラス
/// </summary>
class IScene {
public:
    virtual ~IScene() = default;

    // --- 基本サイクル関数 ---

    /**
     * @brief シーンの初期化処理。シーン生成直後に1度だけ呼ばれます。
     * @param[in] engine エンジンのポインタ
     */
    virtual void Initialize(IrufemiEngine* engine) = 0;

    /**
     * @brief シーンの毎フレームの更新処理。
     */
    virtual void Update() = 0;

    /**
     * @brief シーンの毎フレームの描画処理。
     */
    virtual void Draw() = 0;

    /**
     * @brief シーンが保持する GameObject のリストを取得する
     */
    virtual const std::vector<std::shared_ptr<GameObject>>& GetGameObjects() const { 
        static std::vector<std::shared_ptr<GameObject>> empty; 
        return empty; 
    }

    // --- ライフサイクル管理機能 ---

    /**
     * @brief シーンの終了処理。シーンが破棄される直前に1度だけ呼ばれます。
     * @details メモリ解放や外部リソースのクリーンアップなどを行います。
     */
    virtual void Finalize() {}

    /**
     * @brief シーンがスタックに積まれ、最前面でアクティブになった時に呼ばれます。
     */
    virtual void OnEnter() {}

    /**
     * @brief シーンが破棄される直前、または完全に非アクティブになる時に呼ばれます。
     */
    virtual void OnExit() {}

    /**
     * @brief 上に別のシーンがPushされ、このシーンがバックグラウンドに回った時に呼ばれます。
     * @details 一時停止（Pause）時の状態保存などに利用します。
     */
    virtual void OnSuspend() {}

    /**
     * @brief 上のシーンがPopされ、このシーンが再び最前面に復帰した時に呼ばれます。
     * @details 一時停止からの復帰や、必要な状態の再設定などに利用します。
     */
    virtual void OnResume() {}

    // --- デバッグ機能 ---
    // エンジン共通のデバッグウィンドウにタブを追加する
    virtual void DrawDebugTab() {}


    // --- スタック管理機能 ---
    // このシーンが下のシーンの更新(Update)をブロックするか（デフォルトはブロックする）
    virtual bool IsUpdateBlocking() const { return true; }
    
    // このシーンが下のシーンの描画(Draw)をブロックするか（デフォルトはブロックしない）
    virtual bool IsDrawBlocking() const { return false; }
    
    // このシーンでマウスカーソルを表示するか（デフォルトは表示する）
    virtual bool IsCursorVisible() const { return true; }
};