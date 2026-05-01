#pragma once

#include <optional>

#include "image/Image.h"

#include "RenderIterationStats.h"

#include "BackendAPI.h"

void renderImage(Image *image,
    const Backend::Callbacks *callbacks = nullptr,
    bool trackProgress = true, bool trackIterations = false,
    OptionalIterationStats iterStats = std::nullopt);
