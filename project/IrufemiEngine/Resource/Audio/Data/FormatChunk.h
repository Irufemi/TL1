#pragma once

#include "ChunkHeader.h"
#include <xaudio2.h>

struct FormatChunk {
    ChunkHeader chunk;
    WAVEFORMATEX fmt;
};