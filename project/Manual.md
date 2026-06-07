# IrufemiEngine 取扱説明書 (Manual)

## エディタ画面のレイアウトについて

エディタの画面構成（ドッキングウィンドウの配置など）が崩れてしまった場合や、チーム内で定められた最新の共通レイアウトに更新したい場合は、以下の手順で復元できます。

1. エディター画面上部のメニューバーから **`Window`** をクリック
2. **`Layout` -> `Load Default Layout`** をクリック

現在の自分の使いやすい配置をチームの新しいデフォルト設定にしたい場合は、並び替えたあとに **`Save Current as Default`** を押し、変更された `default_imgui.ini` をGitでコミットしてください。
（※初回クローン時は自動的に共通レイアウトが適用されるようになっています）

---

## パーティクルシステム (GPUParticleSystem) の利用方法

本エンジンのパーティクルシステムは、コンピュートシェーダー(CS)によってGPU上で高速に動作します。
スクリプトやコンポーネントから以下の手順でエミッターを追加・操作することができます。

### コンポーネントからの利用
GameObject に `ParticleEmitterComponent` をアタッチするだけで、自動的にエディタ上で操作・プレビューが可能です。
エディタ（ImGui）上で設定したパラメータは、JSONファイルとして自動的にシリアライズされ、再実行時にも完全に復元されます。

### プログラムからの直接利用 (ParticleObject)
ゲーム内でコードから動的にパーティクルを生成・制御したい場合は `ParticleObject` クラスを使用します。
JSONファイルから設定をロードすることで、エディタで作成した複雑なエフェクトをそのまま呼び出すことができます。

```cpp
#include "Renderer/ParticleGPU/ParticleObject.h"

// 1. ParticleObject の生成とJSONの読み込み
ParticleObject myParticle;
myParticle.LoadFromJson("resources/particles/explosion.json");

// 2. 座標や必要に応じたパラメータの上書き
myParticle.position_ = Vector3(10.0f, 5.0f, 0.0f);
myParticle.emissionRate_ = 100.0f; // 1秒間に100個発生

// （パラメータをコードから変更した場合は MarkDirty() を呼ぶか、Update内で自動反映されます）
myParticle.MarkDirty();

// 3. 毎フレーム Update を呼ぶ
myParticle.Update();

// 4. 一度に大量に発生（バースト）させたい場合
myParticle.EmitBurst(50);
```

### 【上級者向け】GPUParticleManager の直接利用
直接マネージャーに通信して描画リクエストを送ることも可能です（独自の最適化を行いたい場合など）。
※現在のマネージャーは「テクスチャ + ブレンドモード + タイムスケール」の複合キーで管理されています。

```cpp
#include "Renderer/ParticleGPU/GPUParticleManager.h"
#include "Renderer/ParticleGPU/GPUParticleSystem.h"

// 1. マネージャーにエミッターを登録（テクスチャ、ブレンドモード、ポーズ中動作フラグ）
auto handle = GPUParticleManager::GetInstance()->RegisterEmitter(
    "effect/particle_tex.png", 
    BlendMode::kBlendModeAdd, 
    false // trueにするとポーズ中(UnscaledTime)でも動作する
);

// 2. パラメータを設定してマネージャーに更新を通知
GPUParticleEmitter data;
data.emit = 1;
data.type = 0; // 0: Sphere, 1: Beam, 2: Ring, 3: Cylinder, 4: Box
data.translateX = 10.0f;
data.emissionRate = 50.0f; // 1秒あたりの連続放出数

// 3. データの適用
GPUParticleManager::GetInstance()->UpdateEmitterData(handle, data);
```

### 【NEW】ゲーム中での一時的なエフェクト再生 (爆発など)
シーン内の特定座標に単発（ワンショット）の爆発エフェクトなどを出したい場合は、新しく追加された `Effect` クラスを使用するのが最も簡単です。

```cpp
#include "Renderer/Effect/Effect.h"

// 1. エフェクトインスタンスの作成と初期化（例：爆発）
Effect myEffect;
myEffect.Initialize(EffectType::kExplosion);

// 2. 指定した座標でエフェクトを発生させる
myEffect.Play(Vector3(10.0f, 0.0f, 5.0f));

// 3. 毎フレーム Update と Draw を呼ぶ
myEffect.Update();
myEffect.SyncBeforeDraw();
myEffect.Draw();
```

### インスペクターからの ParticleType などの設定
`ParticleEmitterComponent` を GameObject にアタッチした場合、エディターの **Inspector パネル** から以下の新機能を直感的に操作できます。

- **Particle Mesh & Shape (形状と発生範囲)**
  - `Sphere`, `Beam`, `Ring`, `Cylinder`, `Box` などの発生形状を選択可能です。
  - `Box` を選択した場合のみ、専用の `Area Size (X,Y,Z)` を指定して箱状の範囲内に発生させることができます。
  - **Billboard Mode**: パーティクルのカメラに対する向きを `None` (固定), `Billboard` (常にカメラを向く), `Y-Axis` (Y軸固定でカメラを向く・魔法陣などに最適) から選べます。

- **Animation & Visuals (アニメーションと見た目)**
  - **Atlas Rows / Cols**: 連番テクスチャ（スプライトシート）の分割数を指定するだけで、自動的にアニメーション再生されます。
  - **Start / Mid / End Color & Scale**: これまでの開始/終了だけでなく、「中間色・中間スケール」と「それがどのタイミング(Mid Point)で切り替わるか」を設定でき、爆発（白→オレンジ→黒煙）などの複雑な表現が可能になりました。

- **Physics (物理挙動)**
  - 重力やバウンドに加えて、**Jitter (ジッター)** によって不規則なブレ（ノイズ）を与え、魔法の粉や舞い散る火の粉のようなランダムな動きを表現できます。

これらのパラメータはすべて Inspector のGUIからリアルタイムに変更・確認できます。

---

## ポストプロセス (PostProcessManager) の利用方法と描画順序

画面全体にかけるポストプロセスエフェクト（PostProcessManager）を使用する際は、**「エフェクトをスタックに追加する順番（描画順序）」** を意識することで、プロの現場でも通用する意図した映像表現が可能になります。

### 推奨される描画順序（スタックに追加する順）
1. **色調補正系**: ToneMapping, Grayscale, Sepia, HSV など
2. **空間・ぼかし系**: Smoothing, GaussianFilter, RadialBlur など
3. **画面演出系**: Vignette, Noise, Glitch, Dissolve など
4. **画面遷移系**: Fade, Slide など

**なぜこの順番なのか？**
例えば、`Vignette`（画面の端を暗くする/色をつける演出）のあとに `Grayscale`（白黒化）をかけてしまうと、ビネットで赤色などを指定してもモノクロになってしまいます。「色調補正」を先に行い、その上から「画面演出」を乗せるのがセオリーです。

### 【NEW】Vignetteのパラメータ変更について
Vignetteエフェクトがより自然な減衰（Smooth Falloff）になるようパラメータがアップグレードされました。
- **`radius` (旧: scale)**: 減衰が始まる半径 (デフォルト 0.8)
- **`softness` (旧: power)**: 減衰の柔らかさ (デフォルト 0.5)

これにより、画面端が完全に黒く潰れるのを防ぎ、滑らかなグラデーション表現が可能になっています。シーン初期化時などでパラメータを調整する際はご留意ください。
