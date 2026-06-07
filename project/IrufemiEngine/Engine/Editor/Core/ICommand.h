#pragma once

#ifdef EditorMode

/**
 * @class ICommand
 * @brief エディタにおける一つの操作（コマンド）を表すインターフェース
 */
class ICommand {
public:
    virtual ~ICommand() = default;

    /**
     * @brief 操作を実行、または「やり直し」する
     */
    virtual void Do() = 0;

    /**
     * @brief 操作を「元に戻す」
     */
    virtual void Undo() = 0;
};

#endif // EditorMode
