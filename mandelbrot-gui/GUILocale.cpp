#include "GUILocale.h"

#include <filesystem>

#include <QApplication>

#include "util/PathUtil.h"

GUILocale::GUILocale(QApplication &app)
    : _app(app) {}

bool GUILocale::setLanguage(const QString &language) {
    if (_translator) {
        _app.removeTranslator(_translator.get());
        _translator.reset();
    }

    _language = language.isEmpty() ? "en" : language;
    if (_language == "en") return true;

    auto translator = std::make_unique<QTranslator>();
    const std::filesystem::path qmPath = PathUtil::executableDir()
        / "translations"
        / std::filesystem::path(
            ("mandelbrot_" + _language + ".qm").toStdString());
    if (!translator->load(QString::fromStdWString(qmPath.wstring()))) {
        _translator.reset();
        return false;
    }

    _app.installTranslator(translator.get());
    _translator = std::move(translator);
    return true;
}