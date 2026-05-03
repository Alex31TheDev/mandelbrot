#include "ViewportWindow.h"

#include <QApplication>
#include "ui_ViewportWindow.h"
#include "app/GUIConstants.h"
#include "util/IncludeWin32.h"
#include "util/NumberUtil.h"

using namespace GUI;

ViewportWindow::ViewportWindow(ViewportHost *host)
    : QWidget(nullptr)
    , _ui(std::make_unique<Ui::ViewportWindow>())
    , _host(host) {
    _ui->setupUi(this);
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
    const bool panNeedsClear = !_panning && !_panOffset.isNull();
    const bool zoomNeedsClear = !_zoomOutDragActive
        && !NumberUtil::almostEqual(_zoomOutPreviewScale, 1.0);
    if (!panNeedsClear && !zoomNeedsClear && !_zoomOutPendingCommit) {
        return;
    }

    if (!_panning) {
        _panOffset = {};
    }
    if (!_zoomOutDragActive) {
        _zoomOutPending = false;
        _zoomOutDragMoved = false;
        _zoomOutPendingCommit = false;
        _zoomOutPreviewScale = 1.0;
    }
    presentFrame(_presentedViewState);
}

void ViewportWindow::presentFrame(const ViewTextState &displayedView) {
    _presentedViewState = displayedView;
    _updateWindowTitle();
    update();
}

ViewTextState ViewportWindow::displayedPreviewView() const {
    if (!_host || !_host->hasDisplayedViewState() || !_presentedViewState.valid) {
        return {};
    }

    return _presentedViewState;
}

ViewTextState ViewportWindow::targetPreviewView() const {
    if (!_host) {
        return {};
    }

    ViewTextState view = _host->currentViewTextState();
    if (!view.valid) return view;

    if (_panning && !_panOffset.isNull()) {
        QString error;
        ViewTextState pannedView;
        if (_host->previewPannedViewState(
            _mapToOutputDelta(_panOffset), pannedView, error)) {
            view = pannedView;
        } else {
            view.valid = false;
            return view;
        }
    }

    if (!NumberUtil::almostEqual(_zoomOutPreviewScale, 1.0)) {
        QString error;
        ViewTextState scaledView;
        const QPoint outputCenter
            = _mapToOutputPixel(QPoint(width() / 2, height() / 2));
        if (_host->previewScaledViewState(
            outputCenter, _zoomOutPreviewScale, scaledView, error)) {
            view = scaledView;
        } else {
            view.valid = false;
            return view;
        }
    }

    return view;
}

std::optional<ViewportWindow::PreviewTransform>
ViewportWindow::previewTransform() const {
    const QSize logicalSize = size();
    if (!_host || logicalSize.width() <= 0 || logicalSize.height() <= 0) {
        return std::nullopt;
    }

    const ViewTextState sourceView = displayedPreviewView();
    const ViewTextState targetView = targetPreviewView();
    if (!sourceView.valid || !targetView.valid) {
        return std::nullopt;
    }

    if (sourceView.outputSize.width() <= 0
        || sourceView.outputSize.height() <= 0
        || targetView.outputSize.width() <= 0
        || targetView.outputSize.height() <= 0) {
        return std::nullopt;
    }

    const int targetMidX = std::clamp(targetView.outputSize.width() / 2, 0,
        std::max(0, targetView.outputSize.width() - 1));
    const int targetMidY = std::clamp(targetView.outputSize.height() / 2, 0,
        std::max(0, targetView.outputSize.height() - 1));

    QPointF sourceLeft;
    QPointF sourceRight;
    QPointF sourceTop;
    QPointF sourceBottom;
    QString error;
    if (!_host->mapViewPixelToViewPixel(
        sourceView, targetView, QPoint(0, targetMidY), sourceLeft, error)
        || !_host->mapViewPixelToViewPixel(sourceView, targetView,
            QPoint(std::max(0, targetView.outputSize.width() - 1), targetMidY),
            sourceRight, error)
        || !_host->mapViewPixelToViewPixel(
            sourceView, targetView, QPoint(targetMidX, 0), sourceTop, error)
        || !_host->mapViewPixelToViewPixel(sourceView, targetView,
            QPoint(targetMidX, std::max(0, targetView.outputSize.height() - 1)),
            sourceBottom, error)) {
        return std::nullopt;
    }

    const auto sourceToLogical = [&](const QPointF &pixel) {
        return QPointF(pixel.x() * logicalSize.width()
            / std::max(1, sourceView.outputSize.width()),
            pixel.y() * logicalSize.height()
            / std::max(1, sourceView.outputSize.height()));
        };
    const auto targetToLogical = [&](const QPoint &pixel) {
        return QPointF(static_cast<double>(pixel.x()) * logicalSize.width()
            / std::max(1, targetView.outputSize.width()),
            static_cast<double>(pixel.y()) * logicalSize.height()
            / std::max(1, targetView.outputSize.height()));
        };

    const QPointF sourceLeftLogical = sourceToLogical(sourceLeft);
    const QPointF sourceRightLogical = sourceToLogical(sourceRight);
    const QPointF sourceTopLogical = sourceToLogical(sourceTop);
    const QPointF sourceBottomLogical = sourceToLogical(sourceBottom);
    const QPointF targetLeftLogical = targetToLogical(QPoint(0, targetMidY));
    const QPointF targetRightLogical = targetToLogical(
        QPoint(std::max(0, targetView.outputSize.width() - 1), targetMidY));
    const QPointF targetTopLogical = targetToLogical(QPoint(targetMidX, 0));
    const QPointF targetBottomLogical = targetToLogical(
        QPoint(targetMidX, std::max(0, targetView.outputSize.height() - 1)));

    const double sourceSpanX = sourceRightLogical.x() - sourceLeftLogical.x();
    const double sourceSpanY = sourceBottomLogical.y() - sourceTopLogical.y();
    if (NumberUtil::almostEqual(sourceSpanX, 0.0, 1e-9)
        || NumberUtil::almostEqual(sourceSpanY, 0.0, 1e-9)) {
        return std::nullopt;
    }

    const QPointF center(static_cast<double>(logicalSize.width()) / 2.0,
        static_cast<double>(logicalSize.height()) / 2.0);
    const double scaleX
        = (targetRightLogical.x() - targetLeftLogical.x()) / sourceSpanX;
    const double scaleY
        = (targetBottomLogical.y() - targetTopLogical.y()) / sourceSpanY;
    const double translationXLeft = targetLeftLogical.x() - center.x()
        - scaleX * (sourceLeftLogical.x() - center.x());
    const double translationXRight = targetRightLogical.x() - center.x()
        - scaleX * (sourceRightLogical.x() - center.x());
    const double translationYTop = targetTopLogical.y() - center.y()
        - scaleY * (sourceTopLogical.y() - center.y());
    const double translationYBottom = targetBottomLogical.y() - center.y()
        - scaleY * (sourceBottomLogical.y() - center.y());
    const QPointF translation((translationXLeft + translationXRight) * 0.5,
        (translationYTop + translationYBottom) * 0.5);
    if (!std::isfinite(translation.x()) || !std::isfinite(translation.y())
        || !std::isfinite(scaleX) || !std::isfinite(scaleY) || !(scaleX > 0.0)
        || !(scaleY > 0.0)) {
        return std::nullopt;
    }

    const bool active = !NumberUtil::almostEqual(scaleX, 1.0, 1e-6)
        || !NumberUtil::almostEqual(scaleY, 1.0, 1e-6)
        || !NumberUtil::almostEqual(translation.x(), 0.0, 1e-3)
        || !NumberUtil::almostEqual(translation.y(), 0.0, 1e-3);
    if (!active) {
        return std::nullopt;
    }

    return PreviewTransform{ center, translation, scaleX, scaleY };
}

void ViewportWindow::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    if (event) {
        painter.setClipRegion(event->region());
    }
    painter.fillRect(rect(), Qt::black);

    const bool selectionPreviewActive = !_selectionRect.isNull();
    const std::optional<PreviewTransform> availableTransform
        = (_usePreviewFallback() || selectionPreviewActive)
        ? previewTransform()
        : std::nullopt;
    if (_host) {
        _host->withPreviewImage([&](const QImage &image) {
            if (availableTransform) {
                painter.save();
                painter.translate(availableTransform->translation);
                painter.translate(availableTransform->center);
                painter.scale(
                    availableTransform->scaleX, availableTransform->scaleY
                );
                painter.translate(-availableTransform->center);
                painter.drawImage(rect(), image);
                painter.restore();
            } else {
                painter.drawImage(rect(), image);
            }
            });
    }

    if (_minimalUI) {
        return;
    }

    painter.setRenderHint(QPainter::Antialiasing, true);
    _drawGrid(painter);

    if (!_selectionRect.isNull()) {
        painter.setPen(QPen(QColor(255, 255, 255, 180), 1.0, Qt::DashLine));
        painter.drawRect(_selectionRect.normalized());
    }

    const QString statusText = _host->viewportStatusText();
    constexpr int overlayPaddingX = 8;
    constexpr int overlayPaddingY = 6;
    const QRect overlayRect = _statusOverlayRect();

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(80, 80, 80, 150));
    painter.drawRoundedRect(overlayRect, 8.0, 8.0);

    painter.setPen(QColor(230, 230, 230));
    painter.drawText(overlayRect.adjusted(overlayPaddingX, overlayPaddingY,
        -overlayPaddingX, -overlayPaddingY),
        Qt::AlignTop | Qt::AlignLeft, statusText);
}

void ViewportWindow::mousePressEvent(QMouseEvent *event) {
    _lastMousePos = event->position().toPoint();
    _host->updateMouseCoords(_mapToOutputPixel(_lastMousePos));

    if (_host->viewportUsesDirectPick() && event->button() == Qt::LeftButton
        && !_spacePan) {
        _host->pickAtPixel(_mapToOutputPixel(_lastMousePos));
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
            _selectionOrigin = event->position().toPoint();
            _selectionRect = QRect(_selectionOrigin, QSize(1, 1));
            _zoomOutPending = false;
            _zoomOutDragActive = false;
            _zoomOutDragMoved = false;
            _zoomOutPendingCommit = false;
            _zoomOutPreviewScale = 1.0;
            _zoomOutRedrawTimer.stop();
            grabMouse();
            return;
        }

        if (event->button() == Qt::RightButton) {
            _zoomOutPending = true;
            _zoomOutDragActive = true;
            _zoomOutDragMoved = false;
            _zoomOutDragLastPos = _lastMousePos;
            _zoomOutPendingCommit = false;
            _zoomOutPreviewScale = 1.0;
            _selectionRect = {};
            _zoomOutRedrawTimer.stop();
            grabMouse();
            return;
        }
    }
}

void ViewportWindow::mouseMoveEvent(QMouseEvent *event) {
    _lastMousePos = event->position().toPoint();
    _host->updateMouseCoords(_mapToOutputPixel(_lastMousePos));

    if (_panning) {
        _panOffset = _lastMousePos - _dragOrigin;
        update();
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
            _zoomOutPreviewScale
                = std::clamp(_zoomOutPreviewScale * stepScale, 1.0, 64.0);
            _zoomOutPendingCommit = _zoomOutPreviewScale > 1.0001;
            if (_zoomOutPendingCommit && !_zoomOutRedrawTimer.isActive()) {
                _zoomOutRedrawTimer.start();
            }
            update();
        }
        return;
    }

    if (!_selectionRect.isNull()) {
        _selectionRect.setBottomRight(_lastMousePos);
        update();
    }
}

void ViewportWindow::mouseReleaseEvent(QMouseEvent *event) {
    _lastMousePos = event->position().toPoint();
    _host->updateMouseCoords(_mapToOutputPixel(_lastMousePos));

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
        update();
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
        } else if (!_zoomOutDragMoved) {
            _host->zoomAtPixel(_mapToOutputPixel(_lastMousePos), false);
        }
        _zoomOutDragMoved = false;
        _zoomOutPendingCommit = false;
        _zoomOutPreviewScale = 1.0;
        _updateCursor();
        update();
        return;
    }

    if (!_selectionRect.isNull()) {
        const bool commitSelection = event->button() == Qt::LeftButton
            || event->button() == Qt::NoButton;
        const QRect rect = _mapToOutputRect(_selectionRect.normalized());
        _selectionRect = {};
        if (mouseGrabber() == this) {
            releaseMouse();
        }

        if (commitSelection) {
            if (rect.width() >= 8 && rect.height() >= 8) {
                _host->boxZoom(rect);
            }
        }

        _zoomOutPending = false;
        _updateCursor();
        update();
        return;
    }

    if (_zoomOutPending && event->button() == Qt::RightButton) {
        _zoomOutPending = false;
        _host->zoomAtPixel(_mapToOutputPixel(_lastMousePos), false);
    }
}

void ViewportWindow::wheelEvent(QWheelEvent *event) {
    _lastMousePos = event->position().toPoint();
    _host->updateMouseCoords(_mapToOutputPixel(_lastMousePos));

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
            static_cast<double>(std::max(0, output.height() / 2)));
        const QPointF anchor = _rtZoomAnchorPixel.value_or(defaultAnchor);
        _rtZoomAnchorPixel = anchor;
        _host->zoomAtPixel(_clampToOutputPixel(anchor), zoomIn);
        return;
    }

    if (mode == NavMode::zoom) {
        const bool resizeSelection
            = !_selectionRect.isNull() && (event->buttons() & Qt::LeftButton);
        if (!resizeSelection) {
            event->accept();
            return;
        }

        const QRect current = _selectionRect.normalized();
        const QPointF center = current.center();
        const double factor = zoomIn ? 0.9 : 1.1;
        const double maxWidth = std::max(2.0, static_cast<double>(width() - 2));
        const double maxHeight
            = std::max(2.0, static_cast<double>(height() - 2));
        const double nextWidth
            = std::clamp(current.width() * factor, 2.0, maxWidth);
        const double nextHeight
            = std::clamp(current.height() * factor, 2.0, maxHeight);
        QRect scaled(
            static_cast<int>(std::lround(center.x() - nextWidth / 2.0)),
            static_cast<int>(std::lround(center.y() - nextHeight / 2.0)),
            std::max(2, static_cast<int>(std::lround(nextWidth))),
            std::max(2, static_cast<int>(std::lround(nextHeight))));
        const QRect bounds(0, 0, std::max(1, width()), std::max(1, height()));
        scaled = scaled.intersected(bounds);
        if (scaled.width() < 2 || scaled.height() < 2) {
            event->accept();
            return;
        }

        _selectionOrigin = scaled.topLeft();
        _selectionRect = QRect(_selectionOrigin, scaled.bottomRight());
        update();
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
        if (_host) _host->cancelQueuedRenders();
        event->accept();
        return;
    }

    if (_host && _host->matchesShortcut("cycleMode", event)) {
        _host->cycleNavMode();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Space) {
        _spacePan = true;
        if (_host) {
            _host->setDisplayedNavModeOverride(NavMode::pan);
        }
        _updateCursor();
        return;
    }

    if (_host && _host->matchesShortcut("overlay", event)) {
        toggleMinimalUI();
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

    if (_host && _host->matchesShortcut("home", event)) {
        _host->applyHomeView();
        event->accept();
        return;
    }

    if (_host && _host->matchesShortcut("cycleGrid", event)) {
        cycleGridMode();
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
    if (event->key() != Qt::Key_Space) {
        QWidget::keyReleaseEvent(event);
        return;
    }

    _spacePan = false;
    if (_panning && _panButton == Qt::NoButton) {
        _endPanInteraction();
        return;
    }

    if (_host && !_panning) {
        _host->setDisplayedNavModeOverride(std::nullopt);
    }
    _updateCursor();
}

void ViewportWindow::enterEvent(QEnterEvent *) {
    _updateCursor();
}

void ViewportWindow::leaveEvent(QEvent *) {
    if (_host) {
        _host->clearMouseCoords();
    }
}

void ViewportWindow::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (_fullscreenTransitionPending) {
        _fullscreenResizeTimer.start();
        return;
    }

    if (!_host || isFullScreen() || !event->spontaneous()) {
        return;
    }
    
    emit viewportScaleAdjustmentRequested(event->size());
}

bool ViewportWindow::nativeEvent(
    const QByteArray &eventType,
    void *message, qintptr *result
) {
    Q_UNUSED(eventType);
    const bool handled = QWidget::nativeEvent(eventType, message, result);
    if (!message || !result || isFullScreen()) {
        return handled;
    }

    MSG *msg = static_cast<MSG *>(message);
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

    if (msg->message == WM_SIZING && _host) {
        RECT *rect = reinterpret_cast<RECT *>(msg->lParam);
        if (!rect) {
            return handled;
        }

        int nonClientWidth = 0;
        int nonClientHeight = 0;
        if (const HWND hwnd = reinterpret_cast<HWND>(winId())) {
            RECT windowRect{};
            RECT clientRect{};
            if (GetWindowRect(hwnd, &windowRect) && GetClientRect(hwnd, &clientRect)) {
                nonClientWidth = static_cast<int>(
                    (windowRect.right - windowRect.left)
                    - (clientRect.right - clientRect.left));
                nonClientHeight = static_cast<int>(
                    (windowRect.bottom - windowRect.top)
                    - (clientRect.bottom - clientRect.top));
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
                return true;
        }

        *result = TRUE;
        return true;
    }

    if (msg->message == WM_NCLBUTTONDOWN) {
        switch (msg->wParam) {
            case HTLEFT:
            case HTRIGHT:
            case HTTOP:
            case HTBOTTOM:
                *result = 0;
                return true;
            default:
                break;
        }
    }

    if (msg->message == WM_SYSCOMMAND) {
        if ((msg->wParam & 0xFFF0) == SC_SIZE) {
            const WPARAM direction = (msg->wParam & 0x000F);
            if (direction == WMSZ_LEFT || direction == WMSZ_RIGHT
                || direction == WMSZ_TOP || direction == WMSZ_BOTTOM) {
                *result = 0;
                return true;
            }
        }
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
    event->accept();
}

void ViewportWindow::cycleGridMode() {
    int idx = 0;
    while (idx < static_cast<int>(Constants::gridModes.size())
        && Constants::gridModes[idx] != _gridDivisions) {
        idx++;
    }
    idx = (idx + 1) % static_cast<int>(Constants::gridModes.size());

    _gridDivisions = Constants::gridModes[idx];
    presentFrame(_presentedViewState);
}

void ViewportWindow::toggleMinimalUI() {
    _minimalUI = !_minimalUI;
    _updateCursor();
    presentFrame(_presentedViewState);
}

void ViewportWindow::toggleFullscreen() {
    if (!_host) return;

    _host->prepareViewportFullscreenTransition();

    if (!isFullScreen()) {
        _restoreGeometry = geometry();
        _restoreMaximized = isMaximized();
        _restoreOutputSize = _host->outputSize();
        _fullscreenManaged = true;
        _fullscreenTransitionPending = true;
        setMinimumSize(1, 1);
        setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        showFullScreen();
        raise();
        activateWindow();
        setFocus(Qt::ActiveWindowFocusReason);
        _fullscreenResizeTimer.start();
        return;
    }

    _fullscreenManaged = false;
    _fullscreenTransitionPending = true;
    if (_restoreMaximized) {
        showMaximized();
    } else {
        showNormal();
        if (_restoreGeometry.isValid()) {
            setGeometry(_restoreGeometry);
        }
    }
    _fullscreenResizeTimer.start();
}

void ViewportWindow::finalizeFullscreenTransition() {
    if (!_host || !_fullscreenTransitionPending) return;

    _fullscreenTransitionPending = false;
    if (isFullScreen()) {
        const double dpr = devicePixelRatioF();
        const QSize logicalSize = size();
        _host->applyViewportOutputSize(QSize(
            std::max(
                1, static_cast<int>(std::lround(logicalSize.width() * dpr))),
            std::max(
                1, static_cast<int>(std::lround(logicalSize.height() * dpr)))));
        return;
    }

    if (_restoreOutputSize.isValid()) {
        _host->applyViewportOutputSize(_restoreOutputSize);
    }
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
    _zoomOutPreviewScale = 1.0;
}

void ViewportWindow::_stopRealtimeZoom() {
    _rtZoomTimer.stop();
    _rtZoomLastStepAt = {};
}

bool ViewportWindow::_canBeginPanInteraction() const {
    return !_panning && !_rtZoomTimer.isActive() && _selectionRect.isNull()
        && !_zoomOutDragActive;
}

void ViewportWindow::_beginPanInteraction(Qt::MouseButton button) {
    _panning = true;
    _panButton = button;
    if (_host) {
        _host->setDisplayedNavModeOverride(NavMode::pan);
    }
    _dragOrigin = _lastMousePos;
    _panOffset = {};
    _selectionRect = {};
    _resetZoomOutDragState();
    _stopRealtimeZoom();
    _panRedrawTimer.stop();
    _zoomOutRedrawTimer.stop();
    _syncPanRedrawTimerInterval();
    _panRedrawTimer.start();
    grabMouse();
    _updateCursor();
    update();
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
    update();
}

void ViewportWindow::_beginRealtimeZoom(bool zoomIn) {
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
    if (_zoomOutDragActive || !_selectionRect.isNull()
        || _rtZoomTimer.isActive()) {
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
    if (_zoomOutDragActive || !_selectionRect.isNull() || _rtZoomTimer.isActive()) {
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
            static_cast<int>(std::lround(
                static_cast<double>(step)
                * Constants::boostedPanSpeedFactor)));
    }
    _host->panByPixels(QPoint(xDir * step, yDir * step));
}

void ViewportWindow::_drawGrid(QPainter &painter) {
    if (_gridDivisions <= 1) return;

    const QRect area = rect();
    if (area.width() <= 1 || area.height() <= 1) return;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(QPen(QColor(255, 255, 255, 110), 1.0));

    for (int i = 1; i < _gridDivisions; i++) {
        const int x = static_cast<int>(std::lround(
            static_cast<double>(area.width()) * i / _gridDivisions));
        const int y = static_cast<int>(std::lround(
            static_cast<double>(area.height()) * i / _gridDivisions));
        painter.drawLine(x, area.top(), x, area.bottom());
        painter.drawLine(area.left(), y, area.right(), y);
    }

    painter.restore();
}

void ViewportWindow::_updateWindowTitle() {
    if (!_host) {
        setWindowTitle(tr("Viewport"));
        return;
    }

    setWindowTitle(_usePreviewFallback() ? tr("Viewport (Preview)")
        : tr("Viewport (Direct)"));
}

QRect ViewportWindow::_statusOverlayRect() const {
    if (_minimalUI || !_host) {
        return {};
    }

    const QString statusText = _host->viewportStatusText();
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
    const QRect textBounds = metrics.boundingRect(
        textRect, Qt::AlignTop | Qt::AlignLeft, statusText);
    return QRect(overlayMargin, overlayMargin, overlayWidth,
        textBounds.height() + overlayPaddingY * 2);
}

QPoint ViewportWindow::_clampToOutputPixel(const QPointF &pixel) const {
    const QSize outputSize = _host->outputSize();
    return { std::clamp(static_cast<int>(std::lround(pixel.x())), 0,
                 std::max(0, outputSize.width() - 1)),
        std::clamp(static_cast<int>(std::lround(pixel.y())), 0,
            std::max(0, outputSize.height() - 1)) };
}

QPoint ViewportWindow::_mapToOutputPixel(const QPoint &logicalPoint) const {
    return _mapToOutputPixelRaw(logicalPoint);
}

QPoint ViewportWindow::_mapToOutputPixelRaw(const QPoint &logicalPoint) const {
    return _mapToOutputPixelRaw(QPointF(logicalPoint));
}

QPoint ViewportWindow::_mapToOutputPixelRaw(const QPointF &logicalPoint) const {
    const QSize logicalSize = size();
    const QSize outputSize = _host->outputSize();
    if (logicalSize.width() <= 0 || logicalSize.height() <= 0) {
        return {};
    }

    const double x = static_cast<double>(logicalPoint.x()) * outputSize.width()
        / std::max(1, logicalSize.width());
    const double y = static_cast<double>(logicalPoint.y()) * outputSize.height()
        / std::max(1, logicalSize.height());

    return { std::clamp(static_cast<int>(std::lround(x)), 0,
                 std::max(0, outputSize.width() - 1)),
        std::clamp(static_cast<int>(std::lround(y)), 0,
            std::max(0, outputSize.height() - 1)) };
}

QPoint ViewportWindow::_mapToOutputPixel(const QPointF &logicalPoint) const {
    return _mapToOutputPixel(logicalPoint.toPoint());
}

QPoint ViewportWindow::_mapToOutputDelta(const QPoint &logicalDelta) const {
    const QSize logicalSize = size();
    const QSize outputSize = _host->outputSize();
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
        : _host->navMode();
}

void ViewportWindow::_updateCursor() {
    if (_minimalUI) {
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
    if (!_host || _panOffset.isNull()) return;

    const QPoint delta = _mapToOutputDelta(_panOffset);
    if (delta.isNull()) return;

    _host->panByPixels(delta);
    _dragOrigin = _lastMousePos;
    _panOffset = {};
}

void ViewportWindow::_commitZoomOutPreview() {
    if (!_host) return;
    if (!_zoomOutPendingCommit
        || NumberUtil::almostEqual(_zoomOutPreviewScale, 1.0)) {
        return;
    }

    const QPoint viewportCenter(width() / 2, height() / 2);
    const double scaleMultiplier = _zoomOutPreviewScale;
    _zoomOutPreviewScale = 1.0;
    _zoomOutPendingCommit = false;
    _host->scaleAtPixel(_mapToOutputPixel(viewportCenter), scaleMultiplier);
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

    _host->scaleAtPixel(
        _clampToOutputPixel(*_rtZoomAnchorPixel), scaleMultiplier);
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

bool ViewportWindow::_usePreviewFallback() const {
    return _host && _host->shouldUseInteractionPreviewFallback();
}
