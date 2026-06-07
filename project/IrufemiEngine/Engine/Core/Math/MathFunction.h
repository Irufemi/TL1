#pragma once

#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"
#include "Matrix4x4.h"
#include "Quaternion.h"

//前方宣言
struct Segment2D;
struct Segment;
struct Ray;
struct Line;
struct Sphere;
struct Plane;
struct Triangle;
struct AABB;
struct OBB;

/**
 * @namespace Math
 * @brief 数学・幾何学関数を提供する名前空間
 */
namespace Math {

    /** @name 定数 */
    /** @{ */
    constexpr float PI = 3.1415926535f;
    constexpr float PIDiv2 = PI / 2.0f;
    constexpr float PIDiv4 = PI / 4.0f;
    /** @} */


#pragma region 2次元ベクトル関数
    /** @name 2次元ベクトル関数 */
    /** @{ */

    /**
     * @brief 加算 (推奨: a + b)
     */
    Vector2 Add(Vector2 a, Vector2 b);

    /**
     * @brief 減算 (推奨: a - b)
     */
    Vector2 Subtract(Vector2 a, Vector2 b);

    /**
     * @brief スカラー倍 (推奨: scalar * vector)
     */
    Vector2 Multiply(float scalar, Vector2 vector);

    /**
     * @brief 内積
     */
    float Dot(Vector2 a, Vector2 b);

    /**
     * @brief ノルム (長さ)
     */
    float Length(Vector2 vector);

    /**
     * @brief 正規化
     */
    Vector2 Normalize(Vector2 vector);

    /**
     * @brief 点と線分の最近接点
     */
    Vector2 ClosestPoint(Vector2 point, const Segment2D& segment);

    /**
     * @brief 2次ベジェ曲線
     */
    Vector2 Bezier(Vector2 p0, Vector2 p1, Vector2 p2, float t);

    /**
     * @brief Catmull-Rom スプライン
     */
    Vector2 CatmullRom(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, float t);

    /** @} */
#pragma endregion

#pragma region 3次元ベクトル関数
    /** @name 3次元ベクトル関数 */
    /** @{ */

    /**
     * @brief 加算 (推奨: a + b)
     */
    Vector3 Add(Vector3 a, Vector3 b);

    /**
     * @brief 減算 (推奨: a - b)
     */
    Vector3 Subtract(Vector3 a, Vector3 b);

    /**
     * @brief スカラー倍 (推奨: scalar * vector)
     */
    Vector3 Multiply(float scalar, Vector3 vector);

    /**
     * @brief 内積
     */
    float Dot(Vector3 a, Vector3 b);

    /**
     * @brief ノルム (長さ)
     */
    float Length(Vector3 vector);

    /**
     * @brief 正規化
     */
    Vector3 Normalize(Vector3 vector);

    /**
     * @brief クロス積 (外積)
     */
    Vector3 Cross(Vector3 a, Vector3 b);

    /**
     * @brief 正射影ベクトル
     */
    Vector3 Project(Vector3 v1, Vector3 v2);

    /**
     * @brief 点と線分の最近接点
     */
    Vector3 ClosestPoint(Vector3 point, const Segment& segment);

    /**
     * @brief 点と直線の最近接点
     */
    Vector3 ClosestPoint(Vector3 point, const Ray& ray);

    /**
     * @brief 点と半直線の最近接点
     */
    Vector3 ClosestPoint(Vector3 point, const Line& line);

    /**
     * @brief 2次ベジェ曲線
     */
    Vector3 Bezier(Vector3 p0, Vector3 p1, Vector3 p2, float t);

    /**
     * @brief Catmull-Rom スプライン
     */
    Vector3 CatmullRom(Vector3 p0, Vector3 p1, Vector3 p2, Vector3 p3, float t);

    /** @} */
#pragma endregion

#pragma region 4次元ベクトル関数
    /** @name 4次元ベクトル関数 */
    /** @{ */

    /**
     * @brief 加算 (推奨: a + b)
     */
    Vector4 Add(Vector4 a, Vector4 b);
    
    /**
     * @brief 減算 (推奨: a - b)
     */
    Vector4 Subtract(Vector4 a, Vector4 b);
    
    /**
     * @brief スカラー倍 (推奨: scalar * vector)
     */
    Vector4 Multiply(float s, Vector4 v);
    
    /**
     * @brief 内積
     */
    float Dot(Vector4 a, Vector4 b);
    
    /**
     * @brief ノルム (長さ)
     */
    float Length(Vector4 v);
    
    /**
     * @brief 正規化
     */
    Vector4 Normalize(Vector4 v);
    
    /** @} */
#pragma endregion

#pragma region 4x4行列関数
    /** @name 4x4行列関数 */
    /** @{ */

    /**
     * @brief 行列の加算
     */
    Matrix4x4 Add(const Matrix4x4& m1, const Matrix4x4& m2);

    /**
     * @brief 行列の減算
     */
    Matrix4x4 Subtract(const Matrix4x4& m1, const Matrix4x4& m2);

    /**
     * @brief 行列の積 (推奨: m1 * m2)
     */
    Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2);

    /**
     * @brief 逆行列の計算
     */
    Matrix4x4 Inverse(const Matrix4x4& m);

    /**
     * @brief 転置行列の計算
     */
    Matrix4x4 Transpose(const Matrix4x4& m);

    /**
     * @brief 単位行列の作成
     */
    Matrix4x4 MakeIdentity4x4();

    /**
     * @brief 平行移動行列の作成
     */
    Matrix4x4 MakeTranslateMatrix(Vector3 translate);

    /**
     * @brief 拡大縮小行列の作成
     */
    Matrix4x4 MakeScaleMatrix(Vector3 scale);

    /**
     * @brief 座標変換 (w=1として計算後、w除算)
     */
    Vector3 Transform(Vector3 vector, const Matrix4x4& matrix);

    /**
     * @brief ベクトル変換 (平行移動を無視)
     */
    Vector3 TransformNormal(Vector3 vector, const Matrix4x4& matrix);

    /**
     * @brief X軸周り回転行列の作成
     */
    Matrix4x4 MakeRotateXMatrix(float theta);

    /**
     * @brief Y軸周り回転行列の作成
     */
    Matrix4x4 MakeRotateYMatrix(float theta);

    /**
     * @brief Z軸周り回転行列の作成
     */
    Matrix4x4 MakeRotateZMatrix(float theta);

    /**
     * @brief 3軸合成回転行列の作成 (XYZ順)
     */
    Matrix4x4 MakeRotateXYZMatrix(Vector3 rotate);

    /**
     * @brief 3軸合成回転行列の作成 (XYZ順) - 各成分指定版
     */
    Matrix4x4 MakeRotateXYZMatrix(float x, float y, float z);

    /**
     * @brief アフィン変換行列の作成 (Euler回転版)
     */
    Matrix4x4 MakeAffineMatrix(Vector3 scale, Vector3 rotate, Vector3 translate);

    /**
     * @brief アフィン変換行列の作成 (Euler回転版) - 各成分指定版
     */
    Matrix4x4 MakeAffineMatrix(Vector3 scale, float rotateX, float rotateY, float rotateZ, Vector3 translate);

    /**
     * @brief アフィン変換行列の作成 (Quaternion回転版)
     */
    Matrix4x4 MakeAffineMatrix(Vector3 scale, const Quaternion& rotateQuaternion, Vector3 translate);

    /**
     * @brief 透視投影行列の作成
     */
    Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip);

    /**
     * @brief 正射影行列の作成
     */
    Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip);

    /**
     * @brief ビューポート変換行列の作成
     */
    Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth);

    /**
     * @brief 画面上の2D座標から3D空間のレイ（半直線）を生成する
     * @param mousePos 画面上のマウス座標
     * @param screenWidth 画面の幅
     * @param screenHeight 画面の高さ
     * @param viewProjectionInverse カメラのビュー・プロジェクション逆行列
     */
    Ray ScreenPointToRay(Vector2 mousePos, float screenWidth, float screenHeight, const Matrix4x4& viewProjectionInverse);

    /**
     * @brief 任意軸回転行列の作成
     */
    Matrix4x4 MakeRotateAxisAngle(Vector3 axis, float angle);

    /**
     * @brief 方向ベクトル間の回転行列を作成
     */
    Matrix4x4 DirectionToDirection(Vector3 from, Vector3 to);

    /**
     * @brief 回転行列からオイラー角を抽出
     */
    Vector3 ExtractEulerFromMatrix(const Matrix4x4& matrix);

    /** @} */
#pragma endregion

#pragma region Quaternion
    /** @name Quaternion 関数 */
    /** @{ */

    /**
     * @brief クォータニオンの積
     */
    Quaternion Multiply(const Quaternion& lhs, const Quaternion& rhs);

    /**
     * @brief 単位クォータニオン
     */
    Quaternion IdentityQuaternion();

    /**
     * @brief 共役クォータニオン
     */
    Quaternion Conjugate(const Quaternion& quaternion);

    /**
     * @brief クォータニオンのノルム
     */
    float Norm(const Quaternion& quaternion);

    /**
     * @brief クォータニオンの正規化
     */
    Quaternion Normalize(const Quaternion& quaternion);

    /**
     * @brief 逆クォータニオン
     */
    Quaternion Inverse(const Quaternion& quaternion);

    /**
     * @brief 任意軸回転クォータニオンの作成
     */
    Quaternion MakeRotateAxisAngleQuaternion(Vector3 axis, float angle);

    /**
     * @brief クォータニオンによるベクトル回転
     */
    Vector3 RotateVector(Vector3 vector, const Quaternion& quaternion);

    /**
     * @brief クォータニオンから回転行列を作成
     */
    Matrix4x4 MakeRotateMatrix(const Quaternion& quaternion);

    /**
     * @brief 球面線形補間 (SLERP)
     */
    Quaternion Slerp(const Quaternion& q0, const Quaternion& q1, float t);

    /**
     * @brief クォータニオンからオイラー角へ変換
     */
    Vector3 ToEuler(const Quaternion& q);

    /** @} */
#pragma endregion

    /**
     * @brief 垂直なベクトルを求める
     */
    Vector3 Perpendicular(Vector3 vector);

    /**
     * @brief 値を最小値と最大値の間にクランプする
     */
    template <typename T>
    constexpr const T& Clamp(const T& v, const T& lo, const T& hi) {
        return (v < lo) ? lo : (hi < v) ? hi : v;
    }

    /**
     * @brief 角度を -PI から PI の範囲に正規化する
     * @param angle 正規化する角度(ラジアン)
     * @return -PI ~ PI に収まった角度
     */
    float NormalizeAngle(float angle);

    /**
     * @brief 度数法(Degree)から弧度法(Radian)への変換
     * @param degrees 変換する角度(度)
     * @return 弧度法(Radian)での角度
     */
    float ToRadians(float degrees);

    /**
     * @brief 弧度法(Radian)から度数法(Degree)への変換
     * @param radians 変換する角度(ラジアン)
     * @return 度数法(Degree)での角度
     */
    float ToDegrees(float radians);

}

