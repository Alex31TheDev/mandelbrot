#include "ViewportOverlayWindow.h"

#include "ui_ViewportOverlayWindow.h"

#include "util/IncludeWin32.h"

#include "app/GUIConstants.h"

#include "ViewportHost.h"

#include "widgets/ViewportGridOverlayWidget.h"
#include "widgets/ViewportStatusOverlayWidget.h"
#include "widgets/ViewportZoomOverlayWidget.h"

static void placeWindowAboveViewport(QWidget *overlay, QWidget *viewport) {
#ifdef _WIN32
    if (!overlay || !viewport) {
        return;
    }

    const HWND overlayHwnd = reinterpret_cast<HWND>(overlay->winId());
    if (!overlayHwnd || !viewport->winId()) {
        return;
    }

    ::SetWindowPos(overlayHwnd, HWND_TOP, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
#else
    (void)overlay;
    (void)viewport;
#endif
}

ViewportOverlayWindow::ViewportOverlayWindow(
    ViewportHost *host, QWidget *parent
)
    : QWidget(parent,
        Qt::Window | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint
            | Qt::WindowDoesNotAcceptFocus
            | Qt::WindowTransparentForInput)
    , _ui(std::make_unique<Ui::ViewportOverlayWindow>())
    , _host(host) {
    _ui->setupUi(this);
    _gridWidget = _ui->gridWidget;
    _statusWidget = _ui->statusWidget;
    _zoomWidget = _ui->zoomWidget;

    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAutoFillBackground(false);
    setFocusPolicy(Qt::NoFocus);

    if (_statusWidget) {
        _statusWidget->setHost(_host);
    }
    _syncChildGeometry();
    _applyWindowVisibility();
}

ViewportOverlayWindow::~ViewportOverlayWindow() = default;

void ViewportOverlayWindow::syncToViewportRect(
    const QRect &globalRect, bool visible
) {
    _viewportRect = globalRect;
    _viewportVisible = visible && globalRect.isValid();
    if (_viewportVisible && geometry() != globalRect) {
        setGeometry(globalRect);
    }
    _applyWindowVisibility();
}

void ViewportOverlayWindow::refreshOverlay() {
    if (_gridWidget) {
        _gridWidget->update();
    }
    if (_zoomWidget) {
        _zoomWidget->update();
    }
    if (_statusWidget) {
        _statusWidget->update();
    }
}

void ViewportOverlayWindow::raiseAboveViewport() {
    if (isVisible()) {
        placeWindowAboveViewport(this, parentWidget());
    }
}

void ViewportOverlayWindow::cycleGridMode() {
    if (!_gridWidget) {
        return;
    }

    int idx = 0;
    while (idx < static_cast<int>(GUI::Constants::gridModes.size())
        && GUI::Constants::gridModes[idx] != _gridWidget->gridDivisions()) {
        idx++;
    }
    idx = (idx + 1) % static_cast<int>(GUI::Constants::gridModes.size());
    _gridWidget->setGridDivisions(GUI::Constants::gridModes[idx]);
}

void ViewportOverlayWindow::toggleMinimalUI() {
    _minimalUI = !_minimalUI;
    _applyWindowVisibility();
}

void ViewportOverlayWindow::beginZoomSelection(const QPoint &origin) {
    if (_zoomWidget) {
        _zoomWidget->beginSelection(origin);
    }
}

void ViewportOverlayWindow::updateZoomSelection(const QPoint &current) {
    if (_zoomWidget) {
        _zoomWidget->updateSelection(current);
    }
}

bool ViewportOverlayWindow::scaleZoomSelection(double factor) {
    return _zoomWidget && _zoomWidget->scaleSelection(factor, rect());
}

void ViewportOverlayWindow::clearZoomSelection() {
    if (_zoomWidget) {
        _zoomWidget->clearSelection();
    }
}

QRect ViewportOverlayWindow::zoomSelectionRect() const {
    return _zoomWidget ? _zoomWidget->selectionRect() : QRect();
}

void ViewportOverlayWindow::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    _syncChildGeometry();
}

void ViewportOverlayWindow::_syncChildGeometry() {
    const QRect area = rect();
    if (_gridWidget) {
        _gridWidget->setGeometry(area);
    }
    if (_zoomWidget) {
        _zoomWidget->setGeometry(area);
    }
    if (_statusWidget) {
        _statusWidget->setGeometry(area);
    }
}

void ViewportOverlayWindow::_applyWindowVisibility() {
    const bool shouldShow = _viewportVisible && !_minimalUI;
    if (!shouldShow) {
        hide();
        return;
    }

    if (geometry() != _viewportRect && _viewportRect.isValid()) {
        setGeometry(_viewportRect);
    }
    if (!isVisible()) {
        show();
    }
    placeWindowAboveViewport(this, parentWidget());
    refreshOverlay();
}
