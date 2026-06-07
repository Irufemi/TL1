#include "DXRootSignatureManager.h"
#include "../../Core/Utility/Log.h"
#include <cassert>

void DXRootSignatureManager::Initialize(ID3D12Device* device, Log* log) {
    // --- 通常描画用 RootSignature ---
    {
        // --- ディスクリプタレンジの定義 ---

        D3D12_DESCRIPTOR_RANGE rangeTexture[1] = {};
        rangeTexture[0].BaseShaderRegister = 0; // t0
        rangeTexture[0].NumDescriptors = 1;
        rangeTexture[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        rangeTexture[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_DESCRIPTOR_RANGE rangeInstancing[1] = {};
        rangeInstancing[0].BaseShaderRegister = 0; // t0
        rangeInstancing[0].NumDescriptors = 1;
        rangeInstancing[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        rangeInstancing[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_DESCRIPTOR_RANGE rangeEnv[1] = {};
        rangeEnv[0].BaseShaderRegister = 1; // t1
        rangeEnv[0].NumDescriptors = 1;
        rangeEnv[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        rangeEnv[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_DESCRIPTOR_RANGE rangeLine[1] = {};
        rangeLine[0].BaseShaderRegister = 1; // t1
        rangeLine[0].NumDescriptors = 1;
        rangeLine[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        rangeLine[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        // ライトSRVテーブル (t2, t3, t4 を一括バインド)
        D3D12_DESCRIPTOR_RANGE rangeLights[1] = {};
        rangeLights[0].BaseShaderRegister = 2; // t2 から開始
        rangeLights[0].NumDescriptors = 3;     // t2, t3, t4 の3つ分
        rangeLights[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        rangeLights[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        // シャドウマップ (t5)
        D3D12_DESCRIPTOR_RANGE rangeShadow[1] = {};
        rangeShadow[0].BaseShaderRegister = 5; // t5
        rangeShadow[0].NumDescriptors = 1;
        rangeShadow[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        rangeShadow[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        // --- ルートパラメータの定義 ---
        D3D12_ROOT_PARAMETER rootParameters[11] = {};

        // Slot 0: Material (b0, PS)
        rootParameters[(UINT)RootSlot::Material].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[(UINT)RootSlot::Material].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rootParameters[(UINT)RootSlot::Material].Descriptor.ShaderRegister = 0;

        // Slot 1: Transform (b0, VS)
        rootParameters[(UINT)RootSlot::Transform].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[(UINT)RootSlot::Transform].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        rootParameters[(UINT)RootSlot::Transform].Descriptor.ShaderRegister = 0;

        // Slot 2: Texture (t0, PS)
        rootParameters[(UINT)RootSlot::Texture].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[(UINT)RootSlot::Texture].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rootParameters[(UINT)RootSlot::Texture].DescriptorTable.pDescriptorRanges = rangeTexture;
        rootParameters[(UINT)RootSlot::Texture].DescriptorTable.NumDescriptorRanges = 1;

        // Slot 3: LightCommon (b1, ALL)
        rootParameters[(UINT)RootSlot::LightCommon].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[(UINT)RootSlot::LightCommon].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters[(UINT)RootSlot::LightCommon].Descriptor.ShaderRegister = 1;

        // Slot 4: Instancing (t0, VS)
        rootParameters[(UINT)RootSlot::Instancing].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[(UINT)RootSlot::Instancing].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        rootParameters[(UINT)RootSlot::Instancing].DescriptorTable.pDescriptorRanges = rangeInstancing;
        rootParameters[(UINT)RootSlot::Instancing].DescriptorTable.NumDescriptorRanges = 1;

        // Slot 5: Camera (b2, ALL)
        rootParameters[(UINT)RootSlot::Camera].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[(UINT)RootSlot::Camera].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters[(UINT)RootSlot::Camera].Descriptor.ShaderRegister = 2;

        // Slot 6: Lights (t2-t4, PS)
        rootParameters[(UINT)RootSlot::Lights].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[(UINT)RootSlot::Lights].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rootParameters[(UINT)RootSlot::Lights].DescriptorTable.pDescriptorRanges = rangeLights;
        rootParameters[(UINT)RootSlot::Lights].DescriptorTable.NumDescriptorRanges = 1;

        // Slot 7: Special (b6, ALL)
        rootParameters[(UINT)RootSlot::Special].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[(UINT)RootSlot::Special].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters[(UINT)RootSlot::Special].Descriptor.ShaderRegister = 6;

        // Slot 8: EnvMap (t1, PS)
        rootParameters[(UINT)RootSlot::EnvMap].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[(UINT)RootSlot::EnvMap].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rootParameters[(UINT)RootSlot::EnvMap].DescriptorTable.pDescriptorRanges = rangeEnv;
        rootParameters[(UINT)RootSlot::EnvMap].DescriptorTable.NumDescriptorRanges = 1;

        // Slot 9: LineInstancing (t1, VS)
        rootParameters[(UINT)RootSlot::LineInstancing].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[(UINT)RootSlot::LineInstancing].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        rootParameters[(UINT)RootSlot::LineInstancing].DescriptorTable.pDescriptorRanges = rangeLine;
        rootParameters[(UINT)RootSlot::LineInstancing].DescriptorTable.NumDescriptorRanges = 1;

        // Slot 10: ShadowMap (t5, PS)
        rootParameters[(UINT)RootSlot::ShadowMap].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[(UINT)RootSlot::ShadowMap].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rootParameters[(UINT)RootSlot::ShadowMap].DescriptorTable.pDescriptorRanges = rangeShadow;
        rootParameters[(UINT)RootSlot::ShadowMap].DescriptorTable.NumDescriptorRanges = 1;

        D3D12_STATIC_SAMPLER_DESC staticSamplers[5] = {};
        staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
        staticSamplers[0].ShaderRegister = 0;
        staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        staticSamplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        staticSamplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        staticSamplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        staticSamplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        staticSamplers[1].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        staticSamplers[1].MaxLOD = D3D12_FLOAT32_MAX;
        staticSamplers[1].ShaderRegister = 1;
        staticSamplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        staticSamplers[2].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
        staticSamplers[2].AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        staticSamplers[2].AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        staticSamplers[2].AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        staticSamplers[2].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        staticSamplers[2].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
        staticSamplers[2].MaxLOD = D3D12_FLOAT32_MAX;
        staticSamplers[2].ShaderRegister = 2; // s2
        staticSamplers[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        staticSamplers[3].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        staticSamplers[3].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        staticSamplers[3].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        staticSamplers[3].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        staticSamplers[3].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        staticSamplers[3].MaxLOD = D3D12_FLOAT32_MAX;
        staticSamplers[3].ShaderRegister = 3; // s3
        staticSamplers[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        // s4: UはWRAP、VはCLAMP (横スクロール対応等)
        staticSamplers[4].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        staticSamplers[4].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        staticSamplers[4].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        staticSamplers[4].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        staticSamplers[4].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        staticSamplers[4].MaxLOD = D3D12_FLOAT32_MAX;
        staticSamplers[4].ShaderRegister = 4; // s4
        staticSamplers[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_ROOT_SIGNATURE_DESC rsDesc{};
        rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        rsDesc.pParameters = rootParameters;
        rsDesc.NumParameters = _countof(rootParameters);
        rsDesc.pStaticSamplers = staticSamplers;
        rsDesc.NumStaticSamplers = _countof(staticSamplers);

        Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
        Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
        HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, signatureBlob.GetAddressOf(), errorBlob.GetAddressOf());
        if (FAILED(hr)) {
            Log::OutPutLog(log->GetLogStream(), reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
            assert(false);
        }
        hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(graphicsRootSignature_.GetAddressOf()));
        assert(SUCCEEDED(hr));
    }

    // --- Compute Shader用 RootSignature ---
    {
        D3D12_DESCRIPTOR_RANGE srvRanges[3];
        srvRanges[0] = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND }; // t0
        srvRanges[1] = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND }; // t1
        srvRanges[2] = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND }; // t2

        D3D12_DESCRIPTOR_RANGE uavRanges[4];
        uavRanges[0] = { D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND }; // u0
        uavRanges[1] = { D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND }; // u1
        uavRanges[2] = { D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 2, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND }; // u2
        uavRanges[3] = { D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 3, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND }; // u3

        D3D12_ROOT_PARAMETER computeRootParameters[10] = {};
        computeRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        computeRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        computeRootParameters[0].DescriptorTable.pDescriptorRanges = &srvRanges[0];
        computeRootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

        computeRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        computeRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        computeRootParameters[1].DescriptorTable.pDescriptorRanges = &srvRanges[1];
        computeRootParameters[1].DescriptorTable.NumDescriptorRanges = 1;

        computeRootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        computeRootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        computeRootParameters[2].DescriptorTable.pDescriptorRanges = &srvRanges[2];
        computeRootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

        computeRootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        computeRootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        computeRootParameters[3].DescriptorTable.pDescriptorRanges = &uavRanges[0];
        computeRootParameters[3].DescriptorTable.NumDescriptorRanges = 1;

        computeRootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        computeRootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        computeRootParameters[4].Descriptor.ShaderRegister = 0; // b0

        computeRootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        computeRootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        computeRootParameters[5].Descriptor.ShaderRegister = 1; // b1

        computeRootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        computeRootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        computeRootParameters[6].DescriptorTable.pDescriptorRanges = &uavRanges[1];
        computeRootParameters[6].DescriptorTable.NumDescriptorRanges = 1;

        computeRootParameters[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        computeRootParameters[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        computeRootParameters[7].DescriptorTable.pDescriptorRanges = &uavRanges[2];
        computeRootParameters[7].DescriptorTable.NumDescriptorRanges = 1;

        computeRootParameters[8].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        computeRootParameters[8].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        computeRootParameters[8].DescriptorTable.pDescriptorRanges = &uavRanges[3];
        computeRootParameters[8].DescriptorTable.NumDescriptorRanges = 1;

        computeRootParameters[9].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        computeRootParameters[9].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        computeRootParameters[9].Constants.ShaderRegister = 2; // b2
        computeRootParameters[9].Constants.Num32BitValues = 2; // k, j
        computeRootParameters[9].Constants.RegisterSpace = 0;

        D3D12_ROOT_SIGNATURE_DESC computeRSDesc{};
        computeRSDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
        computeRSDesc.pParameters = computeRootParameters;
        computeRSDesc.NumParameters = _countof(computeRootParameters);

        Microsoft::WRL::ComPtr<ID3DBlob> computeSignatureBlob = nullptr;
        Microsoft::WRL::ComPtr<ID3DBlob> computeErrorBlob = nullptr;
        HRESULT hr = D3D12SerializeRootSignature(&computeRSDesc, D3D_ROOT_SIGNATURE_VERSION_1, computeSignatureBlob.GetAddressOf(), computeErrorBlob.GetAddressOf());
        if (FAILED(hr)) {
            Log::OutPutLog(log->GetLogStream(), reinterpret_cast<char*>(computeErrorBlob->GetBufferPointer()));
            assert(false);
        }
        hr = device->CreateRootSignature(0, computeSignatureBlob->GetBufferPointer(), computeSignatureBlob->GetBufferSize(), IID_PPV_ARGS(computeRootSignature_.GetAddressOf()));
        assert(SUCCEEDED(hr));
    }
}

void DXRootSignatureManager::Finalize() {
    graphicsRootSignature_.Reset();
    computeRootSignature_.Reset();
}
