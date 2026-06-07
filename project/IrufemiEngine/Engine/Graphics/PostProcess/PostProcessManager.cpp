/**
 * @file PostProcessManager.cpp
 * @brief ポストプロセス（マルチパス描画）の管理実装クラス
 */
#include "PostProcessManager.h"
#include "../DirectX/DirectXCommon.h"
#include "../DirectX/DirectXUtils.h"
#include <algorithm>
#include <cassert>
#include <d3d12.h>
#include <dxcapi.h>
#include <iostream>
#include <string>
#include <wrl/client.h>

void PostProcessManager::Initialize(DirectXCommon* dxCommon,
                                    DXGI_FORMAT rtvFormat) {
  dxCommon_ = dxCommon;
  device_ = dxCommon->GetDevice();
  rootSig_ = dxCommon->GetRootSignature();
  rtvFormat_ = rtvFormat;

  CreateConstantBuffers();
  CreatePSOs();
}

void PostProcessManager::ResetAllParams() {
    noiseParams_ = NoiseParams();
    vignetteParams_ = VignetteParams();
    smoothingParams_ = SmoothingParams();
    gaussianParams_ = GaussianParams();
    radialBlurParams_ = RadialBlurParams();
    outlineParams_ = OutlineParams();
    dissolveParams_ = DissolveParams();
    hsvParams_ = HSVParams();
    toneMappingParams_ = ToneMappingParams();
    fadeParams_ = FadeParams();
    slideParams_ = SlideParams();
    bloomParams_ = BloomParams();
    glitchParams_ = GlitchParams();
}



void PostProcessManager::Update(float totalTime) {
  CommitPendingModes();

  noiseParams_.time = totalTime;
  if (mappedNoise_) {
    *mappedNoise_ = noiseParams_;
  }

  // 他のパラメータも同期
  if (mappedVignette_) *mappedVignette_ = vignetteParams_;
  if (mappedSmoothing_) *mappedSmoothing_ = smoothingParams_;
  if (mappedGaussian_) *mappedGaussian_ = gaussianParams_;
  if (mappedRadialBlur_) *mappedRadialBlur_ = radialBlurParams_;
  if (mappedOutline_) *mappedOutline_ = outlineParams_;
  if (mappedDissolve_) *mappedDissolve_ = dissolveParams_;
  if (mappedHsv_) *mappedHsv_ = hsvParams_;
  if (mappedToneMapping_) *mappedToneMapping_ = toneMappingParams_;
  if (mappedFade_) *mappedFade_ = fadeParams_;
  if (mappedSlide_) *mappedSlide_ = slideParams_;
  if (mappedBloom_) *mappedBloom_ = bloomParams_;

  glitchParams_.time = totalTime;
  if (mappedGlitch_) *mappedGlitch_ = glitchParams_;

  // 統合パラメータの同期
  combinedParams_.vignetteColor = vignetteParams_.color;
  combinedParams_.vignetteRadius = vignetteParams_.radius;
  combinedParams_.vignetteSoftness = vignetteParams_.softness;
  combinedParams_.noiseIntensity = noiseParams_.intensity;
  combinedParams_.noiseTime = noiseParams_.time;
  combinedParams_.dissolveEdgeColor = dissolveParams_.edgeColor;
  combinedParams_.dissolveBackgroundColor = dissolveParams_.backgroundColor;
  combinedParams_.dissolveThreshold = dissolveParams_.threshold;
  combinedParams_.dissolveEdgeRange = dissolveParams_.edgeRange;
  combinedParams_.hsvHue = hsvParams_.hue;
  combinedParams_.hsvSaturation = hsvParams_.saturation;
  combinedParams_.hsvValue = hsvParams_.value;
  combinedParams_.toneMappingExposure = toneMappingParams_.exposure;
  combinedParams_.fadeColor = fadeParams_.color;
  combinedParams_.fadeIntensity = fadeParams_.intensity;
  combinedParams_.slideColor = slideParams_.color;
  combinedParams_.slideThreshold = slideParams_.threshold;
  combinedParams_.projectionInverse = outlineParams_.projectionInverse;
  combinedParams_.outlineIntensity = outlineParams_.intensity;
  combinedParams_.radialBlurCenter = radialBlurParams_.center;
  combinedParams_.radialBlurWidth = radialBlurParams_.blurWidth;
  combinedParams_.radialBlurSamples = radialBlurParams_.numSamples;
  combinedParams_.glitchIntensity = glitchParams_.intensity;
  combinedParams_.glitchTime = glitchParams_.time;

  if (mappedCombined_) {
    *mappedCombined_ = combinedParams_;
  }
}

void PostProcessManager::Draw(ID3D12GraphicsCommandList *commandList,
                               RenderTexture *srcTexture,
                               D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle,
                               const PostProcessWorkspace& workspace) {
  RenderTexture *currentSource = srcTexture;

  if (!activeModes_.empty()) {
    size_t modeIdx = 0;
    int pingPongIdx = 0;

    while (modeIdx < activeModes_.size()) {
      Mode mode = activeModes_[modeIdx];
      bool isLastBatch = false;

      // 1) 非統合エフェクト (Bloom) の処理
      if (mode == Mode::Bloom) {
        isLastBatch = (modeIdx == activeModes_.size() - 1);
        RenderTexture* nextTarget = isLastBatch ? nullptr : workspace.workTextures[pingPongIdx % 2];
        D3D12_CPU_DESCRIPTOR_HANDLE targetHandle = isLastBatch ? rtvHandle : nextTarget->GetRtvHandle();

        if (!isLastBatch) {
            DirectXUtils::TransitionBarrier(commandList, nextTarget->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        }

        RenderTexture* bloomExtract = workspace.bloomExtract;
        RenderTexture* blurH = workspace.bloomBlur;
        RenderTexture* blurV = workspace.bloomExtract;

        DirectXUtils::TransitionBarrier(commandList, bloomExtract->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        DrawSinglePass(commandList, Mode::Bloom, currentSource, bloomExtract->GetRtvHandle(), false, bloomExtractPSO_.Get());
        DirectXUtils::TransitionBarrier(commandList, bloomExtract->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        
        bloomParams_.direction = { 1.0f, 0.0f };
        if (mappedBloom_) { *mappedBloom_ = bloomParams_; }
        DirectXUtils::TransitionBarrier(commandList, blurH->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        DrawSinglePass(commandList, Mode::Bloom, bloomExtract, blurH->GetRtvHandle(), false, bloomBlurHPSO_.Get());
        DirectXUtils::TransitionBarrier(commandList, blurH->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        
        bloomParams_.direction = { 0.0f, 1.0f };
        if (mappedBloom_) { *mappedBloom_ = bloomParams_; }
        DirectXUtils::TransitionBarrier(commandList, blurV->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        DrawSinglePass(commandList, Mode::Bloom, blurH, blurV->GetRtvHandle(), false, bloomBlurVPSO_.Get());
        DirectXUtils::TransitionBarrier(commandList, blurV->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        
        commandList->OMSetRenderTargets(1, &targetHandle, false, nullptr);
        float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
        commandList->ClearRenderTargetView(targetHandle, clearColor, 0, nullptr);
        commandList->SetPipelineState(isLastBatch ? finalBloomCombinePSO_.Get() : bloomCombinePSO_.Get());
        commandList->SetGraphicsRootSignature(rootSig_);
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->SetGraphicsRootDescriptorTable((UINT)RootSlot::Texture, currentSource->GetSrvHandleGPU());
        commandList->SetGraphicsRootDescriptorTable((UINT)RootSlot::EnvMap, blurV->GetSrvHandleGPU());
        commandList->SetGraphicsRootConstantBufferView((UINT)RootSlot::Material, bloomCB_->GetGPUVirtualAddress());
        commandList->DrawInstanced(3, 1, 0, 0);

        if (!isLastBatch) {
          DirectXUtils::TransitionBarrier(commandList, nextTarget->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
          pingPongIdx++;
        }
        currentSource = nextTarget;
        modeIdx++;
      }
      // 1-B) 分離可能フィルタ (Smoothing / GaussianFilter) の処理
      else if (mode == Mode::Smoothing || mode == Mode::GaussianFilter) {
        isLastBatch = (modeIdx == activeModes_.size() - 1);
        RenderTexture* nextTarget = isLastBatch ? nullptr : workspace.workTextures[pingPongIdx % 2];
        D3D12_CPU_DESCRIPTOR_HANDLE targetHandle = isLastBatch ? rtvHandle : nextTarget->GetRtvHandle();

        // 横方向パス用の中間バッファ（BloomのBlurバッファを一時的に借用）
        RenderTexture* blurH = workspace.bloomBlur;
        
        DirectXUtils::TransitionBarrier(commandList, blurH->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

        ID3D12PipelineState* psoH = (mode == Mode::Smoothing) ? smoothingBlurPSO_.Get() : gaussianBlurPSO_.Get();
        ID3D12PipelineState* psoV = nullptr;
        if (mode == Mode::Smoothing) {
            psoV = isLastBatch ? finalSmoothingBlurPSO_.Get() : smoothingBlurPSO_.Get();
        } else {
            psoV = isLastBatch ? finalGaussianBlurPSO_.Get() : gaussianBlurPSO_.Get();
        }

        // H Pass
        if (mode == Mode::Smoothing) {
            smoothingParams_.direction = { 1.0f, 0.0f };
            if (mappedSmoothing_) { *mappedSmoothing_ = smoothingParams_; }
        } else {
            gaussianParams_.direction = { 1.0f, 0.0f };
            if (mappedGaussian_) { *mappedGaussian_ = gaussianParams_; }
        }
        DrawSinglePass(commandList, mode, currentSource, blurH->GetRtvHandle(), false, psoH);
        
        DirectXUtils::TransitionBarrier(commandList, blurH->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        // V Pass
        if (!isLastBatch) {
            DirectXUtils::TransitionBarrier(commandList, nextTarget->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        }

        if (mode == Mode::Smoothing) {
            smoothingParams_.direction = { 0.0f, 1.0f };
            if (mappedSmoothing_) { *mappedSmoothing_ = smoothingParams_; }
        } else {
            gaussianParams_.direction = { 0.0f, 1.0f };
            if (mappedGaussian_) { *mappedGaussian_ = gaussianParams_; }
        }
        
        commandList->OMSetRenderTargets(1, &targetHandle, false, nullptr);
        float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
        commandList->ClearRenderTargetView(targetHandle, clearColor, 0, nullptr);
        DrawSinglePass(commandList, mode, blurH, targetHandle, isLastBatch, psoV);

        if (!isLastBatch) {
          DirectXUtils::TransitionBarrier(commandList, nextTarget->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
          pingPongIdx++;
        }
        currentSource = nextTarget;
        modeIdx++;
      }
      // 2) 統合バッチ
      else {
        std::vector<Mode> batch;
        size_t lookAhead = modeIdx;
        while (lookAhead < activeModes_.size() && 
               activeModes_[lookAhead] != Mode::Bloom && 
               activeModes_[lookAhead] != Mode::Smoothing && 
               activeModes_[lookAhead] != Mode::GaussianFilter && 
               batch.size() < 16) {
          batch.push_back(activeModes_[lookAhead]);
          lookAhead++;
        }

        isLastBatch = (lookAhead == activeModes_.size());
        RenderTexture* nextTarget = isLastBatch ? nullptr : workspace.workTextures[pingPongIdx % 2];
        D3D12_CPU_DESCRIPTOR_HANDLE targetHandle = isLastBatch ? rtvHandle : nextTarget->GetRtvHandle();

        if (!isLastBatch) {
            DirectXUtils::TransitionBarrier(commandList, nextTarget->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        }

        combinedParams_.effectCount = (int32_t)batch.size();
        D3D12_GPU_DESCRIPTOR_HANDLE extraSrv = { 0 };
        for (int i = 0; i < (int)batch.size(); ++i) {
          combinedParams_.effects[i] = (int32_t)batch[i];
          if (batch[i] == Mode::DepthBasedOutline) {
              extraSrv = depthSrvHandle_;
          } else if (batch[i] == Mode::Dissolve) {
              int noiseIdx = (dissolveParams_.noiseType <= 0) ? 0 : 1;
              extraSrv = dissolveNoiseHandle_[noiseIdx];
          }
        }
        if (mappedCombined_) { *mappedCombined_ = combinedParams_; }

        commandList->OMSetRenderTargets(1, &targetHandle, false, nullptr);
        float clearColor[] = { 0, 0, 0, 1 };
        commandList->ClearRenderTargetView(targetHandle, clearColor, 0, nullptr);

        commandList->SetPipelineState(isLastBatch ? finalCombinedPSO_.Get() : combinedPSO_.Get());
        commandList->SetGraphicsRootSignature(rootSig_);
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->SetGraphicsRootDescriptorTable((UINT)RootSlot::Texture, currentSource->GetSrvHandleGPU());
        if (extraSrv.ptr != 0) {
          commandList->SetGraphicsRootDescriptorTable((UINT)RootSlot::EnvMap, extraSrv);
        } else {
          commandList->SetGraphicsRootDescriptorTable((UINT)RootSlot::EnvMap, currentSource->GetSrvHandleGPU());
        }
        commandList->SetGraphicsRootConstantBufferView((UINT)RootSlot::Material, combinedCB_->GetGPUVirtualAddress());
        commandList->DrawInstanced(3, 1, 0, 0);

        if (!isLastBatch) {
          DirectXUtils::TransitionBarrier(commandList, nextTarget->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
          pingPongIdx++;
        }
        currentSource = nextTarget;
        modeIdx = lookAhead;
      }
    }
  } else {
    Mode singleMode = activeModes_.empty() ? Mode::None : activeModes_[0];
    DrawSinglePass(commandList, singleMode, srcTexture, rtvHandle, true);
  }
}

void PostProcessManager::DrawSinglePass(ID3D12GraphicsCommandList *commandList,
                                        Mode mode, RenderTexture *srcTexture,
                                        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle,
                                        bool isFinalPass,
                                        ID3D12PipelineState* psoOverride) {
  uint32_t modeIdx = static_cast<uint32_t>(mode);
  if (!psoOverride && (modeIdx >= psos_.size() || !psos_[modeIdx]))
    return;

  // レンダーターゲットの設定
  commandList->OMSetRenderTargets(1, &rtvHandle, false, nullptr);

  // 描画前にターゲットをクリアする（Shaderで discard された箇所が背景色として残るように）
  float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
  if (mode == Mode::Dissolve) {
    clearColor[0] = dissolveParams_.backgroundColor.x;
    clearColor[1] = dissolveParams_.backgroundColor.y;
    clearColor[2] = dissolveParams_.backgroundColor.z;
    clearColor[3] = dissolveParams_.backgroundColor.w;
  }
  commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

  // PSOとルートシグネチャの設定
  ID3D12PipelineState* pso = psoOverride;
  if (!pso) {
      pso = isFinalPass ? finalPsos_[modeIdx].Get() : psos_[modeIdx].Get();
  }
  if (!pso) return;

  commandList->SetPipelineState(pso);
  commandList->SetGraphicsRootSignature(rootSig_);
  commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // ソーステクスチャのバインド (Root2 -> t0)
  commandList->SetGraphicsRootDescriptorTable((UINT)RootSlot::Texture, srcTexture->GetSrvHandleGPU());

  // 定数バッファのバインド (Root0 -> b0)
  D3D12_GPU_VIRTUAL_ADDRESS cbvAddress = 0;
  switch (mode) {
  case Mode::Vignette:
    cbvAddress = vignetteCB_->GetGPUVirtualAddress();
    break;
  case Mode::Smoothing:
    cbvAddress = smoothingCB_->GetGPUVirtualAddress();
    break;
  case Mode::GaussianFilter:
    cbvAddress = gaussianCB_->GetGPUVirtualAddress();
    break;
  case Mode::RadialBlur:
    cbvAddress = radialBlurCB_->GetGPUVirtualAddress();
    break;
  case Mode::DepthBasedOutline:
    cbvAddress = outlineCB_->GetGPUVirtualAddress();
    break;
  case Mode::Dissolve:
    cbvAddress = dissolveCB_->GetGPUVirtualAddress();
    break;
  case Mode::Noise:
    cbvAddress = noiseCB_->GetGPUVirtualAddress();
    break;
  case Mode::HSV:
    cbvAddress = hsvCB_->GetGPUVirtualAddress();
    break;
  case Mode::ToneMapping:
    cbvAddress = toneMappingCB_->GetGPUVirtualAddress();
    break;
  case Mode::Fade:
    cbvAddress = fadeCB_->GetGPUVirtualAddress();
    break;
  case Mode::Slide:
    cbvAddress = slideCB_->GetGPUVirtualAddress();
    break;
  case Mode::Bloom:
    cbvAddress = bloomCB_->GetGPUVirtualAddress();
    break;
  case Mode::Glitch:
    cbvAddress = glitchCB_->GetGPUVirtualAddress();
    break;
  default:
    break;
  }
  if (cbvAddress != 0) {
    commandList->SetGraphicsRootConstantBufferView((UINT)RootSlot::Material, cbvAddress);
  }

  // 追加のリソース（深度、ノイズテクスチャ）のバインド (Root12 -> t1)
  if (mode == Mode::DepthBasedOutline) {
    commandList->SetGraphicsRootDescriptorTable((UINT)RootSlot::EnvMap, depthSrvHandle_);
  } else if (mode == Mode::Dissolve) {
    int32_t noiseIdx = (std::max)(int32_t(0), (std::min)(int32_t(1), dissolveParams_.noiseType));
    commandList->SetGraphicsRootDescriptorTable((UINT)RootSlot::EnvMap, dissolveNoiseHandle_[noiseIdx]);
  }

  // 描画実行 (3頂点インデックスなし)
  commandList->DrawInstanced(3, 1, 0, 0);
}

void PostProcessManager::CreatePSOs() {
  auto* shaderManager = dxCommon_->GetShaderManager();
  
  // --- シェーダコンパイル設定 ---
  ShaderCompileOptions options;
#if defined(_DEBUG) || defined(DEVELOPMENT) || defined(EditorMode)
  options.isDebug = true;
#endif

  auto vsBlob = shaderManager->GetOrCompile(L"resources/shaders/Fullscreen.VS.hlsl", options);

  struct ShaderPath {
    Mode mode;
    std::wstring path;
  };
  std::vector<ShaderPath> shaders = {
      {Mode::None, L"resources/shaders/CopyImage.PS.hlsl"},
      {Mode::Grayscale, L"resources/shaders/Grayscale.PS.hlsl"},
      {Mode::Sepia, L"resources/shaders/Sepia.PS.hlsl"},
      {Mode::Vignette, L"resources/shaders/Vignette.PS.hlsl"},
      {Mode::Smoothing, L"resources/shaders/Smoothing.PS.hlsl"},
      {Mode::GaussianFilter, L"resources/shaders/GaussianFilter.PS.hlsl"},
      {Mode::DepthBasedOutline, L"resources/shaders/DepthBasedOutline.PS.hlsl"},
      {Mode::RadialBlur, L"resources/shaders/RadialBlur.PS.hlsl"},
      {Mode::Dissolve, L"resources/shaders/Dissolve.PS.hlsl"},
      {Mode::Noise, L"resources/shaders/Noise.PS.hlsl"},
      {Mode::HSV, L"resources/shaders/HSV.PS.hlsl"},
      {Mode::ToneMapping, L"resources/shaders/ToneMapping.PS.hlsl"},
      {Mode::Fade, L"resources/shaders/Fade.PS.hlsl"},
      {Mode::Slide, L"resources/shaders/Slide.PS.hlsl"},
      {Mode::Bloom, L"resources/shaders/CopyImage.PS.hlsl"},
      {Mode::Glitch, L"resources/shaders/Glitch.PS.hlsl"},
  };

  for (const auto &s : shaders) {
    auto psBlob = shaderManager->GetOrCompile(s.path, options);
    if (!psBlob)
      continue;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = rootSig_;
    desc.VS = {vsBlob->GetBufferPointer(), vsBlob->GetBufferSize()};
    desc.PS = {psBlob->GetBufferPointer(), psBlob->GetBufferSize()};

    desc.BlendState.RenderTarget[0].RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_ALL;
    desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    desc.DepthStencilState.DepthEnable = FALSE;
    desc.DepthStencilState.StencilEnable = FALSE;
    desc.InputLayout = {nullptr, 0};
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets = 1;
    desc.SampleDesc.Count = 1;

    // 中間パス用 (_UNORM)
    desc.RTVFormats[0] = rtvFormat_;
    device_->CreateGraphicsPipelineState(
        &desc, IID_PPV_ARGS(&psos_[static_cast<uint32_t>(s.mode)]));

    // 最終パス用 (_SRGB)
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    device_->CreateGraphicsPipelineState(
        &desc, IID_PPV_ARGS(&finalPsos_[static_cast<uint32_t>(s.mode)]));
  }

  // --- ブルーム用個別 PSO ---
  auto extractPS = shaderManager->GetOrCompile(L"resources/shaders/HighLuminanceExtract.PS.hlsl", options);
  auto blurPS = shaderManager->GetOrCompile(L"resources/shaders/GaussianBlur.PS.hlsl", options);
  auto combinePS = shaderManager->GetOrCompile(L"resources/shaders/BloomCombine.PS.hlsl", options);

  D3D12_GRAPHICS_PIPELINE_STATE_DESC bloomDesc{};
  bloomDesc.pRootSignature = rootSig_;
  bloomDesc.VS = {vsBlob->GetBufferPointer(), vsBlob->GetBufferSize()};
  bloomDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
  bloomDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
  bloomDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
  bloomDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  bloomDesc.DepthStencilState.DepthEnable = FALSE;
  bloomDesc.DepthStencilState.StencilEnable = FALSE;
  bloomDesc.InputLayout = {nullptr, 0};
  bloomDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  bloomDesc.NumRenderTargets = 1;
  bloomDesc.RTVFormats[0] = rtvFormat_;
  bloomDesc.SampleDesc.Count = 1;

  bloomDesc.PS = {extractPS->GetBufferPointer(), extractPS->GetBufferSize()};
  device_->CreateGraphicsPipelineState(&bloomDesc, IID_PPV_ARGS(&bloomExtractPSO_));

  bloomDesc.PS = {blurPS->GetBufferPointer(), blurPS->GetBufferSize()};
  device_->CreateGraphicsPipelineState(&bloomDesc, IID_PPV_ARGS(&bloomBlurHPSO_));
  device_->CreateGraphicsPipelineState(&bloomDesc, IID_PPV_ARGS(&bloomBlurVPSO_));

    bloomDesc.PS = {combinePS->GetBufferPointer(), combinePS->GetBufferSize()};
    bloomDesc.RTVFormats[0] = rtvFormat_; // 中間パス用 (_UNORM)
    device_->CreateGraphicsPipelineState(&bloomDesc, IID_PPV_ARGS(&bloomCombinePSO_));

    bloomDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // 最終パス用 (_SRGB)
    device_->CreateGraphicsPipelineState(&bloomDesc, IID_PPV_ARGS(&finalBloomCombinePSO_));

    // --- 統合ポストプロセス用 PSO ---
    auto combinedPS = shaderManager->GetOrCompile(L"resources/shaders/PostProcess.PS.hlsl", options);
    D3D12_GRAPHICS_PIPELINE_STATE_DESC combinedDesc = bloomDesc; // ベースを流用
    combinedDesc.PS = {combinedPS->GetBufferPointer(), combinedPS->GetBufferSize()};
    
    // 中間パス用
    combinedDesc.RTVFormats[0] = rtvFormat_;
    device_->CreateGraphicsPipelineState(&combinedDesc, IID_PPV_ARGS(&combinedPSO_));
    // 最終パス用
    combinedDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    device_->CreateGraphicsPipelineState(&combinedDesc, IID_PPV_ARGS(&finalCombinedPSO_));

    // --- 分離可能フィルタ用 PSO ---
    auto boxBlurPS = shaderManager->GetOrCompile(L"resources/shaders/BoxBlur.PS.hlsl", options);
    D3D12_GRAPHICS_PIPELINE_STATE_DESC sepDesc = bloomDesc;
    sepDesc.RTVFormats[0] = rtvFormat_;
    
    sepDesc.PS = {boxBlurPS->GetBufferPointer(), boxBlurPS->GetBufferSize()};
    device_->CreateGraphicsPipelineState(&sepDesc, IID_PPV_ARGS(&smoothingBlurPSO_));
    sepDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    device_->CreateGraphicsPipelineState(&sepDesc, IID_PPV_ARGS(&finalSmoothingBlurPSO_));

    sepDesc.RTVFormats[0] = rtvFormat_;
    auto gaussianPS = shaderManager->GetOrCompile(L"resources/shaders/GaussianFilter.PS.hlsl", options);
    sepDesc.PS = {gaussianPS->GetBufferPointer(), gaussianPS->GetBufferSize()};
    device_->CreateGraphicsPipelineState(&sepDesc, IID_PPV_ARGS(&gaussianBlurPSO_));
    sepDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    device_->CreateGraphicsPipelineState(&sepDesc, IID_PPV_ARGS(&finalGaussianBlurPSO_));
}

void PostProcessManager::CreateConstantBuffers() {
    combinedCB_ = CreateBuffer(sizeof(CombinedParams));
    combinedCB_->Map(0, nullptr, reinterpret_cast<void**>(&mappedCombined_));
  noiseCB_ = CreateBuffer(sizeof(NoiseParams));
  noiseCB_->Map(0, nullptr, reinterpret_cast<void **>(&mappedNoise_));

  vignetteCB_ = CreateBuffer(sizeof(VignetteParams));
  vignetteCB_->Map(0, nullptr, reinterpret_cast<void **>(&mappedVignette_));

  smoothingCB_ = CreateBuffer(sizeof(SmoothingParams));
  smoothingCB_->Map(0, nullptr, reinterpret_cast<void **>(&mappedSmoothing_));

  gaussianCB_ = CreateBuffer(sizeof(GaussianParams));
  gaussianCB_->Map(0, nullptr, reinterpret_cast<void **>(&mappedGaussian_));

  radialBlurCB_ = CreateBuffer(sizeof(RadialBlurParams));
  radialBlurCB_->Map(0, nullptr, reinterpret_cast<void **>(&mappedRadialBlur_));

  outlineCB_ = CreateBuffer(sizeof(OutlineParams));
  outlineCB_->Map(0, nullptr, reinterpret_cast<void **>(&mappedOutline_));

  dissolveCB_ = CreateBuffer(sizeof(DissolveParams));
  dissolveCB_->Map(0, nullptr, reinterpret_cast<void **>(&mappedDissolve_));

  hsvCB_ = CreateBuffer(sizeof(HSVParams));
  hsvCB_->Map(0, nullptr, reinterpret_cast<void **>(&mappedHsv_));

  toneMappingCB_ = CreateBuffer(sizeof(ToneMappingParams));
  toneMappingCB_->Map(0, nullptr, reinterpret_cast<void **>(&mappedToneMapping_));

  fadeCB_ = CreateBuffer(sizeof(FadeParams));
  fadeCB_->Map(0, nullptr, reinterpret_cast<void **>(&mappedFade_));

  slideCB_ = CreateBuffer(sizeof(SlideParams));
  slideCB_->Map(0, nullptr, reinterpret_cast<void **>(&mappedSlide_));

  bloomCB_ = CreateBuffer(sizeof(BloomParams));
  bloomCB_->Map(0, nullptr, reinterpret_cast<void **>(&mappedBloom_));

  glitchCB_ = CreateBuffer(sizeof(GlitchParams));
  glitchCB_->Map(0, nullptr, reinterpret_cast<void **>(&mappedGlitch_));
}

Microsoft::WRL::ComPtr<ID3D12Resource>
PostProcessManager::CreateBuffer(size_t size) {
  D3D12_HEAP_PROPERTIES heapProps{};
  heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

  D3D12_RESOURCE_DESC desc{};
  desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  desc.Width = (size + 255) & ~255;
  desc.Height = 1;
  desc.DepthOrArraySize = 1;
  desc.MipLevels = 1;
  desc.SampleDesc.Count = 1;
  desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

  Microsoft::WRL::ComPtr<ID3D12Resource> resource;
  device_->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
                                   D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                   IID_PPV_ARGS(&resource));
  return resource;
}
