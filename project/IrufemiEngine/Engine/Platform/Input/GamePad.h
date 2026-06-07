#pragma once
#include <Windows.h>
#include <Xinput.h>
#include <utility>
#pragma comment(lib, "Xinput.lib")

enum class Stick8 {
    Neutral, Up, UpRight, Right, DownRight, Down, DownLeft, Left, UpLeft
};

/**
 * @class GamePad
 * @brief ゲームパッド入力を管理するクラス
 * @details XInput を使用してゲームパッドの状態を取得します。
 *          ボタン入力、アナログスティック、デッドゾーン処理、8方向変換などの機能を提供します。
 */
class GamePad {
public:
    GamePad() = default;
    ~GamePad() = default;

    /** @name 初期化・更新 */
    ///@{
    void Initialize();
    void Update();
    void Clear();
    ///@}

    /** @name ボタン入力状態 */
    ///@{
    bool IsButtonDown(WORD button) const;
    bool IsButtonUp(WORD button) const;
    /** @brief ボタンが押された瞬間か判定（立ち上がり） */
    bool IsButtonPressed(WORD button) const;
    /** @brief ボタンが離された瞬間か判定（立ち下がり） */
    bool IsButtonReleased(WORD button) const;
    ///@}

    /** @name アナログスティック・トリガー */
    ///@{
    float GetLeftStickX()  const;
    float GetLeftStickY()  const;
    float GetRightStickX() const;
    float GetRightStickY() const;
    float GetLeftTrigger() const;
    float GetRightTrigger() const;

    /** @brief スティックのデッドゾーンを設定 */
    void SetLeftDeadZone(float dz) { deadZoneLeft_ = dz; }
    void SetRightDeadZone(float dz) { deadZoneRight_ = dz; }
    ///@}

    /** @name 接続状態・インデックス */
    ///@{
    /** @brief コントローラーが接続されているか取得 */
    bool IsConnected() const { return connected_; }
    /** @brief 使用するコントローラーのインデックス (0~3) を設定 */
    void SetIndex(int idx) { index_ = idx; }
    ///@}

    /** @name 状態取得 */
    ///@{
    /** @brief XINPUT_STATE を直接取得 */
    const XINPUT_STATE& GetState() const { return state_; }
    ///@}

    /** @name スティック8方向入力 */
    ///@{
    /** @brief 左スティックの状態を8方向に変換して取得 */
    Stick8 Left8(float threshold = 0.30f) const;
    bool Left8Is(Stick8 dir, float threshold = 0.30f) const;
    bool Left8Pressed(Stick8 dir, float threshold = 0.30f) const;
    bool Left8Released(Stick8 dir, float threshold = 0.30f) const;

    // 方向ショートカット
    bool LUp(float th = 0.30f) const;
    bool LDown(float th = 0.30f) const;
    bool LLeft(float th = 0.30f) const;
    bool LRight(float th = 0.30f) const;
    bool LUpRight(float th = 0.30f) const;
    bool LUpLeft(float th = 0.30f) const;
    bool LDownRight(float th = 0.30f) const;
    bool LDownLeft(float th = 0.30f) const;
    ///@}

    /** @name D-Pad 8方向入力 */
    ///@{
    Stick8 DPad8Now() const;
    bool DPad8Is(Stick8 dir) const;
    bool DPad8Pressed(Stick8 dir) const;
    bool DPad8Released(Stick8 dir) const;

    // D-Pad ショートカット
    bool DPadUp() const;    bool DPadUpPressed() const;    bool DPadUpReleased() const;
    bool DPadDown() const;  bool DPadDownPressed() const;  bool DPadDownReleased() const;
    bool DPadLeft() const;  bool DPadLeftPressed() const;  bool DPadLeftReleased() const;
    bool DPadRight() const; bool DPadRightPressed() const; bool DPadRightReleased() const;

    // 斜め方向ショートカット
    bool DPadUpRightDown() const;   bool DPadUpRightPressed() const;   bool DPadUpRightReleased() const;
    bool DPadUpLeftDown() const;    bool DPadUpLeftPressed() const;    bool DPadUpLeftReleased() const;
    bool DPadDownRightDown() const; bool DPadDownRightPressed() const; bool DPadDownRightReleased() const;
    bool DPadDownLeftDown() const;  bool DPadDownLeftPressed() const;  bool DPadDownLeftReleased() const;
    ///@}

    /** @name トリガー（しきい値デジタル判定） */
    ///@{
    float LeftTriggerAnalog(float deadzone = 30.0f)  const;
    float RightTriggerAnalog(float deadzone = 30.0f) const;
    bool LeftTriggerDown(uint8_t threshold = 30) const;
    bool LeftTriggerPressed(uint8_t threshold = 30) const;
    bool LeftTriggerReleased(uint8_t threshold = 30) const;
    bool RightTriggerDown(uint8_t threshold = 30) const;
    bool RightTriggerPressed(uint8_t threshold = 30) const;
    bool RightTriggerReleased(uint8_t threshold = 30) const;

    /** @brief 左右いずれかのトリガーの状態を判定
     *  @param[in] right true で右トリガー、false で左トリガーを判定
     */
    bool TriggerDown(bool right, uint8_t threshold = 30) const;
    bool TriggerPressed(bool right, uint8_t threshold = 30) const;
    bool TriggerReleased(bool right, uint8_t threshold = 30) const;
    ///@}

    /** @name 特殊ボタンショートカット */
    ///@{
    bool LBDown() const;     bool LBPressed() const;     bool LBReleased() const;
    bool RBDown() const;     bool RBPressed() const;     bool RBReleased() const;
    bool StartDown() const;  bool StartPressed() const;  bool StartReleased() const;
    ///@}

    /** @brief Y軸の判定を反転させるか設定 */
    void SetInvertY(bool inv) { invertY_ = inv; }

private:
    /** @brief ラジアル正規化（円形デッドゾーン処理） */
    static std::pair<float, float> RadialNormalize(short x, short y, int dz);

    /** @brief スティック入力の8方向変換ヘルパー */
    static Stick8 Stick8From(short x, short y, int dz, bool invertY, float threshold);

    XINPUT_STATE state_{};
    XINPUT_STATE prev_{};
    float deadZoneLeft_ = 0.2f;
    float deadZoneRight_ = 0.2f;
    bool connected_ = false;
    int  index_ = 0;
    bool invertY_ = false;
};
