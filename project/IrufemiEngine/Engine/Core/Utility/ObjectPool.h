#pragma once
#include <vector>
#include <memory>
#include <functional>

/**
 * @class ObjectPool
 * @brief ゲームループ中の動的メモリ確保(new/delete)を回避するための汎用オブジェクトプール
 * @tparam T プールで管理するオブジェクトの型
 */
template <typename T>
class ObjectPool {
public:
    /**
     * @brief コンストラクタ。指定された数のオブジェクトを事前に生成しプールに格納する。
     * @param initialSize プールに事前確保するオブジェクトの数
     * @param factory オブジェクトを生成するためのファクトリ関数（オプション。Prefab生成などに使用）
     */
    explicit ObjectPool(size_t initialSize, std::function<std::shared_ptr<T>()> factory = nullptr) {
        pool_.reserve(initialSize);
        for (size_t i = 0; i < initialSize; ++i) {
            if (factory) {
                pool_.push_back(factory());
            } else {
                pool_.push_back(std::make_shared<T>());
            }
        }
    }

    ~ObjectPool() {
        pool_.clear();
    }

    /**
     * @brief プールから利用可能なオブジェクトを取得する。
     * @return 取得したオブジェクトのshared_ptr。プールが枯渇している場合は nullptr を返す。
     */
    std::shared_ptr<T> Acquire() {
        if (pool_.empty()) {
            // メモリの厳格化要件に従い、実行中の自動拡張(動的確保)は行わず nullptr を返す。
            return nullptr;
        }
        std::shared_ptr<T> obj = pool_.back();
        pool_.pop_back();
        return obj;
    }

    /**
     * @brief 使用済みのオブジェクトをプールに返却する。
     * @param obj 返却するオブジェクトのshared_ptr
     */
    void Release(std::shared_ptr<T> obj) {
        if (obj) {
            pool_.push_back(obj);
        }
    }

    /**
     * @brief 現在プールで待機している（未使用の）オブジェクトの数を取得する。
     * @return 待機中のオブジェクト数
     */
    size_t GetAvailableCount() const {
        return pool_.size();
    }

private:
    std::vector<std::shared_ptr<T>> pool_; ///< 未使用オブジェクトを格納するコンテナ
};
