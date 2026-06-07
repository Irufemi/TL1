#pragma once

// --- Core (数学・便利ツール) ---

#include "Core/Math/Vector2.h"
#include "Core/Math/Vector3.h"
#include "Core/Math/Vector4.h"
#include "Core/Math/Transform.h"
#include "Core/Math/Matrix3x3.h"
#include "Core/Math/Matrix4x4.h"
#include "Core/Math/Quaternion.h"
#include "Core/Math/QuaternionTransform.h"
#include "Core/Math/NumericalSequence.h"


#include "Core/Math/Geometry/AABB.h"
#include "Core/Math/Geometry/OBB.h"
#include "Core/Math/Math.h"
#include "Core/Math/Geometry/Collision.h"
#include "Core/Math/Random/Random.h"
#include "Core/Utility/Ease.h"

#include "Core/Shape/Ball.h"
#include "Core/Shape/LinePrimitive.h"
#include "Core/Shape/Plane.h"
#include "Core/Shape/Sphere.h"
#include "Core/Shape/Triangle.h"

#include "Core/Type/BlendMode.h"

// --- Platform (OS・入力) ---
#include "Platform/Input/InputManager.h"
#include "Platform/Input/GamePad.h"
#include "Platform/Input/Keyboard.h"
#include "Platform/Input/Mouse.h"

// --- Graphics / Renderer (描画オブジェクト) ---
// LineInstanced
#include "Renderer/LineInstanced/LineClass.h"
// Object2D
#include "Renderer/Object2D/Sprite/Sprite.h"
#include "Renderer/Object2D/Primitive/Circle2D.h"
// Object3D
#include "Renderer/Object3D/AnimationModel/AnimationModel.h"
#include "Renderer/Object3D/StaticModelObject/StaticModelObject.h"
#include "Renderer/Object3D/Primitive/Primitive3DObject.h"

// ParticleGPU
#include "Renderer/ParticleGPU/GPUParticleSystem.h"
// VoxelParticle
#include "Renderer/VoxelParticle/VoxelParticleSystem.h"
// Region
#include "Renderer/Region/ModelRegion.h"
#include "Renderer/Region/PrimitiveRegion.h"
// Skybox
#include "Renderer/Skybox/Skybox.h"
// Effect
#include "Renderer/Effect/Effect.h"

// 音関連
#include "Resource/Audio/Bgm.h"
#include "Resource/Audio/Se.h"

// --- Resource (素材管理) ---
#include "Resource/Texture/TextureManager.h"
#include "Resource/Model/ModelManager.h"
#include "Resource/Audio/AudioManager.h"

// --- Framework (シーン) ---
//#include "Framework/SceneManager.h"
//#include "Framework/IScene.h"

// --- Debug (デバッグ) ---
#include "Manager/DebugUI.h"

// 描画
#include "Manager/DrawManager.h"

// エンジン
#include "IrufemiEngine.h"
