#pragma once

#include "../image/Image.h"
#include "BackendApi.h"

void renderImage(Image *image,
    const Backend::Callbacks *callbacks = nullptr,
    bool trackProgress = true, bool trackIterations = false);
