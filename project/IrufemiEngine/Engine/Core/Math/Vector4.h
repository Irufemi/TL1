#pragma once

/**
 * @struct Vector4
 * @brief 4次元ベクトル
 */
struct Vector4 final {
    float x;
    float y;
    float z;
    float w;

    /** @name 単項演算子 */
    /** @{ */
    Vector4 operator+() const;
    Vector4 operator-() const;
    /** @} */

    /** @name 複合代入演算子 */
    /** @{ */
    Vector4& operator+=(Vector4 v);
    Vector4& operator-=(Vector4 v);
    Vector4& operator*=(float s);
    Vector4& operator/=(float s);
    /** @} */

    /** @name 比較演算子 */
    /** @{ */
    bool operator==(const Vector4& v) const { return x == v.x && y == v.y && z == v.z && w == v.w; }
    bool operator!=(const Vector4& v) const { return !(*this == v); }
    /** @} */
};

/** @name 非メンバ演算子 */
/** @{ */

/**
 * @brief ベクトル同士の加算
 */
Vector4 operator+(Vector4 v1, Vector4 v2);

/**
 * @brief ベクトル同士の減算
 */
Vector4 operator-(Vector4 v1, Vector4 v2);

/**
 * @brief スカラー乗算
 */
Vector4 operator*(Vector4 v, float s);

/**
 * @brief スカラー乗算 (可換)
 */
Vector4 operator*(float s, Vector4 v);

/**
 * @brief スカラー除算
 */
Vector4 operator/(Vector4 v, float s);

/** @} */

