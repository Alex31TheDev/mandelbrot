#pragma once

#include "util/fnv1a.h"

#ifndef OUT_FILENAME
#define OUT_FILENAME "mandelbrot"
#endif
#ifndef OUT_FILETYPE
#define OUT_FILETYPE "png"
#endif

namespace _fullnameImpl {
    using namespace fnv1a;

    static_assert(fnv1a::hash_32(OUT_FILETYPE) == "png"_hash_32 ||
        fnv1a::hash_32(OUT_FILETYPE) == "jpg"_hash_32 ||
        fnv1a::hash_32(OUT_FILETYPE) == "bmp"_hash_32,
        "OUT_FILETYPE must be one of: \"png\", \"jpg\", \"bmp\"");
}

const char *fullname = OUT_FILENAME "." OUT_FILETYPE;