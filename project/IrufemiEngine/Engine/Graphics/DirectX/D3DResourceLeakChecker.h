#pragma once

#include <wrl.h>
#include <dxgidebug.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#pragma comment(lib,"dxguid.lib")

struct D3DResourceLeakChecker {
    ~D3DResourceLeakChecker() {
        // 現在、リークレポートは DirectXCommon::Finalize() 内で
        // デバイス破棄の直前に行われるように統合されました。
        // ここでの処理は二重レポートによる不安定さを避けるため空にしています。
    }
};
