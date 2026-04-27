#include "windows/control/ControlWindow.h"
#include "AppSettings.h"
#include "GUILocale.h"
#include <QIcon>
#include <QtGui/QGuiApplication>
#include <QtWidgets/QApplication>
#include <QStyleFactory>

int main(int argc, char *argv[]) {
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::Round);
    QApplication app(argc, argv);
    if (QStyleFactory::keys().contains("windowsvista")) {
        app.setStyle(QStyleFactory::create("windowsvista"));
    }
    AppSettings settings;
    GUILocale locale(app);
    locale.setLanguage(settings.language());
    app.setWindowIcon(QIcon(":/mandelbrotgui/mandelbrot.ico"));
    ControlWindow window(locale);
    window.show();
    return app.exec();
}