#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

#include "BackendApi.h"

using CreateBackendFn = Backend::Session *(__cdecl *)();
using DestroyBackendFn = void(__cdecl *)(Backend::Session *);

struct SessionDeleter {
    DestroyBackendFn destroy = nullptr;

    void operator()(Backend::Session *session) const {
        if (destroy && session) destroy(session);
    }
};

using SessionPtr = std::unique_ptr<Backend::Session, SessionDeleter>;

struct BackendModule {
    HMODULE module = nullptr;
    SessionPtr session;

    BackendModule() = default;
    BackendModule(HMODULE module, SessionPtr session);

    BackendModule(const BackendModule &) = delete;
    BackendModule &operator=(const BackendModule &) = delete;

    BackendModule(BackendModule &&other) noexcept;
    BackendModule &operator=(BackendModule &&other) noexcept;

    ~BackendModule() {  reset(); }

    explicit operator bool() const { return module && session; }

    void reset();
};

std::filesystem::path executableDir();
BackendModule loadBackendModule(const std::filesystem::path &exeDir,
    std::string_view configName, std::string &error);
