#pragma once

#include <cassert>

/**
 * @class Singleton
 * @brief ゲーム開発向けの安全な手動ライフサイクル管理型シングルトンベースクラス (CRTP)
 * @details 派生クラスは friend class Singleton<T>; を指定し、コンストラクタ/デストラクタを private にする必要があります。
 *          初期化順序のバグを防ぐため、必ずメインスレッドの適切なタイミングで Initialize() を呼び出してください。
 */
template <typename T>
class Singleton {
protected:
    Singleton() = default;
    virtual ~Singleton() = default;

public:
    // コピーとムーブを禁止
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;
    Singleton(Singleton&&) = delete;
    Singleton& operator=(Singleton&&) = delete;

    /**
     * @brief インスタンスを生成する
     * @details メインループ開始前（DirectX初期化後など）に手動で呼び出します。
     */
    static void Initialize() {
        assert(!instance_ && "Singleton is already initialized.");
        instance_ = new T();
    }

    /**
     * @brief インスタンスを破棄する
     * @details メインループ終了後（DirectX破棄前など）に手動で呼び出し、確実にリソースを解放します。
     */
    static void Finalize() {
        if (instance_) {
            delete instance_;
            instance_ = nullptr;
        }
    }

    /**
     * @brief インスタンスを取得する
     * @return シングルトンインスタンスのポインタ
     */
    static T* GetInstance() {
        assert(instance_ && "Singleton is not initialized. Call Initialize() first.");
        return instance_;
    }

private:
    static T* instance_;
};

template <typename T>
T* Singleton<T>::instance_ = nullptr;
