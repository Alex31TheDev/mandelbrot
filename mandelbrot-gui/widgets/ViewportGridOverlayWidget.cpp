#include "ViewportGridOverlayWidget.h"

#include <cmath>

#include <QPaintEvent>
#include <QPainter>
#include <QPen>

ViewportGridOverlayWidget::ViewportGridOverlayWidget(QWidget *parent)
    : QWidget(parent) {
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAutoFillBackground(false);
    setFocusPolicy(Qt::NoFocus);
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

    QPainter painter(this);
    if (event) painter.setClipRegion(event->region());
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(QPen(QColor(255, 255, 255, 110), 1.0));

    for (int i = 1; i < _gridDivisions; i++) {
        const int x = static_cast<int>(std::lround(
            static_cast<double>(area.width()) * i / _gridDivisions
        ));
        const int y = static_cast<int>(std::lround(
            static_cast<double>(area.height()) * i / _gridDivisions
        ));
        painter.drawLine(x, area.top(), x, area.bottom());
        painter.drawLine(area.left(), y, area.right(), y);
    }
}
