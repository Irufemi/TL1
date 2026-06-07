#include "Sound.h"
#include <cassert>

Sound::~Sound() {
    if (pWaveFormat) {
        CoTaskMemFree(pWaveFormat);
        pWaveFormat = nullptr;
    }
}

bool Sound::Load(const std::wstring& filePath) {
    HRESULT hr;

    // ソースリーダーの作成
    IMFSourceReader* pMFSourceReader{ nullptr };
    hr = MFCreateSourceReaderFromURL(filePath.c_str(), NULL, &pMFSourceReader);
    if (FAILED(hr)) return false;

    // メディアタイプをPCMに設定
    IMFMediaType* pMFMediaType{ nullptr };
    MFCreateMediaType(&pMFMediaType);
    pMFMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    pMFMediaType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    pMFSourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pMFMediaType);

    pMFMediaType->Release();
    pMFMediaType = nullptr;
    pMFSourceReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &pMFMediaType);

    // オーディオデータ形式の取得
    MFCreateWaveFormatExFromMFMediaType(pMFMediaType, &pWaveFormat, nullptr);
    pMFMediaType->Release();

    // データの読み込み
    mediaData.clear();
    while (true) {
        IMFSample* pMFSample{ nullptr };
        DWORD dwStreamFlags{ 0 };
        pMFSourceReader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &dwStreamFlags, nullptr, &pMFSample);

        if (dwStreamFlags & MF_SOURCE_READERF_ENDOFSTREAM) {
            if (pMFSample) pMFSample->Release();
            break;
        }

        IMFMediaBuffer* pMFMediaBuffer{ nullptr };
        pMFSample->ConvertToContiguousBuffer(&pMFMediaBuffer);

        BYTE* pBuffer{ nullptr };
        DWORD cbCurrentLength{ 0 };
        pMFMediaBuffer->Lock(&pBuffer, nullptr, &cbCurrentLength);

        size_t currentSize = mediaData.size();
        mediaData.resize(currentSize + cbCurrentLength);
        memcpy(mediaData.data() + currentSize, pBuffer, cbCurrentLength);

        pMFMediaBuffer->Unlock();
        pMFMediaBuffer->Release();
        pMFSample->Release();
    }

    pMFSourceReader->Release();
    return true;
}