#include "ViewportWindow.h"

#include "ui_ViewportWindow.h"

#include "util/IncludeWin32.h"

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#include <QApplication>
#include <QHideEvent>
#include <QMoveEvent>
#include <QShowEvent>

#include "app/GUIConstants.h"

#include "util/NumberUtil.h"

#include "ViewportOverlayWindow.h"

using namespace GUI;

ViewportWindow::ViewportWindow(ViewportHost *host)
    : QWidget(nullptr)
    , _ui(std::make_unique<Ui::ViewportWindow>())
    , _host(host) {
    _ui->setupUi(this);
    _d3dWidget = _ui->d3dWidget;
    _overlayWindow = new ViewportOverlayWindow(host, this);
    _d3dWidget->setHost(host);
    _syncPresentationSize();
    _syncOverlayWindow();
    _updateWindowTitle();

    const int interactionTickIntervalMs
        = _host ? _host->interactionFrameIntervalMs() : 16;
    _rtZoomTimer.setTimerType(Qt::PreciseTimer);
    _rtZoomTimer.setInterval(interactionTickIntervalMs);
    QObject::connect(&_rtZoomTimer, &QTimer::timeout, this, [this]() {
        _syncRealtimeZoomTimerInterval();
        _applyRealtimeZoomStep(false);
        });

    _panRedrawTimer.setTimerType(Qt::PreciseTimer);
    _panRedrawTimer.setSingleShot(true);
    _panRedrawTimer.setInterval(interactionTickIntervalMs);
    QObject::connect(&_panRedrawTimer, &QTimer::timeout, this, [this]() {
        if (!_panning) return;

        _commitPanOffset();
        if (_panning) {
            _syncPanRedrawTimerInterval();
            _panRedrawTimer.start();
        }
        });

    _zoomOutRedrawTimer.setTimerType(Qt::PreciseTimer);
    _zoomOutRedrawTimer.setSingleShot(true);
    _zoomOutRedrawTimer.setInterval(interactionTickIntervalMs);
    QObject::connect(&_zoomOutRedrawTimer, &QTimer::timeout, this, [this]() {
        if (!_zoomOutDragActive) return;

        _commitZoomOutPreview();
        if (_zoomOutDragActive && _zoomOutPendingCommit) {
            _zoomOutRedrawTimer.start();
        }
        });

    _arrowPanTimer.setTimerType(Qt::PreciseTimer);
    _arrowPanTimer.setInterval(interactionTickIntervalMs);
    QObject::connect(&_arrowPanTimer, &QTimer::timeout, this, [this]() {
        _syncArrowPanTimerInterval();
        _applyArrowPanStep();
        });

    _fullscreenResizeTimer.setTimerType(Qt::PreciseTimer);
    _fullscreenResizeTimer.setSingleShot(true);
    _fullscreenResizeTimer.setInterval(120);
    QObject::connect(&_fullscreenResizeTimer, &QTimer::timeout, this, [this]() {
        if (_fullscreenTransitionPending) {
            finalizeFullscreenTransition();
        }
        });
}

ViewportWindow::~ViewportWindow() = default;

void ViewportWindow::clearPreviewOffset() {
    const bool panNeedsClear
        = !_panning && _d3dWidget && !_d3dWidget->panPreviewOffset().isNull();
    const bool zoomNeedsClear = !_zoomOutDragActive && _d3dWidget
        && !NumberUtil::almostEqual(_d3dWidget->zoomPreviewScale(), 1.0);
    if (!panNeedsClear && !zoomNeedsClear && !_zoomOutPendingCommit) {
        return;
    }

    if (!_d3dWidget) {
        return;
    }

    if (!_panning) {
        _d3dWidget->setPanPreviewOffset({});
    }
    if (!_zoomOutDragActive) {
        _zoomOutPending = false;
        _zoomOutDragMoved = false;
        _zoomOutPendingCommit = false;
        _d3dWidget->setZoomPreviewScale(1.0);
    }
    _d3dWidget->refreshPreviewTransform();
    refreshOverlay();
}

void ViewportWindow::refreshPreviewTransform() {
    _syncPresentationSize();
    if (_d3dWidget) {
        _d3dWidget->refreshPreviewTransform();
    }
    refreshOverlay();
}

void ViewportWindow::presentFrame(const PresentedFrame &frame) {
    if (_d3dWidget) {
        if (frame.valid) {
            _syncPresentationSize();
            _d3dWidget->presentFrame(frame);
        } else {
            _d3dWidget->clearFrame();
        }
    }
    refreshOverlay();
    _updateWindowTitle();
}

void ViewportWindow::refreshOverlay() {
    if (_overlayWindow) {
        _overlayWindow->refreshOverlay();
    }
    _updateWindowTitle();
}

bool ViewportWindow::event(QEvent *event) {
    const bool handled = QWidget::event(event);
    if (!event) {
        return handled;
    }

    switch (event->type()) {
        case QEvent::ActivationChange:
        case QEvent::WindowActivate:
        case QEvent::ZOrderChange:
            _syncOverlayWindow();
            break;
        default:
            break;
    }

    return handled;
}

void ViewportWindow::mousePressEvent(QMouseEvent *event) {
    const QPointF mousePos = event->position();
    _lastMousePos = mousePos.toPoint();
    if (_host) {
        _host->updateMouseCoords(_mapToOutputPixel(mousePos));
    }

    if (_host && _host->viewportUsesDirectPick() && event->button() == Qt::LeftButton
        && !_spacePan) {
        _host->pickAtPixel(_mapToOutputPixel(mousePos));
        return;
    }

    const bool panDrag = event->button() == Qt::MiddleButton
        || (_effectiveMode() == NavMode::pan
            && (event->button() == Qt::LeftButton
                || event->button() == Qt::RightButton));
    if (panDrag) {
        _beginPanInteraction(event->button());
        return;
    }

    if (_effectiveMode() == NavMode::realtimeZoom) {
        if (event->button() == Qt::LeftButton
            || event->button() == Qt::RightButton) {
            _beginRealtimeZoom(event->button() == Qt::LeftButton);
            return;
        }
    }

    if (_effectiveMode() == NavMode::zoom) {
        if (event->button() == Qt::LeftButton) {
            if (_overlayWindow) {
                _overlayWindow->beginZoomSelection(event->position().toPoint());
            }
            _zoomOutPending = false;
            _zoomOutDragActive = false;
            _zoomOutDragMoved = false;
            _zoomOutPendingCommit = false;
            _d3dWidget->setZoomPreviewScale(1.0);
            _zoomOutRedrawTimer.stop();
            grabMouse();
            refreshOverlay();
            return;
        }

        if (event->button() == Qt::RightButton) {
            _zoomOutPending = true;
            _zoomOutDragActive = true;
            _zoomOutDragMoved = false;
            _zoomOutDragLastPos = _lastMousePos;
            _zoomOutPendingCommit = false;
            if (_overlayWindow) {
                _overlayWindow->clearZoomSelection();
            }
            _d3dWidget->setZoomPreviewScale(1.0);
            _zoomOutRedrawTimer.stop();
            grabMouse();
            refreshOverlay();
            return;
        }
    }
}

void ViewportWindow::mouseMoveEvent(QMouseEvent *event) {
    const QPointF mousePos = event->position();
    _lastMousePos = mousePos.toPoint();
    if (_host) {
        _host->updateMouseCoords(_mapToOutputPixel(mousePos));
    }

    if (_panning) {
        _d3dWidget->setPanPreviewOffset(_lastMousePos - _dragOrigin);
        return;
    }

    if (_zoomOutDragActive) {
        const QPoint delta = _lastMousePos - _zoomOutDragLastPos;
        _zoomOutDragLastPos = _lastMousePos;
        const int dragMagnitude = std::abs(delta.x()) + std::abs(delta.y());
        if (dragMagnitude > 0) {
            _zoomOutDragMoved = true;
            const double zoomFactor = _host ? _host->zoomRateFactor()
                : Constants::zoomStepTable[8];
            const double zoomOutDragPixelsPerStep = 12.0;
            const double stepScale = std::pow(zoomFactor,
                static_cast<double>(dragMagnitude) / zoomOutDragPixelsPerStep);
            const double nextScale = std::clamp(_d3dWidget->zoomPreviewScale()
                * stepScale, 1.0, 64.0);
            _d3dWidget->setZoomPreviewScale(nextScale);
            _zoomOutPendingCommit = nextScale > 1.0001;
            if (_zoomOutPendingCommit && !_zoomOutRedrawTimer.isActive()) {
                _zoomOutRedrawTimer.start();
            }
        }
        return;
    }

    if (mouseGrabber() == this
        && !_zoomOutDragActive && !_panning && _effectiveMode() == NavMode::zoom
        && (event->buttons() & Qt::LeftButton)) {
        if (_overlayWindow) {
            _overlayWindow->updateZoomSelection(_lastMousePos);
        }
        refreshOverlay();
    }
}

void ViewportWindow::mouseReleaseEvent(QMouseEvent *event) {
    const QPointF mousePos = event->position();
    _lastMousePos = mousePos.toPoint();
    if (_host) {
        _host->updateMouseCoords(_mapToOutputPixel(mousePos));
    }

    if (_panning) {
        if (_panButton == Qt::NoButton || event->button() == _panButton
            || event->button() == Qt::NoButton) {
            _endPanInteraction();
        }
        return;
    }

    if (_rtZoomTimer.isActive()) {
        _rtZoomTimer.stop();
        _rtZoomLastStepAt = {};
        releaseMouse();
        _updateCursor();
        return;
    }

    if (_zoomOutDragActive
        && (event->button() == Qt::RightButton
            || event->button() == Qt::NoButton)) {
        _zoomOutDragActive = false;
        _zoomOutPending = false;
        _zoomOutRedrawTimer.stop();
        if (mouseGrabber() == this) {
            releaseMouse();
        }
        if (_zoomOutPendingCommit) {
            _commitZoomOutPreview();
        } else if (!_zoomOutDragMoved && _host) {
            _host->zoomAtPixel(_mapToOutputPixel(mousePos), false);
        }
        _zoomOutDragMoved = false;
        _zoomOutPendingCommit = false;
        _d3dWidget->setZoomPreviewScale(1.0);
        _updateCursor();
        refreshOverlay();
        return;
    }

    const QRect normalized = _overlayWindow
        ? _overlayWindow->zoomSelectionRect().normalized()
        : QRect();
    const bool selectionActive
        = !_zoomOutDragActive && mouseGrabber() == this
        && !_panning && _effectiveMode() == NavMode::zoom
        && (event->button() == Qt::LeftButton || event->button() == Qt::NoButton)
        && !normalized.isNull();
    if (selectionActive) {
        const QRect rect = _mapToOutputRect(normalized);
        if (_overlayWindow) {
            _overlayWindow->clearZoomSelection();
        }
        if (mouseGrabber() == this) {
            releaseMouse();
        }
        if (_host && rect.width() >= 8 && rect.height() >= 8) {
            _host->boxZoom(rect);
        }
        _zoomOutPending = false;
        _updateCursor();
        refreshOverlay();
        return;
    }

    if (_zoomOutPending && event->button() == Qt::RightButton && _host) {
        _zoomOutPending = false;
        _host->zoomAtPixel(_mapToOutputPixel(mousePos), false);
        refreshOverlay();
    }
}

void ViewportWindow::wheelEvent(QWheelEvent *event) {
    const QPointF mousePos = event->position();
    _lastMousePos = mousePos.toPoint();
    if (_host) {
        _host->updateMouseCoords(_mapToOutputPixel(mousePos));
    }

    if (_panning) {
        event->accept();
        return;
    }

    const auto mode = _effectiveMode();
    const bool zoomIn = event->angleDelta().y() > 0;

    if (mode == NavMode::realtimeZoom) {
        const QSize output = _host->outputSize();
        const QPointF defaultAnchor(
            static_cast<double>(std::max(0, output.width() / 2)),
            static_cast<double>(std::max(0, output.height() / 2))
        );
        const QPointF anchor = _rtZoomAnchorPixel.value_or(defaultAnchor);
        _rtZoomAnchorPixel = anchor;
        _host->zoomAtPixel(_clampToOutputPixel(anchor), zoomIn);
        return;
    }

    if (mode == NavMode::zoom) {
        const bool resizeSelection
            = mouseGrabber() == this && (event->buttons() & Qt::LeftButton);
        if (!resizeSelection) {
            event->accept();
            return;
        }

        const double factor = zoomIn ? 0.9 : 1.1;
        if (!_overlayWindow || !_overlayWindow->scaleZoomSelection(factor)) {
            event->accept();
            return;
        }

        refreshOverlay();
        event->accept();
        return;
    }

    const int delta = zoomIn ? 10 : -10;
    _host->adjustIterationsBy(delta);
}

void ViewportWindow::keyPressEvent(QKeyEvent *event) {
    if (!event->isAutoRepeat()
        && (event->key() == Qt::Key_Shift || event->key() == Qt::Key_Control)) {
        _refreshActiveInteractionTimers();
    }
    if (_handleArrowPanKeyPress(event)) {
        event->accept();
        return;
    }
    if (event->isAutoRepeat()) return;

    if (_host && _host->matchesShortcut("cancel", event)) {
        _host->cancelQueuedRenders();
        event->accept();
        return;
    }

    if (_host && _host->matchesShortcut("cycleMode", event)) {
        _host->cycleNavMode();
        event->accept();
        return;
    }

    if (_host && _host->matchesShortcut("home", event)) {
        _host->applyHomeView();
        event->accept();
        return;
    }

    if (_host && _host->matchesShortcut("quickSave", event)) {
        _host->quickSaveImage();
        event->accept();
        return;
    }

    if (_host && _host->matchesShortcut("fullscreen", event)) {
        toggleFullscreen();
        event->accept();
        return;
    }

    if (_host && _host->matchesShortcut("overlay", event)) {
        toggleMinimalUI();
        event->accept();
        return;
    }

    if (_host && _host->matchesShortcut("cycleGrid", event)) {
        cycleGridMode();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Space) {
        _spacePan = true;
        _updateCursor();
        event->accept();
        return;
    }

    QWidget::keyPressEvent(event);
}

void ViewportWindow::keyReleaseEvent(QKeyEvent *event) {
    if (!event->isAutoRepeat()
        && (event->key() == Qt::Key_Shift || event->key() == Qt::Key_Control)) {
        _refreshActiveInteractionTimers();
    }
    if (_handleArrowPanKeyRelease(event)) {
        event->accept();
        return;
    }
    if (event->isAutoRepeat()) return;

    if (event->key() == Qt::Key_Space) {
        _spacePan = false;
        _updateCursor();
        event->accept();
        return;
    }

    QWidget::keyReleaseEvent(event);
}

void ViewportWindow::enterEvent(QEnterEvent *) {
    _updateCursor();
}

void ViewportWindow::leaveEvent(QEvent *) {
    if (_host) {
        _host->clearMouseCoords();
    }
}

void ViewportWindow::moveEvent(QMoveEvent *event) {
    QWidget::moveEvent(event);
    _syncOverlayWindow();
}

void ViewportWindow::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    _syncOverlayWindow();
}

void ViewportWindow::hideEvent(QHideEvent *event) {
    QWidget::hideEvent(event);
    _hideOverlayWindow();
}

void ViewportWindow::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (_d3dWidget) {
        _d3dWidget->setGeometry(rect());
        _d3dWidget->refreshPreviewTransform();
    }
    _syncOverlayWindow();
    if (!event) return;

    const QSize logicalSize = event->size();
    if (logicalSize.isValid()) {
        emit viewportScaleAdjustmentRequested(logicalSize);
    }
}

bool ViewportWindow::nativeEvent(
    const QByteArray &eventType, void *message, qintptr *result
) {
    const bool handled = QWidget::nativeEvent(eventType, message, result);
    if (!message || !result || isFullScreen()) {
        return handled;
    }

    MSG *msg = static_cast<MSG *>(message);
    if (!msg) return handled;

    if (msg->message == WM_NCHITTEST) {
        switch (*result) {
            case HTLEFT:
            case HTRIGHT:
            case HTTOP:
            case HTBOTTOM:
                *result = HTCLIENT;
                return true;
            default:
                return handled;
        }
    }

    if (msg->message == WM_SIZING && _host && _host->outputSize().isValid()) {
        RECT *rect = reinterpret_cast<RECT *>(msg->lParam);
        if (!rect) return handled;

        int nonClientWidth = 0;
        int nonClientHeight = 0;
        if (const HWND hwnd = reinterpret_cast<HWND>(winId())) {
            RECT windowRect{};
            RECT clientRect{};
            if (GetWindowRect(hwnd, &windowRect) && GetClientRect(hwnd, &clientRect)) {
                nonClientWidth = static_cast<int>(
                    (windowRect.right - windowRect.left)
                    - (clientRect.right - clientRect.left)
                );
                nonClientHeight = static_cast<int>(
                    (windowRect.bottom - windowRect.top)
                    - (clientRect.bottom - clientRect.top)
                );
            }
        }

        const QSize newSize = _host->constrainViewportSize(
            QSize(static_cast<int>(rect->right - rect->left),
                static_cast<int>(rect->bottom - rect->top)),
            QSize(nonClientWidth, nonClientHeight)
        );

        switch (msg->wParam) {
            case WMSZ_TOPLEFT:
                rect->left = rect->right - newSize.width();
                rect->top = rect->bottom - newSize.height();
                break;
            case WMSZ_TOPRIGHT:
                rect->right = rect->left + newSize.width();
                rect->top = rect->bottom - newSize.height();
                break;
            case WMSZ_BOTTOMLEFT:
                rect->left = rect->right - newSize.width();
                rect->bottom = rect->top + newSize.height();
                break;
            case WMSZ_BOTTOMRIGHT:
                rect->right = rect->left + newSize.width();
                rect->bottom = rect->top + newSize.height();
                break;
            default:
                return handled;
        }

        *result = TRUE;
        return true;
    }

    return handled;
}

void ViewportWindow::closeEvent(QCloseEvent *event) {
    _rtZoomTimer.stop();
    _rtZoomLastStepAt = {};
    _panRedrawTimer.stop();
    _zoomOutRedrawTimer.stop();
    _arrowPanTimer.stop();
    if (mouseGrabber() == this) {
        releaseMouse();
    }
    if (!_closeAllowed) {
        event->ignore();
        const bool skipDirtyViewPrompt
            = (QApplication::keyboardModifiers() & Qt::ShiftModifier) != 0;
        emit closeRequested(skipDirtyViewPrompt);
        return;
    }
    _hideOverlayWindow();
    event->accept();
}

void ViewportWindow::cycleGridMode() {
    if (_overlayWindow) {
        _overlayWindow->cycleGridMode();
    }
}

void ViewportWindow::toggleMinimalUI() {
    if (_overlayWindow) {
        _overlayWindow->toggleMinimalUI();
    }
    _updateCursor();
    _syncOverlayWindow();
}

void ViewportWindow::toggleFullscreen() {
    if (!_host) return;

    _host->prepareViewportFullscreenTransition();

    if (!isFullScreen()) {
        _restoreGeometry = geometry();
        _restoreMaximized = isMaximized();
        _restoreOutputSize = _host->outputSize();
        _fullscreenTransitionPending = true;
        setMinimumSize(1, 1);
        setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        showFullScreen();
        _syncOverlayWindow();
        _fullscreenResizeTimer.start();
        return;
    }

    _fullscreenTransitionPending = true;
    if (_restoreMaximized) {
        showMaximized();
    } else {
        showNormal();
        if (_restoreGeometry.isValid()) {
            setGeometry(_restoreGeometry);
        }
    }
    _syncOverlayWindow();
    _fullscreenResizeTimer.start();
}

void ViewportWindow::finalizeFullscreenTransition() {
    if (!_host || !_fullscreenTransitionPending) return;

    _fullscreenTransitionPending = false;
    if (isFullScreen()) {
        const double dpr = devicePixelRatioF();
        const QSize logicalSize = size();
        _host->applyViewportOutputSize(QSize(std::max(1,
            static_cast<int>(std::lround(logicalSize.width() * dpr))),
            std::max(1,
                static_cast<int>(std::lround(logicalSize.height() * dpr)))));
        _syncOverlayWindow();
        return;
    }

    if (_restoreOutputSize.isValid()) {
        _host->applyViewportOutputSize(_restoreOutputSize);
    }
    _syncOverlayWindow();
}

bool ViewportWindow::_precisionModifierActive() const {
    return QApplication::keyboardModifiers().testFlag(Qt::ShiftModifier);
}

bool ViewportWindow::_speedModifierActive() const {
    return QApplication::keyboardModifiers().testFlag(Qt::ControlModifier);
}

int ViewportWindow::_baseInteractionTickIntervalMs() const {
    return _host ? _host->interactionFrameIntervalMs() : 16;
}

int ViewportWindow::_precisionInteractionTickIntervalMs() const {
    return _precisionModifierActive()
        ? Constants::precisionInteractionDelayMs
        : _baseInteractionTickIntervalMs();
}

void ViewportWindow::_syncPanRedrawTimerInterval() {
    _panRedrawTimer.setInterval(_precisionInteractionTickIntervalMs());
}

void ViewportWindow::_syncRealtimeZoomTimerInterval() {
    _rtZoomTimer.setInterval(_precisionInteractionTickIntervalMs());
}

void ViewportWindow::_syncArrowPanTimerInterval() {
    _arrowPanTimer.setInterval(_precisionInteractionTickIntervalMs());
}

void ViewportWindow::_refreshActiveInteractionTimers() {
    _syncPanRedrawTimerInterval();
    _syncRealtimeZoomTimerInterval();
    _syncArrowPanTimerInterval();
}

void ViewportWindow::_resetZoomOutDragState() {
    _zoomOutPending = false;
    _zoomOutDragActive = false;
    _zoomOutDragMoved = false;
    _zoomOutPendingCommit = false;
    if (_d3dWidget) {
        _d3dWidget->setZoomPreviewScale(1.0);
    }
}

void ViewportWindow::_stopRealtimeZoom() {
    _rtZoomTimer.stop();
    _rtZoomLastStepAt = {};
}

bool ViewportWindow::_canBeginPanInteraction() const {
    return !_panning && !_rtZoomTimer.isActive() && !_zoomOutDragActive;
}

void ViewportWindow::_beginPanInteraction(Qt::MouseButton button) {
    if (!_canBeginPanInteraction()) return;

    _panning = true;
    _panButton = button;
    if (_host) {
        _host->setDisplayedNavModeOverride(NavMode::pan);
    }
    _dragOrigin = _lastMousePos;
    if (_overlayWindow) {
        _overlayWindow->clearZoomSelection();
    }
    if (_d3dWidget) {
        _d3dWidget->setPanPreviewOffset({});
    }
    _resetZoomOutDragState();
    _stopRealtimeZoom();
    _panRedrawTimer.stop();
    _zoomOutRedrawTimer.stop();
    _syncPanRedrawTimerInterval();
    _panRedrawTimer.start();
    grabMouse();
    _updateCursor();
    refreshOverlay();
}

void ViewportWindow::_endPanInteraction() {
    _panRedrawTimer.stop();
    _panning = false;
    _commitPanOffset();
    _panButton = Qt::NoButton;
    if (_host) {
        _host->setDisplayedNavModeOverride(std::nullopt);
    }
    if (mouseGrabber() == this) {
        releaseMouse();
    }
    _updateCursor();
    refreshOverlay();
}

void ViewportWindow::_beginRealtimeZoom(bool zoomIn) {
    if (!_host) return;
    if (!_rtZoomAnchorPixel.has_value()) {
        const QSize output = _host->outputSize();
        _rtZoomAnchorPixel
            = QPointF(static_cast<double>(std::max(0, output.width() / 2)),
                static_cast<double>(std::max(0, output.height() / 2)));
    }
    _rtZoomZoomIn = zoomIn;
    _rtZoomLastStepAt = std::chrono::steady_clock::now();
    _syncRealtimeZoomTimerInterval();
    grabMouse();
    if (!_precisionModifierActive()) {
        _applyRealtimeZoomStep(true);
    }
    _rtZoomTimer.start();
}

bool ViewportWindow::_handleArrowPanKeyPress(QKeyEvent *event) {
    if (!_host || _effectiveMode() != NavMode::pan) {
        return false;
    }
    if (_zoomOutDragActive || _rtZoomTimer.isActive()) {
        return false;
    }

    bool *pressed = nullptr;
    switch (event->key()) {
        case Qt::Key_Left:
            pressed = &_arrowPanLeft;
            break;
        case Qt::Key_Right:
            pressed = &_arrowPanRight;
            break;
        case Qt::Key_Up:
            pressed = &_arrowPanUp;
            break;
        case Qt::Key_Down:
            pressed = &_arrowPanDown;
            break;
        default:
            return false;
    }

    if (event->isAutoRepeat()) {
        return true;
    }

    const bool alreadyPressed = *pressed;
    *pressed = true;
    _syncArrowPanTimerInterval();
    if (!_arrowPanTimer.isActive()) {
        _arrowPanTimer.start();
    }
    const bool precisionMode = event->modifiers().testFlag(Qt::ShiftModifier);
    if (!event->isAutoRepeat() && !alreadyPressed && !precisionMode) {
        _applyArrowPanStep();
    }
    return true;
}

bool ViewportWindow::_handleArrowPanKeyRelease(QKeyEvent *event) {
    bool *pressed = nullptr;
    switch (event->key()) {
        case Qt::Key_Left:
            pressed = &_arrowPanLeft;
            break;
        case Qt::Key_Right:
            pressed = &_arrowPanRight;
            break;
        case Qt::Key_Up:
            pressed = &_arrowPanUp;
            break;
        case Qt::Key_Down:
            pressed = &_arrowPanDown;
            break;
        default:
            return false;
    }

    if (event->isAutoRepeat()) {
        return true;
    }

    *pressed = false;
    if (!_arrowPanLeft && !_arrowPanRight && !_arrowPanUp && !_arrowPanDown) {
        _arrowPanTimer.stop();
    } else {
        _syncArrowPanTimerInterval();
    }
    return true;
}

void ViewportWindow::_applyArrowPanStep() {
    if (!_host || _effectiveMode() != NavMode::pan) {
        _arrowPanTimer.stop();
        return;
    }
    if (_zoomOutDragActive || _rtZoomTimer.isActive()) {
        return;
    }

    const int xDir = (_arrowPanLeft ? 1 : 0) + (_arrowPanRight ? -1 : 0);
    const int yDir = (_arrowPanUp ? 1 : 0) + (_arrowPanDown ? -1 : 0);
    if (xDir == 0 && yDir == 0) {
        _arrowPanTimer.stop();
        return;
    }

    const int panRate = _host->panRateValue();
    if (panRate <= 0) {
        return;
    }

    int step = 1;
    if (panRate > 1) {
        const int shiftedRate = panRate - 1;
        const double panFactor
            = std::pow(1.125, static_cast<double>(shiftedRate - 8));
        if (!(panFactor > 0.0) || !std::isfinite(panFactor)) {
            return;
        }
        step = std::max(1, static_cast<int>(std::lround(16.0 * panFactor)));
    }
    if (_speedModifierActive()) {
        step = std::max(1,
            static_cast<int>(std::lround(static_cast<double>(step)
                * Constants::boostedPanSpeedFactor)));
    }
    _host->panByPixels(QPoint(xDir * step, yDir * step));
}

void ViewportWindow::_syncOverlayWindow() {
    if (!_overlayWindow) {
        return;
    }

    const bool visible = isVisible() && !isMinimized()
        && _d3dWidget && _d3dWidget->isVisible();
    if (!visible) {
        _hideOverlayWindow();
        return;
    }

    _overlayWindow->syncToViewportRect(_viewportClientGlobalRect(), true);
    _overlayWindow->raiseAboveViewport();
}

void ViewportWindow::_hideOverlayWindow() {
    if (_overlayWindow) {
        _overlayWindow->syncToViewportRect({}, false);
    }
}

void ViewportWindow::_syncPresentationSize() {
    if (_d3dWidget) {
        _d3dWidget->setPresentationSize(_host ? _host->outputSize() : QSize());
    }
}

void ViewportWindow::_updateWindowTitle() {
    if (!_host) {
        setWindowTitle(tr("Viewport"));
        return;
    }

    setWindowTitle(_host->shouldUseInteractionPreviewFallback()
            ? tr("Viewport (Preview)")
            : tr("Viewport (Direct)"));
}

QRect ViewportWindow::_viewportClientGlobalRect() const {
    if (!_d3dWidget || !_d3dWidget->isVisible()) {
        return {};
    }

    const QSize size = _d3dWidget->size();
    if (!size.isValid()) {
        return {};
    }

    return QRect(_d3dWidget->mapToGlobal(QPoint(0, 0)), size);
}

QPoint ViewportWindow::_clampToOutputPixel(const QPointF &pixel) const {
    const QSize outputSize = _host ? _host->outputSize() : QSize();
    return { std::clamp(static_cast<int>(std::lround(pixel.x())), 0,
                 std::max(0, outputSize.width() - 1)),
        std::clamp(static_cast<int>(std::lround(pixel.y())), 0,
            std::max(0, outputSize.height() - 1)) };
}

QPoint ViewportWindow::_mapToOutputPixel(const QPoint &logicalPoint) const {
    return _mapToOutputPixel(QPointF(logicalPoint));
}

QPoint ViewportWindow::_mapToOutputPixel(const QPointF &logicalPoint) const {
    const QSize logicalSize = size();
    const QSize outputSize = _host ? _host->outputSize() : QSize();
    if (logicalSize.width() <= 0 || logicalSize.height() <= 0) {
        return {};
    }

    const auto mapAxis = [](double logical, int logicalExtent, int outputExtent) {
        if (outputExtent <= 1 || logicalExtent <= 1) {
            return 0.0;
        }

        return logical * static_cast<double>(outputExtent - 1)
            / static_cast<double>(logicalExtent - 1);
    };

    const double x = mapAxis(logicalPoint.x(), logicalSize.width(),
        outputSize.width());
    const double y = mapAxis(logicalPoint.y(), logicalSize.height(),
        outputSize.height());

    return { std::clamp(static_cast<int>(std::lround(x)), 0,
                 std::max(0, outputSize.width() - 1)),
        std::clamp(static_cast<int>(std::lround(y)), 0,
            std::max(0, outputSize.height() - 1)) };
}

QPoint ViewportWindow::_mapToOutputDelta(const QPoint &logicalDelta) const {
    const QSize logicalSize = size();
    const QSize outputSize = _host ? _host->outputSize() : QSize();
    if (logicalSize.width() <= 0 || logicalSize.height() <= 0) {
        return {};
    }

    return { static_cast<int>(std::lround(static_cast<double>(logicalDelta.x())
                 * outputSize.width() / std::max(1, logicalSize.width()))),
        static_cast<int>(std::lround(static_cast<double>(logicalDelta.y())
            * outputSize.height() / std::max(1, logicalSize.height()))) };
}

QRect ViewportWindow::_mapToOutputRect(const QRect &logicalRect) const {
    const QPoint topLeft = _mapToOutputPixel(logicalRect.topLeft());
    const QPoint bottomRight = _mapToOutputPixel(logicalRect.bottomRight());
    return QRect(topLeft, bottomRight).normalized();
}

NavMode ViewportWindow::_effectiveMode() const {
    return (_spacePan || (_panning && _panButton == Qt::MiddleButton))
        ? NavMode::pan
        : (_host ? _host->navMode() : NavMode::realtimeZoom);
}

void ViewportWindow::_updateCursor() {
    if (_overlayWindow && _overlayWindow->minimalUIEnabled()) {
        setCursor(Qt::BlankCursor);
        return;
    }

    if (_panning) {
        setCursor(Qt::ClosedHandCursor);
        return;
    }

    switch (_effectiveMode()) {
        case NavMode::realtimeZoom:
        case NavMode::zoom:
            setCursor(Qt::CrossCursor);
            break;
        case NavMode::pan:
            setCursor(Qt::OpenHandCursor);
            break;
    }
}

void ViewportWindow::_commitPanOffset() {
    if (!_host || !_d3dWidget) return;

    const QPoint logicalOffset = _d3dWidget->panPreviewOffset();
    if (logicalOffset.isNull()) return;

    const QPoint delta = _mapToOutputDelta(logicalOffset);
    if (delta.isNull()) {
        _d3dWidget->setPanPreviewOffset({});
        _dragOrigin = _lastMousePos;
        return;
    }

    _deferPreviewTransformRefresh = true;
    _host->panByPixels(delta);
    _deferPreviewTransformRefresh = false;
    _dragOrigin = _lastMousePos;
    _d3dWidget->setPanPreviewOffset({});
}

void ViewportWindow::_commitZoomOutPreview() {
    if (!_host || !_d3dWidget) return;
    const double scaleMultiplier = _d3dWidget->zoomPreviewScale();
    if (!_zoomOutPendingCommit || NumberUtil::almostEqual(scaleMultiplier, 1.0)) {
        return;
    }

    const QPoint viewportCenter(width() / 2, height() / 2);
    _zoomOutPendingCommit = false;
    _deferPreviewTransformRefresh = true;
    _host->scaleAtPixel(_mapToOutputPixel(viewportCenter), scaleMultiplier);
    _deferPreviewTransformRefresh = false;
    _d3dWidget->setZoomPreviewScale(1.0);
}

void ViewportWindow::_applyRealtimeZoomStep(bool firstStep) {
    if (!_host || !_rtZoomAnchorPixel.has_value()) return;

    const auto now = std::chrono::steady_clock::now();
    constexpr double baseRateFPS = 60.0;
    double elapsedSeconds = 1.0 / baseRateFPS;

    if (!firstStep) {
        if (_rtZoomLastStepAt.time_since_epoch().count() == 0) {
            _rtZoomLastStepAt = now;
            return;
        }

        const auto elapsed = std::chrono::duration_cast<
            std::chrono::duration<double, std::milli>>(now - _rtZoomLastStepAt);
        elapsedSeconds = elapsed.count() / 1000.0;
        if (!(elapsedSeconds > 0.0) || !std::isfinite(elapsedSeconds)) return;
    }

    _rtZoomLastStepAt = now;
    _updateRealtimeZoomAnchor(elapsedSeconds);
    const double zoomFactor = _host->zoomRateFactor();
    if (!(zoomFactor > 0.0) || !std::isfinite(zoomFactor)) return;

    const double scalePerSecond = std::pow(zoomFactor, baseRateFPS);
    const double scaleMultiplier = _rtZoomZoomIn
        ? std::pow(scalePerSecond, -elapsedSeconds)
        : std::pow(scalePerSecond, elapsedSeconds);
    if (!(scaleMultiplier > 0.0) || !std::isfinite(scaleMultiplier)) return;

    _host->scaleAtPixel(_clampToOutputPixel(*_rtZoomAnchorPixel),
        scaleMultiplier);
}

void ViewportWindow::_updateRealtimeZoomAnchor(double elapsedSeconds) {
    if (!_host || !_rtZoomAnchorPixel.has_value()) return;

    const QPointF targetPixel(_mapToOutputPixel(_lastMousePos));
    QPointF anchorPixel = *_rtZoomAnchorPixel;
    const double dx = targetPixel.x() - anchorPixel.x();
    const double dy = targetPixel.y() - anchorPixel.y();
    if (NumberUtil::almostEqual(dx, 0.0, 1e-6)
        && NumberUtil::almostEqual(dy, 0.0, 1e-6)) {
        _rtZoomAnchorPixel = targetPixel;
        return;
    }

    const double distance = std::hypot(dx, dy);
    if (!(distance > 0.0) || !std::isfinite(distance)) {
        _rtZoomAnchorPixel = targetPixel;
        return;
    }

    double panFactor = std::max(0.1, _host->panRateFactor());
    if (_speedModifierActive()) {
        panFactor *= Constants::boostedPanSpeedFactor;
    }
    const double followPixelsPerSecond = 32.0 * panFactor * 60.0;
    const double maxStep
        = std::max(1.0, followPixelsPerSecond * elapsedSeconds);
    if (distance <= maxStep) {
        _rtZoomAnchorPixel = targetPixel;
        return;
    }

    const double t = maxStep / distance;
    anchorPixel.rx() += dx * t;
    anchorPixel.ry() += dy * t;
    const QSize output = _host->outputSize();
    _rtZoomAnchorPixel
        = QPointF(std::clamp(anchorPixel.x(), 0.0,
            static_cast<double>(std::max(0, output.width() - 1))),
            std::clamp(anchorPixel.y(), 0.0,
                static_cast<double>(std::max(0, output.height() - 1))));
}
