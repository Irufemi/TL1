#pragma once

/**
 * @struct Matrix4x4
 * @brief 4x4 行列
 */
struct Matrix4x4 final {
    float m[4][4];

    /** @name 複合代入演算子 */
    /** @{ */
    Matrix4x4& operator+=(const Matrix4x4& rhs);
    Matrix4x4& operator-=(const Matrix4x4& rhs);
    Matrix4x4& operator*=(const Matrix4x4& rhs);
    /** @} */
};

/** @name 非メンバ演算子 */
/** @{ */

/**
 * @brief 行列の加算
 */
Matrix4x4 operator+(const Matrix4x4& lhs, const Matrix4x4& rhs);

/**
 * @brief 行列の減算
 */
Matrix4x4 operator-(const Matrix4x4& lhs, const Matrix4x4& rhs);

/**
 * @brief 行列の積 (lhs * rhs)
 */
Matrix4x4 operator*(const Matrix4x4& lhs, const Matrix4x4& rhs);

/**
 * @brief 単項演算子 +
 */
Matrix4x4 operator+(const Matrix4x4& m);

/**
 * @brief 単項演算子 - (符号反転)
 */
Matrix4x4 operator-(const Matrix4x4& m);

/** @} */


