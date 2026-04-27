#pragma once

#include <utility>

#include <QImage>
#include <QWidget>

class SinePreviewWidget final : public QWidget {
    Q_OBJECT

public:
    explicit SinePreviewWidget(QWidget *parent = nullptr);

    [[nodiscard]] std::pair<double, double> range() const;
    void setPreviewImage(const QImage &image);
    void resetRange(double minValue, double maxValue);

signals:
    void rangeChanged(double minValue, double maxValue);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    enum class DragMode { none, left, right, pan };

    static constexpr double kDefaultMin = 0.0;
    static constexpr double kDefaultMax = 100.0;
    static constexpr double kMinSpan = 1.0;
    static constexpr int kHandleHitWidth = 10;

    double _rangeMin = kDefaultMin;
    double _rangeMax = kDefaultMax;
    double _dragStartX = 0.0;
    double _dragRangeMin = kDefaultMin;
    double _dragRangeMax = kDefaultMax;
    DragMode _dragMode = DragMode::none;
    QImage _previewImage;

    [[nodiscard]] QRect _previewRect() const;
    [[nodiscard]] DragMode _hitTest(
        const QPoint &point, const QRect &strip) const;
    void _drawHandle(QPainter &painter, int x, const QRect &strip) const;
    void _updateCursor(DragMode mode);
    void _applyRange(double minValue, double maxValue);
};
