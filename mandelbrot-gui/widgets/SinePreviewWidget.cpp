#include "SinePreviewWidget.h"

#include <algorithm>

#include <QMouseEvent>
#include <QPainter>
#include <QPen>

SinePreviewWidget::SinePreviewWidget(QWidget *parent)
    : QWidget(parent) {
    setMouseTracking(true);
}

std::pair<double, double> SinePreviewWidget::range() const {
    return { _rangeMin, _rangeMax };
}

void SinePreviewWidget::setPreviewImage(const QImage &image) {
    _previewImage = image;
    update();
}

void SinePreviewWidget::resetRange(double minValue, double maxValue) {
    _rangeMin = minValue;
    _rangeMax = std::max(minValue + minSpan, maxValue);
    emit rangeChanged(_rangeMin, _rangeMax);
    update();
}

void SinePreviewWidget::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.fillRect(rect(), palette().window());

    const QRect strip = _previewRect();
    if (strip.width() <= 0 || strip.height() <= 0) return;
    const bool widgetEnabled = isEnabled();

    if (_previewImage.isNull()) {
        painter.fillRect(strip, palette().base());
    } else if (widgetEnabled) {
        painter.drawImage(strip, _previewImage);
    } else {
        const QImage disabledImage
            = _previewImage.convertToFormat(QImage::Format_Grayscale8)
            .convertToFormat(QImage::Format_RGB32);
        painter.drawImage(strip, disabledImage);
        painter.fillRect(strip, QColor(255, 255, 255, 90));
    }

    painter.setPen(
        widgetEnabled ? palette().dark().color() : palette().mid().color());
    painter.drawRect(strip.adjusted(0, 0, -1, -1));

    painter.setPen(QPen(
        widgetEnabled ? QColor(255, 255, 255, 180) : palette().mid().color(),
        1.0));
    painter.drawLine(strip.left(), strip.top(), strip.left(), strip.bottom());
    painter.drawLine(strip.right(), strip.top(), strip.right(), strip.bottom());

    _drawHandle(painter, strip.left(), strip);
    _drawHandle(painter, strip.right(), strip);

    painter.setPen(
        widgetEnabled ? palette().text().color() : palette().mid().color());
    painter.drawText(rect().adjusted(2, strip.bottom() + 3, -2, -2),
        Qt::AlignLeft | Qt::AlignVCenter,
        tr("Range %1 - %2")
        .arg(QString::number(_rangeMin, 'f', 2))
        .arg(QString::number(_rangeMax, 'f', 2)));
}

void SinePreviewWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::RightButton) {
        const QRect strip = _previewRect();
        if (strip.contains(event->position().toPoint())) {
            resetRange(defaultMin, defaultMax);
        }
        return;
    }

    if (event->button() != Qt::LeftButton) return;

    const QRect strip = _previewRect();
    if (!strip.contains(event->position().toPoint())) return;

    _dragMode = _hitTest(event->position().toPoint(), strip);
    _dragStartX = event->position().x();
    _dragRangeMin = _rangeMin;
    _dragRangeMax = _rangeMax;
    _updateCursor(_dragMode);
}

void SinePreviewWidget::mouseMoveEvent(QMouseEvent *event) {
    const QRect strip = _previewRect();
    if (_dragMode == DragMode::none) {
        _updateCursor(_hitTest(event->position().toPoint(), strip));
        return;
    }
    if (strip.width() <= 0) return;

    const double span = std::max(minSpan, _dragRangeMax - _dragRangeMin);
    const double deltaValue
        = (_dragStartX - event->position().x()) / strip.width() * span;

    switch (_dragMode) {
        case DragMode::pan:
            _applyRange(_dragRangeMin + deltaValue, _dragRangeMax + deltaValue);
            break;
        case DragMode::left:
            _applyRange(
                std::min(_dragRangeMin + deltaValue, _dragRangeMax - minSpan),
                _dragRangeMax);
            break;
        case DragMode::right:
            _applyRange(_dragRangeMin,
                std::max(_dragRangeMin + minSpan, _dragRangeMax + deltaValue));
            break;
        case DragMode::none:
            break;
    }
}

void SinePreviewWidget::mouseReleaseEvent(QMouseEvent *) {
    _dragMode = DragMode::none;
    _updateCursor(DragMode::none);
}

void SinePreviewWidget::leaveEvent(QEvent *) {
    if (_dragMode == DragMode::none) {
        unsetCursor();
    }
}

QRect SinePreviewWidget::_previewRect() const {
    return rect().adjusted(2, 2, -2, -16);
}

SinePreviewWidget::DragMode SinePreviewWidget::_hitTest(
    const QPoint &point, const QRect &strip) const {
    if (!strip.contains(point)) return DragMode::none;
    if (std::abs(point.x() - strip.left()) <= handleHitWidth)
        return DragMode::left;
    if (std::abs(point.x() - strip.right()) <= handleHitWidth)
        return DragMode::right;
    return DragMode::pan;
}

void SinePreviewWidget::_drawHandle(
    QPainter &painter, int x, const QRect &strip) const {
    const QRect handle(
        x - 2, strip.top() + 4, 4, std::max(6, strip.height() - 8));
    painter.fillRect(handle,
        isEnabled() ? QColor(255, 255, 255, 180) : palette().mid().color());
}

void SinePreviewWidget::_updateCursor(DragMode mode) {
    switch (mode) {
        case DragMode::left:
        case DragMode::right:
            setCursor(Qt::SizeHorCursor);
            break;
        case DragMode::pan:
            setCursor(Qt::OpenHandCursor);
            break;
        case DragMode::none:
            unsetCursor();
            break;
    }
}

void SinePreviewWidget::_applyRange(double minValue, double maxValue) {
    const double clampedMin = minValue;
    const double clampedMax = std::max(clampedMin + minSpan, maxValue);
    if (qFuzzyCompare(_rangeMin, clampedMin)
        && qFuzzyCompare(_rangeMax, clampedMax)) {
        return;
    }

    _rangeMin = clampedMin;
    _rangeMax = clampedMax;
    emit rangeChanged(_rangeMin, _rangeMax);
    update();
}
