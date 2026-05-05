#include "ViewportZoomOverlayWidget.h"

#include <algorithm>
#include <cmath>

#include <QPaintEvent>
#include <QPainter>
#include <QPen>

ViewportZoomOverlayWidget::ViewportZoomOverlayWidget(QWidget *parent)
    : QWidget(parent) {
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAutoFillBackground(false);
    setFocusPolicy(Qt::NoFocus);
}

void ViewportZoomOverlayWidget::beginSelection(const QPoint &origin) {
    _selectionOrigin = origin;
    _setSelectionRect(QRect(origin, QSize(1, 1)));
}

void ViewportZoomOverlayWidget::updateSelection(const QPoint &current) {
    _setSelectionRect(QRect(_selectionOrigin, current));
}

bool ViewportZoomOverlayWidget::scaleSelection(
    double factor, const QRect &bounds
) {
    const QRect current = _selectionRect.normalized();
    if (current.width() < 2 || current.height() < 2) {
        return false;
    }
    if (!bounds.isValid()) {
        return false;
    }

    const QPointF center = current.center();
    const double maxWidth = std::max(2.0, static_cast<double>(bounds.width() - 2));
    const double maxHeight
        = std::max(2.0, static_cast<double>(bounds.height() - 2));
    const double nextWidth
        = std::clamp(current.width() * factor, 2.0, maxWidth);
    const double nextHeight
        = std::clamp(current.height() * factor, 2.0, maxHeight);
    QRect scaled(
        static_cast<int>(std::lround(center.x() - nextWidth / 2.0)),
        static_cast<int>(std::lround(center.y() - nextHeight / 2.0)),
        std::max(2, static_cast<int>(std::lround(nextWidth))),
        std::max(2, static_cast<int>(std::lround(nextHeight)))
    );
    scaled = scaled.intersected(bounds);
    if (scaled.width() < 2 || scaled.height() < 2) {
        return false;
    }

    _selectionOrigin = scaled.topLeft();
    _setSelectionRect(QRect(_selectionOrigin, scaled.bottomRight()));
    return true;
}

void ViewportZoomOverlayWidget::clearSelection() {
    _setSelectionRect({});
}

void ViewportZoomOverlayWidget::paintEvent(QPaintEvent *event) {
    QWidget::paintEvent(event);
    if (_selectionRect.isNull()) {
        return;
    }

    QPainter painter(this);
    if (event) painter.setClipRegion(event->region());
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(255, 255, 255, 180), 1.0, Qt::DashLine));
    painter.drawRect(_selectionRect.normalized());
}

void ViewportZoomOverlayWidget::_setSelectionRect(const QRect &rect) {
    if (_selectionRect == rect) return;
    _selectionRect = rect;
    update();
}
