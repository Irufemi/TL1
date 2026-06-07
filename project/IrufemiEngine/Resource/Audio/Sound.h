#pragma once
#include <vector>
#include <string>
#include <xaudio2.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")

class Sound {
public:
    Sound() = default;
    ~Sound();

    // ファイルから音声データを読み込む
    bool Load(const std::wstring& filePath);

    // WAVEFORMATEXへのポインタを取得
    const WAVEFORMATEX* GetFormat() const { return pWaveFormat; }

    // オーディオデータへのポインタを取得
    const BYTE* GetData() const { return mediaData.data(); }

    // オーディオデータのサイズを取得
    UINT32 GetSize() const { return static_cast<UINT32>(mediaData.size()); }

private:
    WAVEFORMATEX* pWaveFormat{ nullptr };
    std::vector<BYTE> mediaData;
};