#include <QStyleFactory>
#include <QCoreApplication>
#include <QIcon>
#include <QtGui/QGuiApplication>
#include <QtWidgets/QApplication>

#include "AppVersion.h"

#include "app/GUIAppController.h"
#include "locale/GUILocale.h"
#include "settings/AppSettings.h"

int main(int argc, char *argv[]) {
    QCoreApplication::setApplicationName(QStringLiteral("Mandelbrot GUI"));
    QCoreApplication::setApplicationVersion(
        QString::fromStdString(GUI::Constants::appVersion)
    );
    
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::Round
    );

    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(true);

    if (QStyleFactory::keys().contains("windowsvista")) {
        app.setStyle(QStyleFactory::create("windowsvista"));
    }

    app.setWindowIcon(QIcon(":/mandelbrot-gui/mandelbrot.ico"));

    AppSettings settings;
    GUILocale locale(app);
    locale.setLanguage(settings.language());

    auto *controller = new GUIAppController(locale, settings);
    if (!controller->initialize()) return 1;

    controller->show();
    return app.exec();
}