#include "ViewportStatusOverlayWidget.h"

#include <algorithm>

#include <QFontMetrics>
#include <QPaintEvent>
#include <QPainter>

ViewportStatusOverlayWidget::ViewportStatusOverlayWidget(QWidget *parent)
    : QWidget(parent) {
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAutoFillBackground(false);
    setFocusPolicy(Qt::NoFocus);
}

void ViewportStatusOverlayWidget::setHost(ViewportHost *host) {
    _host = host;
    update();
}

void ViewportStatusOverlayWidget::paintEvent(QPaintEvent *event) {
    QWidget::paintEvent(event);
    if (!_host) return;

    const QString statusText = _host->viewportStatusText();
    if (statusText.isEmpty()) {
        return;
    }

    constexpr int overlayPaddingX = 8;
    constexpr int overlayPaddingY = 6;
    const QRect overlayRect = _statusOverlayRect(statusText);
    if (!overlayRect.isValid()) {
        return;
    }

    QPainter painter(this);
    if (event) painter.setClipRegion(event->region());
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(80, 80, 80, 150));
    painter.drawRoundedRect(overlayRect, 8.0, 8.0);

    painter.setPen(QColor(230, 230, 230));
    painter.drawText(overlayRect.adjusted(overlayPaddingX, overlayPaddingY,
        -overlayPaddingX, -overlayPaddingY),
        Qt::AlignTop | Qt::AlignLeft, statusText);
}

QRect ViewportStatusOverlayWidget::_statusOverlayRect(
    const QString &statusText
) const {
    constexpr int overlayMargin = 8;
    constexpr int overlayPaddingX = 8;
    constexpr int overlayPaddingY = 6;
    const QFontMetrics metrics(font());
    const QString maxPrecisionMouseLine = "Mouse: 999999, 999999  |  "
        "-1.7976931348623157e+308  "
        "-1.7976931348623157e+308";
    int overlayTextWidth = metrics.horizontalAdvance(maxPrecisionMouseLine);
    for (const QString &line : statusText.split('\n')) {
        overlayTextWidth
            = std::max(overlayTextWidth, metrics.horizontalAdvance(line));
    }

    const int overlayWidth = std::min(std::max(1, width() - overlayMargin * 2),
        overlayTextWidth + overlayPaddingX * 2 + 2);
    const QRect textRect(overlayMargin + overlayPaddingX,
        overlayMargin + overlayPaddingY,
        std::max(1, overlayWidth - overlayPaddingX * 2),
        std::max(1, height() - (overlayMargin + overlayPaddingY) * 2));
    const QRect textBounds = metrics.boundingRect(textRect,
        Qt::AlignTop | Qt::AlignLeft, statusText);
    return QRect(overlayMargin, overlayMargin, overlayWidth,
        textBounds.height() + overlayPaddingY * 2);
}
