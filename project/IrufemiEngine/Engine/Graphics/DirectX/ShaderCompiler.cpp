#include "ShaderCompiler.h"
#include "../../Core/Utility/Log.h"
#include "../../Core/Utility/StringUtility.h"
#include <format>
#include <cassert>
#include <Windows.h>

/**
 * @brief 初期化
 */
void ShaderCompiler::Initialize() {
    HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(dxcUtils_.GetAddressOf()));
    assert(SUCCEEDED(hr));
    hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(dxcCompiler_.GetAddressOf()));
    assert(SUCCEEDED(hr));
    hr = dxcUtils_->CreateDefaultIncludeHandler(includeHandler_.GetAddressOf());
    assert(SUCCEEDED(hr));
}

/**
 * @brief シェーダのコンパイル
 * @param[in] filePath HLSLファイルへのパス
 * @param[in] profile コンパイルプロファイル
 * @param[in] options コンパイルオプション
 * @return コンパイルされたシェーダのBlob
 */
Microsoft::WRL::ComPtr<IDxcBlob> ShaderCompiler::Compile(
    const std::wstring& filePath,
    const wchar_t* profile,
    const ShaderCompileOptions& options
) {
    // 1. HLSLファイルの読み込み
    Microsoft::WRL::ComPtr<IDxcBlobEncoding> shaderSource;
    HRESULT hr = dxcUtils_->LoadFile(filePath.c_str(), nullptr, &shaderSource);
    if (FAILED(hr)) {
        assert(false && "Failed to load shader file.");
        return nullptr;
    }

    DxcBuffer shaderSourceBuffer;
    shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
    shaderSourceBuffer.Size = shaderSource->GetBufferSize();
    shaderSourceBuffer.Encoding = DXC_CP_UTF8;

    // 2. コンパイル引数の構築
    std::vector<LPCWSTR> arguments;
    arguments.push_back(filePath.c_str());
    arguments.push_back(L"-E"); arguments.push_back(options.entryPoint.c_str());
    arguments.push_back(L"-T"); arguments.push_back(profile);
    arguments.push_back(L"-Zpr"); // 行優先(Row Major)

    // デバッグ設定
    if (options.isDebug) {
        arguments.push_back(L"-Zi");            // デバッグ情報の生成
        arguments.push_back(L"-Qembed_debug");   // PDBをBlobに埋め込む
        arguments.push_back(L"-Od");            // 最適化オフ
    } else {
        arguments.push_back(L"-O3");            // 最大最適化
    }

    // マクロ定義の追加
    std::vector<std::wstring> macroStorage;
    macroStorage.reserve(options.macros.size());
    for (const auto& macro : options.macros) {
        arguments.push_back(L"-D");
        if (macro.second.empty()) {
            macroStorage.push_back(macro.first);
        } else {
            macroStorage.push_back(macro.first + L"=" + macro.second);
        }
        arguments.push_back(macroStorage.back().c_str());
    }

    // 3. コンパイル実行
    Microsoft::WRL::ComPtr<IDxcResult> shaderResult;
    hr = dxcCompiler_->Compile(
        &shaderSourceBuffer,
        arguments.data(),
        static_cast<UINT32>(arguments.size()),
        includeHandler_.Get(),
        IID_PPV_ARGS(&shaderResult)
    );
    assert(SUCCEEDED(hr));

    // 4. エラー・警告の確認
    Microsoft::WRL::ComPtr<IDxcBlobUtf8> shaderError;
    shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
    if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
        // デバッグ出力
        std::string errStr = shaderError->GetStringPointer();
        
        // どのファイルか分かるようにする
        std::string fileStr = ConvertString(filePath);
        std::string fullErr = "Shader Compile Error in " + fileStr + ":\n" + errStr;
        
        OutputDebugStringA(fullErr.c_str());
        
        // ログファイルにも出力
        FILE* f;
        fopen_s(&f, "shader_error.txt", "w");
        if (f) {
            fprintf(f, "%ws: %s\n", filePath.c_str(), errStr.c_str());
            fclose(f);
        }
        
        assert(false && "Shader Compile Error");
    }

    // 5. コンパイル済みバイナリの取得
    Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob;
    hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
    assert(SUCCEEDED(hr));

    return shaderBlob;
}

/**
 * @brief ファイル名からプロファイルを推論する
 */
std::wstring ShaderCompiler::GetInferredProfile(const std::wstring& filePath) {
    if (filePath.find(L".VS.") != std::wstring::npos) return L"vs_6_0";
    if (filePath.find(L".PS.") != std::wstring::npos) return L"ps_6_0";
    if (filePath.find(L".GS.") != std::wstring::npos) return L"gs_6_0";
    if (filePath.find(L".CS.") != std::wstring::npos) return L"cs_6_0";
    if (filePath.find(L".DS.") != std::wstring::npos) return L"ds_6_0";
    if (filePath.find(L".HS.") != std::wstring::npos) return L"hs_6_0";
    
    // デフォルト
    return L"ps_6_0";
}

