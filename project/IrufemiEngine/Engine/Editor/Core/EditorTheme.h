#pragma once

#ifdef EditorMode

/**
 * @class EditorTheme
 * @brief エディター共通のカラーパレットやボタンスタイルを提供するユーティリティ
 */
class EditorTheme {
public:
    /**
     * @brief 危険な操作（削除など）のボタンスタイルを適用する
     */
    static void PushDangerButtonStyle();

    /**
     * @brief 適用したボタンスタイルを解除する
     */
    static void PopButtonStyle();
};

#endif // EditorMode
