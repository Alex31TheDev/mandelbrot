#pragma once

#include "BackendAPI.h"

class CallbackFormatter {
public:
    void bind(Backend::Session &session) const;
};
