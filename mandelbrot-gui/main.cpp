#include "mandelbrotgui.h"
#include <QIcon>
#include <QtGui/QGuiApplication>
#include <QtWidgets/QApplication>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::Round);
    QApplication app(argc, argv);
    if (QStyleFactory::keys().contains("windowsvista")) {
        app.setStyle(QStyleFactory::create("windowsvista"));
    }
    app.setWindowIcon(QIcon(":/mandelbrotgui/mandelbrot.ico"));
    mandelbrotgui window;
    window.show();
    return app.exec();
}
