#pragma once

/**
 * @class GameApplication
 * @brief ゲームアプリケーション全体のエントリーポイントと実行を管理するクラス
 * @details エンジンの初期化、シーンの登録、ゲームループの開始など、
 *          アプリケーションのライフサイクル全体を統括します。
 */
class GameApplication {
public:
    /**
     * @brief コンストラクタ
     */
    GameApplication();
    /**
     * @brief デストラクタ
     */
    ~GameApplication();

    /**
     * @brief ゲームアプリケーションを実行します
     * @details エンジンの初期化からゲームループの終了まで、全ての処理を管理します。
     */
    void Run();

private:
    // コピー禁止
    GameApplication(const GameApplication&) = delete;
    GameApplication& operator=(const GameApplication&) = delete;
};