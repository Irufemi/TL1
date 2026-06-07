#pragma once
#include <xaudio2.h>
#include <memory>

//音声データ
struct SoundData {
	//波形フォーマット
	WAVEFORMATEX wfex;
	//バッファの先頭アドレス
	std::unique_ptr<BYTE[]> pBuffer;
	//バッファのサイズ
	unsigned int bufferSize;
};