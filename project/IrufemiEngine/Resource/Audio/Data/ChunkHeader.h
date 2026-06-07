#pragma once

#include <cstdint>

struct ChunkHeader {
    //チャンク毎のID
    char id[4];
    //チャンクサイズ
    int32_t size;
};