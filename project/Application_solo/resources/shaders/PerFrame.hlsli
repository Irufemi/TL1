#pragma once

/**
 * @struct PerFrame
 * @brief フレーム単位の時間データ (Legacy/Compute用)
 * @details 各種更新用Compute Shaderで使用される。
 */
struct PerFrame
{
	float32_t time;
	float32_t deltaTime;
};

/**
 * @struct PerFrameData
 * @brief フレーム全体で共有されるデータ構造体 (Camera, Timeなど)
 * @details C++側の PerFrameData (SceneGPUStructs.h) と一致させる。
 *          DrawManager によって Graphics RootSlot::Camera (register b2) にバインドされる。
 */
struct PerFrameData
{
    // --- CameraForGPU camera ---
    float32_t4x4 view;
    float32_t4x4 projection;
    float32_t3 cameraWorldPosition;
    
    // --- Time / DeltaTime ---
    float32_t time;
    float32_t deltaTime;
    
    // --- Padding ---
    float32_t2 padding;
};