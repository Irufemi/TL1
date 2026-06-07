#pragma once

/**
 * @struct Vector3
 * @brief 3次元ベクトル
 */
struct Vector3 final {
	float x;
	float y;
	float z;

	/**
	 * @brief 添え字演算子
	 * @param index 成分のインデックス (0:x, 1:y, 2:z)
	 * @return 成分への参照
	 */
	float& operator[](int index);

	/**
	 * @brief 添え字演算子 (const)
	 * @param index 成分のインデックス (0:x, 1:y, 2:z)
	 * @return 成分の値
	 */
	float operator[](int index) const;

	/** @name 複合代入演算子 */
	/** @{ */
	Vector3& operator+=(Vector3 rhs);
	Vector3& operator-=(Vector3 rhs);
	Vector3& operator*=(float s);
	Vector3& operator/=(float s);
	/** @} */
};

/** @name 非メンバ演算子 */
/** @{ */

/**
 * @brief ベクトル同士の加算
 */
Vector3 operator+(Vector3 lhs, Vector3 rhs);

/**
 * @brief ベクトル同士の減算
 */
Vector3 operator-(Vector3 lhs, Vector3 rhs);

/**
 * @brief 単項演算子 +
 */
Vector3 operator+(Vector3 v);

/**
 * @brief 単項演算子 - (符号反転)
 */
Vector3 operator-(Vector3 v);

/**
 * @brief スカラー乗算
 */
Vector3 operator*(Vector3 v, float s);

/**
 * @brief スカラー乗算 (可換)
 */
Vector3 operator*(float s, Vector3 v);

/**
 * @brief スカラー除算
 */
Vector3 operator/(Vector3 v, float s);

/** @} */