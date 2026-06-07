#pragma once
#include <Windows.h>
#include <array>
#include <cstdint>

/**
 * @class Keyboard
 * @brief キーボード入力を管理するクラス
 * @details 各キーの押下状態（Down, Up, Pressed, Released）を取得します。
 *          DIK（DirectInput Key）から VK（Virtual Key）への変換機能も内蔵しています。
 */
class Keyboard {
public:
    static const int KEY_COUNT = 256;

    Keyboard() = default;
    ~Keyboard() = default;

    /** @name 初期化・更新 */
    ///@{
    void Initialize();
    void Update();
    void Clear();
    ///@}

    /** @name キー状態の取得 */
    ///@{
    bool IsKeyDown(uint8_t key) const;
    bool IsKeyUp(uint8_t key) const;
    /** @brief キーが押された瞬間か判定（立ち上がり） */
    bool IsKeyPressed(uint8_t key) const;
    /** @brief キーが離された瞬間か判定（立ち下がり） */
    bool IsKeyReleased(uint8_t key) const;
    ///@}

    /** @name DIK互換API */
    ///@{
    bool IsKeyDownDIK(uint8_t dik) const;
    bool IsKeyUpDIK(uint8_t dik) const;
    bool IsKeyPressedDIK(uint8_t dik) const;
    bool IsKeyReleasedDIK(uint8_t dik) const;
    ///@}

private:
    std::array<BYTE, KEY_COUNT> currentKeys_{};
    std::array<BYTE, KEY_COUNT> previousKeys_{};

    /**
     * @brief DIKからVKへの変換
     */
    static uint8_t DIKToVK(uint8_t dik);
};
