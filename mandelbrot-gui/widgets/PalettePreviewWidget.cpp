#include "PalettePreviewWidget.h"

#include <QPainter>

PalettePreviewWidget::PalettePreviewWidget(QWidget *parent)
    : QWidget(parent) {}

void PalettePreviewWidget::setPreviewPixmap(const QPixmap &pixmap) {
    _previewPixmap = pixmap;
    update();
}

void PalettePreviewWidget::clearPreview() {
    _previewPixmap = {};
    update();
}

QSize PalettePreviewWidget::sizeHint() const {
    return { 0, 30 };
}

QSize PalettePreviewWidget::minimumSizeHint() const {
    return { 0, 30 };
}

void PalettePreviewWidget::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    const QRect frameRect = rect().adjusted(0, 0, -1, -1);
    painter.fillRect(rect(), palette().base());

    if (!_previewPixmap.isNull()) {
        const QPixmap scaled = _previewPixmap.scaled(frameRect.size(),
            Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        if (isEnabled()) {
            painter.drawPixmap(frameRect.topLeft(), scaled);
        } else {
            const QImage disabledImage
                = scaled.toImage()
                .convertToFormat(QImage::Format_Grayscale8)
                .convertToFormat(QImage::Format_RGB32);
            painter.drawImage(frameRect, disabledImage);
            painter.fillRect(frameRect, QColor(255, 255, 255, 90));
        }
    }

    painter.setPen(
        isEnabled() ? palette().dark().color() : palette().mid().color()
    );
    painter.drawRect(frameRect);
}