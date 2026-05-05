#pragma once

#include <memory>

#include <QRect>
#include <QResizeEvent>
#include <QWidget>

#include "ui_ViewportOverlayWindow.h"

#include "widgets/ViewportGridOverlayWidget.h"
#include "widgets/ViewportStatusOverlayWidget.h"
#include "widgets/ViewportZoomOverlayWidget.h"

#include "ViewportHost.h"

class ViewportOverlayWindow final : public QWidget {
public:
    explicit ViewportOverlayWindow(
        ViewportHost *host, QWidget *parent = nullptr
    );
    ~ViewportOverlayWindow() override;

    void syncToViewportRect(const QRect &globalRect, bool visible);
    void refreshOverlay();
    void raiseAboveViewport();
    void cycleGridMode();
    void toggleMinimalUI();
    [[nodiscard]] bool minimalUIEnabled() const { return _minimalUI; }

    void beginZoomSelection(const QPoint &origin);
    void updateZoomSelection(const QPoint &current);
    bool scaleZoomSelection(double factor);
    void clearZoomSelection();
    [[nodiscard]] QRect zoomSelectionRect() const;

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    std::unique_ptr<Ui::ViewportOverlayWindow> _ui;
    ViewportHost *_host = nullptr;
    ViewportGridOverlayWidget *_gridWidget = nullptr;
    ViewportStatusOverlayWidget *_statusWidget = nullptr;
    ViewportZoomOverlayWidget *_zoomWidget = nullptr;
    QRect _viewportRect;
    bool _viewportVisible = false;
    bool _minimalUI = false;

    void _syncChildGeometry();
    void _applyWindowVisibility();
};
