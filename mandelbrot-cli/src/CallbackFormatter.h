#pragma once

#include "BackendApi.h"

class CallbackFormatter {
public:
    void bind(Backend::Session &session) const;
};
