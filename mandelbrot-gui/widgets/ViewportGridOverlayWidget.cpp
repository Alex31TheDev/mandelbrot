#include "ViewportGridOverlayWidget.h"

#include <cmath>

#include <QImage>
#include <QPainter>
#include <QPaintEvent>
#include <QPen>

#include "app/GUIConstants.h"

ViewportGridOverlayWidget::ViewportGridOverlayWidget(QWidget *parent)
    : QWidget(parent) {
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents);
}

void ViewportGridOverlayWidget::refreshOverlay(bool minimalUI) {
    const bool visible = !minimalUI && _gridDivisions > 1;
    setVisible(visible);
    if (visible) {
        update();
    }
}

void ViewportGridOverlayWidget::cycleMode() {
    int idx = 0;
    while (idx < static_cast<int>(GUI::Constants::gridModes.size())
        && GUI::Constants::gridModes[idx] != _gridDivisions) {
        idx++;
    }

    idx = (idx + 1) % static_cast<int>(GUI::Constants::gridModes.size());
    setGridDivisions(GUI::Constants::gridModes[idx]);
}

void ViewportGridOverlayWidget::setGridDivisions(int divisions) {
    if (_gridDivisions == divisions) return;
    _gridDivisions = divisions;
    update();
}

void ViewportGridOverlayWidget::paintEvent(QPaintEvent *event) {
    QWidget::paintEvent(event);
    if (_gridDivisions <= 1) return;

    const QRect area = rect();
    if (area.width() <= 1 || area.height() <= 1) return;

    const qreal devicePixelRatio = devicePixelRatioF();
    const QSize overlaySize(std::max(1, static_cast<int>(std::lround(
            width() * devicePixelRatio
        ))),
        std::max(1, static_cast<int>(std::lround(height() * devicePixelRatio))));
    QImage overlayImage(overlaySize,
        QImage::Format_ARGB32_Premultiplied);
    overlayImage.setDevicePixelRatio(devicePixelRatio);
    overlayImage.fill(Qt::transparent);

    QPainter overlayPainter(&overlayImage);
    overlayPainter.setRenderHint(QPainter::Antialiasing, false);
    overlayPainter.setPen(QPen(Qt::white, 1.0));

    for (int i = 1; i < _gridDivisions; i++) {
        const int x = static_cast<int>(std::lround(
            static_cast<double>(area.width()) * i / _gridDivisions
        ));
        const int y = static_cast<int>(std::lround(
            static_cast<double>(area.height()) * i / _gridDivisions
        ));
        overlayPainter.drawLine(x, area.top(), x, area.bottom());
        overlayPainter.drawLine(area.left(), y, area.right(), y);
    }

    QPainter painter(this);
    if (event) painter.setClipRegion(event->region());
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setCompositionMode(QPainter::CompositionMode_Difference);
    painter.drawImage(0, 0, overlayImage);
}
