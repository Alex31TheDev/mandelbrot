#include "settings/AppSettings.h"

#include <algorithm>
#include <filesystem>

#include "app/GUIConstants.h"
#include "util/PathUtil.h"

static QString settingsPath() {
    const std::filesystem::path path
        = PathUtil::executableDir() / "settings.ini";
    return QString::fromStdWString(path.wstring());
}

AppSettings::AppSettings()
    : _settings(settingsPath(), QSettings::IniFormat) {}

QString AppSettings::language() const {
    return _settings.value("ui/language", "en").toString();
}

void AppSettings::setLanguage(const QString &language) {
    _settings.setValue("ui/language", language);
}

QString AppSettings::shortcut(
    const QString &id, const QString &fallback) const {
    return _settings.value(QString("shortcuts/%1").arg(id), fallback)
        .toString();
}

void AppSettings::setShortcut(const QString &id, const QString &sequence) {
    _settings.setValue(QString("shortcuts/%1").arg(id), sequence);
}

QByteArray AppSettings::controlWindowGeometry() const {
    return _settings
        .value("window/controlGeometry", _settings.value("window/mainGeometry"))
        .toByteArray();
}

void AppSettings::setControlWindowGeometry(const QByteArray &geometry) {
    _settings.setValue("window/controlGeometry", geometry);
}

QByteArray AppSettings::controlWindowState() const {
    return _settings
        .value("window/controlState", _settings.value("window/mainState"))
        .toByteArray();
}

void AppSettings::setControlWindowState(const QByteArray &state) {
    _settings.setValue("window/controlState", state);
}

QString AppSettings::preferredBackend() const {
    return _settings.value("render/backend").toString();
}

void AppSettings::setPreferredBackend(const QString &backend) {
    _settings.setValue("render/backend", backend);
}

int AppSettings::previewFallbackFPS() const {
    return _settings.value(
        "render/previewFallbackFPS", GUI::Constants::defaultInteractionTargetFPS)
        .toInt();
}

void AppSettings::setPreviewFallbackFPS(int fps) {
    _settings.setValue("render/previewFallbackFPS", std::max(0, fps));
}

int AppSettings::selectedOutputWidth() const {
    return std::max(
        1,
        _settings
            .value("render/outputWidth", GUI::Constants::defaultOutputWidth)
            .toInt());
}

void AppSettings::setSelectedOutputWidth(int width) {
    _settings.setValue("render/outputWidth", std::max(1, width));
}

int AppSettings::selectedOutputHeight() const {
    return std::max(1,
        _settings
            .value("render/outputHeight", GUI::Constants::defaultOutputHeight)
            .toInt());
}

void AppSettings::setSelectedOutputHeight(int height) {
    _settings.setValue("render/outputHeight", std::max(1, height));
}

void AppSettings::sync() {
    _settings.sync();
}

void AppSettings::clearPersistedSettings() {
    _settings.clear();
    _settings.sync();
}
