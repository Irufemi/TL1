
#pragma once

// This file needs to be included first for all MSDFgen sources

#define _CRT_SECURE_NO_WARNINGS
#define MSDFGEN_PUBLIC

#ifndef MSDFGEN_PUBLIC
#include <msdfgen/msdfgen-config.h>
#endif

#include <cstddef>

namespace msdfgen {

typedef unsigned char byte;

}
