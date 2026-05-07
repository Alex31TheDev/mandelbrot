#pragma once

#include <memory>

#include <QPoint>
#include <QRect>
#include <QResizeEvent>
#include <QWidget>

#include "windows/viewport/ViewportHost.h"

class ViewportGridOverlayWidget;
class ViewportStatusOverlayWidget;
class ViewportZoomOverlayWidget;

namespace Ui {
    class ViewportOverlayWidget;
}

class ViewportOverlayWidget final : public QWidget {
public:
    explicit ViewportOverlayWidget(QWidget *parent = nullptr);
    ~ViewportOverlayWidget() override;

    void setHost(ViewportHost *host);
    void refreshOverlay();
    void cycleGridMode();
    void toggleMinimalUI();
    [[nodiscard]] bool minimalUIEnabled() const { return _minimalUI; }

    void beginZoomSelection(const QPoint &origin);
    void updateZoomSelection(const QPoint &current);
    bool scaleZoomSelection(double factor);
    void clearZoomSelection();
    [[nodiscard]] QRect zoomSelectionRect() const;
    [[nodiscard]] bool hasZoomSelection() const;

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    std::unique_ptr<Ui::ViewportOverlayWidget> _ui;
    ViewportHost *_host = nullptr;
    ViewportGridOverlayWidget *_gridWidget = nullptr;
    ViewportStatusOverlayWidget *_statusWidget = nullptr;
    ViewportZoomOverlayWidget *_zoomWidget = nullptr;
    bool _minimalUI = false;

    void _syncChildGeometry();
};
