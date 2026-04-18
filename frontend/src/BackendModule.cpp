#include "BackendModule.h"

#include <utility>

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

std::filesystem::path executableDir() {
    std::wstring path(MAX_PATH, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, path.data(),
        static_cast<DWORD>(path.size()));

    path.resize(length);
    return std::filesystem::path(path).parent_path();
}

BackendModule loadBackendModule(const std::filesystem::path &exeDir,
    std::string_view configName, std::string &error) {
    const std::filesystem::path dllPath = exeDir /
        (std::string(configName) + ".dll");

    SetDllDirectoryW(exeDir.c_str());

    HMODULE module = LoadLibraryW(dllPath.c_str());
    if (!module) {
        error = "Failed to load core DLL: " + dllPath.string();
        return {};
    }

    const auto createSession = reinterpret_cast<CreateBackendFn>(
        GetProcAddress(module, "mandelbrot_backend_create"));
    const auto destroySession = reinterpret_cast<DestroyBackendFn>(
        GetProcAddress(module, "mandelbrot_backend_destroy"));

    if (!createSession || !destroySession) {
        error = "Core DLL is missing backend factory exports.";
        FreeLibrary(module);
        return {};
    }

    SessionPtr session(createSession(), SessionDeleter{ destroySession });
    if (!session) {
        error = "Failed to create backend session.";
        FreeLibrary(module);
        return {};
    }

    error.clear();
    return BackendModule(module, std::move(session));
}
