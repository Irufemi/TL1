#pragma once
#include <vector>
#include <cstdint>

/**
 * @namespace Math::Sequence
 * @brief 数列に関する計算を行うユーティリティ
 */
namespace Math::Sequence {

    /**
     * @brief 等差数列の第n項（0-indexed）を計算する
     * @details a[n] = a[0] + n * d
     * @param firstTerm 初項 (a[0])
     * @param difference 公差 (d)
     * @param index 項の番号 (0から始まる)
     * @return 第index項の値
     */
    float CalculateArithmetic(float firstTerm, float difference, uint32_t index);

    /**
     * @brief 等比数列の第n項（0-indexed）を計算する
     * @details a[n] = a[0] * r^n
     * @param firstTerm 初項 (a[0])
     * @param ratio 公比 (r)
     * @param index 項の番号 (0から始まる)
     * @return 第index項の値
     */
    float CalculateGeometric(float firstTerm, float ratio, uint32_t index);

    /**
     * @brief 等差数列を指定した個数生成する
     * @param firstTerm 初項
     * @param difference 公差
     * @param count 生成する項の数
     * @return 生成された数列のvector
     */
    std::vector<float> GenerateArithmetic(float firstTerm, float difference, uint32_t count);

    /**
     * @brief 等比数列を指定した個数生成する
     * @param firstTerm 初項
     * @param ratio 公比
     * @param count 生成する項の数
     * @return 生成された数列のvector
     */
    std::vector<float> GenerateGeometric(float firstTerm, float ratio, uint32_t count);
}
