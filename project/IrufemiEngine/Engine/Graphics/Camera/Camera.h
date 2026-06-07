#pragma once

#include "Engine/Core/Math/Vector2.h"
#include "Engine/Core/Math/Vector3.h"
#include "Engine/Core/Math/Matrix4x4.h"
#include "Engine/Core/Math/Geometry/Frustum.h"

/**
 * @class Camera
 * @brief 3D空間の視点を管理し、ビュー行列や射影行列を生成するクラス
 */
class Camera {
private: // メンバ変数

    //カメラの位置。ワールド座標。
    Vector3 translate_ = { 0.0f,0.0f,-50.0f };

    //カメラの回転角度
    Vector3 rotate_ = { 0.0f,0.0f,0.0f };

    //カメラの拡縮(ここはいじらない。)
    const Vector3 scale_ = { 1.0f,1.0f,1.0f };

#pragma region 正射影行列を構成する変数(カメラで映す空間の範囲)

    // カメラで映す空間の左端のX座標
    float left_ = 0.0f;

    //カメラで映す空間の上端のY座標
    float top_ = 0.0f;

    //カメラで映す空間の右端のX座標
    float right_ = 1280.0f;

    //カメラで映す空間の下端のY座標
    float bottom_ = 720.0f;

    //近平面。ここではz軸が奥行きになるため一番手前
    float nearClip_ = 0.0f;

    //遠平面。ここではz軸が奥行きになるため遠さを表す。
    float farClip_ = 100.0f;

#pragma endregion

#pragma region 透視投影行列を構成する変数(カメラで映す空間の範囲)

    //垂直方向視野角
    float fovAngleY_ = 45.0f * 3.141592654f / 180.0f;

    //ビューポートのアスペクト比
    float aspectRatio_ = 16.0f / 9.0f;

    //深度限界(手前側)(面なので0だと面が点になって映らない。できるだけ全部が映る遠いところがいい。大体目安は0.1程度)。
    float nearZ_ = 0.1f;

    //深度限界(奥側)
    float farZ_ = 1000.0f;

#pragma endregion

#pragma region ビューポート行列を構成する変数(ウィンドウ上で映す範囲)

    //画面上で映す横幅
    float width_ = 1280.0f;

    //画面上で映す高さ
    float height_ = 720.0f;

    //ウィンドウに映す範囲の左上の座標
    Vector2 leftTop_ = { 0.0f,0.0f };

    //mindepth(最小深度値)
    float minDepth_ = 0.0f;

    //maxDepth(最大深度値)
    float maxDepth_ = 1.0f;

#pragma endregion

    //ワールド行列
    Matrix4x4 worldMatrix_{};

    //ビュー行列
    Matrix4x4 viewMatrix_{};

    //正射影行列
    Matrix4x4 orthographicMatrix_{};

    //透視投影行列
    Matrix4x4 perspectiveFovMatrix_{};

    //ビューポート行列
    Matrix4x4 viewportMatrix_{};

public: // メンバ関数
    /**
     * @brief コンストラクタ
     */
    Camera();

    /**
     * @brief デストラクタ
     */
    ~Camera();

    /**
     * @brief 初期化処理
     * @param windowWidth ウィンドウの幅
     * @param windowHeight ウィンドウの高さ
     */
    void Initialize(const int& windowWidth = 1280, const int& windowHeight = 720);

    /**
     * @brief 更新処理
     */
    void Update();


    /**
     * @brief デバッグ用タブを表示します
     * @param label タブのラベル
     */
    void DrawDebugTab(const char* label);
    
    /**
     * @brief デバッグUIの内容表示 (TabItemなし)
     */
    void DrawDebugContents();


    //セッター

    /**
     * @brief カメラの座標を設定します
     * @param translate ワールド座標
     */
    void SetTranslate(const Vector3& translate) { this->translate_ = translate; }

    /**
     * @brief カメラの回転角度を設定します
     * @param rotate オイラー角
     */
    void SetRotate(const Vector3& rotate) { this->rotate_ = rotate; }

    void SetViewMatrix(const Matrix4x4& viewMatrix) { 
        this->viewMatrix_ = viewMatrix; 
        this->frustum_.SetFromViewProjection(this->viewMatrix_ * this->perspectiveFovMatrix_);
    }

    void SetPerspectiveFovMatrix(const Matrix4x4& perspectiveFovMatrix) { 
        this->perspectiveFovMatrix_ = perspectiveFovMatrix; 
        this->frustum_.SetFromViewProjection(this->viewMatrix_ * this->perspectiveFovMatrix_);
    }

    void SetFarClip(const float& farClip) { this->farClip_ = farClip; }

    void SetFovY(const float& fovY) { this->fovAngleY_ = fovY; }


    //ゲッター

    /**
     * @brief カメラの座標を取得します
     * @return const Vector3& ワールド座標
     */
    const Vector3& GetTranslate() const { return this->translate_; }

    /**
     * @brief カメラの回転角度を取得します
     * @return const Vector3& オイラー角
     */
    const Vector3& GetRotate() const { return this->rotate_; }

    /**
     * @brief カメラ行列(ワールド行列)を取得します
     * @return const Matrix4x4& カメラのワールド行列
     */
    const Matrix4x4& GetCameraMatrix();

    /**
     * @brief ワールド行列を取得します
     * @return const Matrix4x4& ワールド行列
     */
    const Matrix4x4& GetWorldMatrix() const { return worldMatrix_; }

    /**
     * @brief ビュー行列を取得します
     * @return const Matrix4x4& ビュー行列
     */
    const Matrix4x4& GetViewMatrix() const { return viewMatrix_; }

    /**
     * @brief 透視投影行列を取得します
     * @return const Matrix4x4& 透視投影行列
     */
    const Matrix4x4& GetPerspectiveFovMatrix() const { return perspectiveFovMatrix_; }

    /**
     * @brief 正射影行列を取得します
     * @return const Matrix4x4& 正射影行列
     */
    const Matrix4x4& GetOrthographicMatrix() const { return orthographicMatrix_; }

    /**
     * @brief ビューポート変換行列を取得します
     * @return const Matrix4x4& ビューポート変換行列
     */
    const Matrix4x4& GetViewportMatrix() const { return viewportMatrix_; }
    const Frustum& GetFrustum() const { return frustum_; }

    Matrix4x4 GetViewProjectionMatrix2D();

    Matrix4x4 GetViewProjectionMatrix3D();


    /**
     * @brief ワールド行列を再計算します
     */
    void MakeWorldMatrix();

    /**
     * @brief ビュー行列を再計算します
     */
    void MakeViewMatrix();

    /**
     * @brief 透視投影行列を再計算します
     */
    void UpdatePerspectiveFovMatrix();

    /**
     * @brief 正射影行列を再計算します
     */
    void UpdateOrthographicMatrix();

    /**
     * @brief ビューポート行列を再計算します
     */
    void UpdateViewportMatrix();

    /**
     * @brief カメラを揺らします
     * @param intensity 揺れの強さ
     * @param durationFrames 揺らすフレーム数
     */
    void Shake(float intensity, int durationFrames);

private:
    float shakeIntensity_ = 0.0f;
    int shakeFrames_ = 0;
    Frustum frustum_;

public:
    /**
     * @brief すべての行列を更新します
     */
    void UpdateMatrix();

    // 2Dで使うための現在のビューポートサイズ取得
    const float& GetViewportWidth() const { return width_; }
    const float& GetViewportHeight() const { return height_; }

};

