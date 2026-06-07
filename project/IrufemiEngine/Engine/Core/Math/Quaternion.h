#pragma once

/**
 * @struct Quaternion
 * @brief クォータニオン (四元数)
 */
struct Quaternion final {
    float x;
    float y;
    float z;
    float w;

    /**
     * @brief 添え字演算子
     * @param index 成分のインデックス (0:x, 1:y, 2:z, 3:w)
     * @return 成分への参照
     */
    float& operator[](int index);

    /**
     * @brief 添え字演算子 (const)
     * @param index 成分のインデックス (0:x, 1:y, 2:z, 3:w)
     * @return 成分の値
     */
    float operator[](int index) const;

    /** @name 複合代入演算子 */
    /** @{ */
    Quaternion& operator+=(Quaternion rhs);
    Quaternion& operator-=(Quaternion rhs);
    Quaternion& operator*=(float s);
    Quaternion& operator/=(float s);
    /** @} */
};

/** @name 非メンバ演算子 */
/** @{ */

/**
 * @brief クォータニオン同士の加算
 */
Quaternion operator+(Quaternion lhs, Quaternion rhs);

/**
 * @brief クォータニオン同士の減算
 */
Quaternion operator-(Quaternion lhs, Quaternion rhs);

/**
 * @brief 単項演算子 +
 */
Quaternion operator+(Quaternion q);

/**
 * @brief 単項演算子 - (符号反転)
 */
Quaternion operator-(Quaternion q);

/**
 * @brief スカラー乗算
 */
Quaternion operator*(Quaternion q, float s);

/**
 * @brief スカラー乗算 (可換)
 */
Quaternion operator*(float s, Quaternion q);

/**
 * @brief スカラー除算
 */
Quaternion operator/(Quaternion q, float s);

/**
 * @brief クォータニオン同士の積 (ハミルトン積)
 */
Quaternion operator*(const Quaternion& lhs, const Quaternion& rhs);

/** @} */
