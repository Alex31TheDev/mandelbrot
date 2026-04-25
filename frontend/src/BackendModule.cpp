#include "BackendModule.h"

#include <string>
#include <filesystem>
#include <utility>

#include "BackendAPI.h"

BackendModule::BackendModule(HMODULE moduleHandle, SessionPtr sessionPtr)
    : module(moduleHandle), session(std::move(sessionPtr)) {}

BackendModule::BackendModule(BackendModule &&other) noexcept
    : module(other.module), session(std::move(other.session)) {
    other.module = nullptr;
}

BackendModule &BackendModule::operator=(BackendModule &&other) noexcept {
    if (this != &other) {
        reset();
        module = other.module;
        session = std::move(other.session);
        other.module = nullptr;
    }

    return *this;
}

void BackendModule::reset() {
    session.reset();

    if (module) {
        FreeLibrary(module);
        module = nullptr;
    }
}

void BackendModule::forceKill() const {
    if (session) session->forceKill();
}

std::filesystem::path executableDir() {
    std::wstring path(MAX_PATH, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, path.data(),
        static_cast<DWORD>(path.size()));

    path.resize(length);
    return std::filesystem::path(path).parent_path();
}

BackendModule loadBackendModule(const std::filesystem::path &exeDir,
    const std::string &configName, std::string &err) {
    const std::filesystem::path dllPath = (exeDir / "backends") / (configName + ".dll");
    HMODULE module = LoadLibraryExW(dllPath.c_str(), nullptr,
        LOAD_WITH_ALTERED_SEARCH_PATH);

    if (!module) {
        err = "Failed to load backend DLL: " + dllPath.string();
        return {};
    }

    const auto createSession = reinterpret_cast<CreateBackendFunc>(
        GetProcAddress(module, "mandelbrot_backend_create"));
    const auto destroySession = reinterpret_cast<DestroyBackendFunc>(
        GetProcAddress(module, "mandelbrot_backend_destroy"));

    if (!createSession || !destroySession) {
        err = "Backend DLL is missing backend factory exports.";
        FreeLibrary(module);
        return {};
    }

    SessionPtr session(createSession(), SessionDeleter{ destroySession });
    if (!session) {
        err = "Failed to create backend session.";
        FreeLibrary(module);
        return {};
    }

    err.clear();
    return BackendModule(module, std::move(session));
}