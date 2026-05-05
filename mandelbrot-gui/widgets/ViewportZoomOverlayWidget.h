#pragma once

#include <QPoint>
#include <QRect>
#include <QWidget>

class ViewportZoomOverlayWidget final : public QWidget {
public:
    explicit ViewportZoomOverlayWidget(QWidget *parent = nullptr);

    void beginSelection(const QPoint &origin);
    void updateSelection(const QPoint &current);
    bool scaleSelection(double factor, const QRect &bounds);
    void clearSelection();

    [[nodiscard]] QRect selectionRect() const { return _selectionRect; }
    [[nodiscard]] bool hasSelection() const {
        return !_selectionRect.normalized().isNull();
    }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QPoint _selectionOrigin;
    QRect _selectionRect;

    void _setSelectionRect(const QRect &rect);
};
