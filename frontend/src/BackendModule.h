#pragma once

#include <filesystem>
#include <memory>
#include <string>

#include "util/IncludeWin32.h"

#include "BackendAPI.h"

typedef Backend::Session *(__cdecl *CreateBackendFunc)();
typedef void(__cdecl *DestroyBackendFunc)(Backend::Session *);

struct SessionDeleter {
    DestroyBackendFunc destroy = nullptr;

    void operator()(Backend::Session *session) const {
        if (destroy && session) destroy(session);
    }
};

typedef std::unique_ptr<Backend::Session, SessionDeleter> SessionPtr;

struct BackendModule {
    HMODULE module = nullptr;
    SessionPtr session;

    BackendModule() = default;
    BackendModule(HMODULE module, SessionPtr session);

    BackendModule(const BackendModule &) = delete;
    BackendModule &operator=(const BackendModule &) = delete;

    BackendModule(BackendModule &&other) noexcept;
    BackendModule &operator=(BackendModule &&other) noexcept;

    ~BackendModule() { reset(); }

    explicit operator bool() const { return module && session; }

    void reset();
    void forceKill() const;
};

std::filesystem::path executableDir();
BackendModule loadBackendModule(const std::filesystem::path &exeDir,
    const std::string &configName, std::string &err);