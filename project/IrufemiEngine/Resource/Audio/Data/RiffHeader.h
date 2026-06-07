#pragma once

#include "ChunkHeader.h"

//RIFFヘッダチャンク
struct RiffHeader {
    ChunkHeader chunk;
    char type[4];
};