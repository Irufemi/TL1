#pragma once

/**
 * @struct Vector2
 * @brief 2次元ベクトル
 */
struct Vector2 final {
	float x;
	float y;

	/**
	 * @brief 添え字演算子
	 * @param index 成分のインデックス (0:x, 1:y)
	 * @return 成分への参照
	 */
	float& operator[](int index);

	/**
	 * @brief 添え字演算子 (const)
	 * @param index 成分のインデックス (0:x, 1:y)
	 * @return 成分の値
	 */
	float operator[](int index) const;

	/** @name 複合代入演算子 */
	/** @{ */
	Vector2& operator+=(Vector2 rhs);
	Vector2& operator-=(Vector2 rhs);
	Vector2& operator*=(float s);
	Vector2& operator/=(float s);
	/** @} */
};

/** @name 非メンバ演算子 */
/** @{ */

/**
 * @brief ベクトル同士の加算
 */
Vector2 operator+(Vector2 lhs, Vector2 rhs);

/**
 * @brief ベクトル同士の減算
 */
Vector2 operator-(Vector2 lhs, Vector2 rhs);

/**
 * @brief 単項演算子 +
 */
Vector2 operator+(Vector2 v);

/**
 * @brief 単項演算子 - (符号反転)
 */
Vector2 operator-(Vector2 v);

/**
 * @brief スカラー乗算
 */
Vector2 operator*(Vector2 v, float s);

/**
 * @brief スカラー乗算 (可換)
 */
Vector2 operator*(float s, Vector2 v);

/**
 * @brief スカラー除算
 */
Vector2 operator/(Vector2 v, float s);

/** @} */