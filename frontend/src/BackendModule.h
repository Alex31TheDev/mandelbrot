#pragma once

#include <memory>
#include <string>
#include <vector>
#include <filesystem>

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

class BackendModule {
public:
    BackendModule() = default;
    BackendModule(HMODULE module,
        CreateBackendFunc createSession, DestroyBackendFunc destroySession);

    BackendModule(const BackendModule &) = delete;
    BackendModule &operator=(const BackendModule &) = delete;

    BackendModule(BackendModule &&other) noexcept;
    BackendModule &operator=(BackendModule &&other) noexcept;

    explicit operator bool() const { return _module; }

    ~BackendModule() { reset(); }

    Backend::Session *makeSession();
    void reset();
    void forceKill() const;

private:
    HMODULE _module = nullptr;
    std::vector<SessionPtr> _sessions;

    CreateBackendFunc _createSession = nullptr;
    DestroyBackendFunc _destroySession = nullptr;
};

BackendModule loadBackendModule(const std::filesystem::path &exeDir,
    const std::string &configName, std::string &err);
