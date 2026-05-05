#pragma once

#include <cstdint>

struct D3DPresentationSurface {
    void *device = nullptr;
    void *deviceContext = nullptr;
    void *texture = nullptr;
    int32_t width = 0;
    int32_t height = 0;

    [[nodiscard]] bool valid() const {
        return device != nullptr
            && deviceContext != nullptr
            && texture != nullptr
            && width > 0
            && height > 0;
    }
};
