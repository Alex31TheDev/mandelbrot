#include "BackendModule.h"

#include <string>
#include <filesystem>
#include <utility>

#include "BackendAPI.h"

BackendModule::BackendModule(HMODULE module,
    CreateBackendFunc createSession, DestroyBackendFunc destroySession)
    : _module(module),
    _createSession(createSession), _destroySession(destroySession) {}

BackendModule::BackendModule(BackendModule &&other) noexcept
    : _module(other._module), _sessions(std::move(other._sessions)),
    _createSession(other._createSession), _destroySession(other._destroySession) {
    other._module = nullptr;
    other._createSession = nullptr;
    other._destroySession = nullptr;
}

BackendModule &BackendModule::operator=(BackendModule &&other) noexcept {
    if (this != &other) {
        reset();

        _module = other._module;
        _sessions = std::move(other._sessions);
        _createSession = other._createSession;
        _destroySession = other._destroySession;

        other._module = nullptr;
        other._createSession = nullptr;
        other._destroySession = nullptr;
    }

    return *this;
}

Backend::Session *BackendModule::makeSession() {
    if (!_createSession || !_destroySession) return nullptr;

    SessionPtr session(_createSession(), SessionDeleter{ _destroySession });
    if (!session) return nullptr;

    Backend::Session *rawSession = session.get();
    _sessions.push_back(std::move(session));
    return rawSession;
}

void BackendModule::reset() {
    _sessions.clear();
    _createSession = nullptr;
    _destroySession = nullptr;

    if (_module) {
        FreeLibrary(_module);
        _module = nullptr;
    }
}

void BackendModule::forceKill() const {
    for (const SessionPtr &session : _sessions) {
        if (session) session->forceKill();
    }
}

BackendModule loadBackendModule(const std::filesystem::path &exeDir,
    const std::string &configName, std::string &err) {
    const std::filesystem::path dllPath =
        (exeDir / "backends") / (configName + ".dll");
    HMODULE module = LoadLibraryExW(dllPath.c_str(), nullptr,
        LOAD_WITH_ALTERED_SEARCH_PATH);

    if (!module) {
        err = "Failed to load backend DLL: " + dllPath.string();
        return {};
    }

    const auto createSession = reinterpret_cast<CreateBackendFunc>(
        GetProcAddress(module, "mandelbrot_backend_create")
        );
    const auto destroySession = reinterpret_cast<DestroyBackendFunc>(
        GetProcAddress(module, "mandelbrot_backend_destroy")
        );

    if (!createSession || !destroySession) {
        err = "Backend DLL is missing backend factory exports.";
        FreeLibrary(module);
        return {};
    }

    err.clear();
    return BackendModule(module, createSession, destroySession);
}