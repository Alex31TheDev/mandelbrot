#pragma once

#include <chrono>
#include <memory>
#include <optional>

#include <QCloseEvent>
#include <QEnterEvent>
#include <QEvent>
#include <QHideEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QMoveEvent>
#include <QPoint>
#include <QRect>
#include <QResizeEvent>
#include <QSize>
#include <QShowEvent>
#include <QTimer>
#include <QWheelEvent>
#include <QWidget>

#include "ui_ViewportWindow.h"

#include "app/GUITypes.h"

#include "widgets/D3DWidget.h"

#include "ViewportHost.h"
#include "ViewportOverlayWindow.h"

class ViewportWindow final : public QWidget {
    Q_OBJECT

public:
    explicit ViewportWindow(ViewportHost *host);
    ~ViewportWindow() override;

    void clearPreviewOffset();
    void presentFrame(const GUI::PresentedFrame &frame);
    void refreshPreviewTransform();
    void refreshOverlay();
    void cycleGridMode();
    void toggleMinimalUI();
    void toggleFullscreen();
    void finalizeFullscreenTransition();
    [[nodiscard]] bool previewTransformRefreshDeferred() const {
        return _deferPreviewTransformRefresh;
    }
    void setCloseAllowed(bool allowed) { _closeAllowed = allowed; }

signals:
    void closeRequested(bool skipDirtyViewPrompt);
    void viewportScaleAdjustmentRequested(const QSize &logicalSize);

protected:
    bool event(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;
    void moveEvent(QMoveEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    bool nativeEvent(const QByteArray &eventType,
        void *message, qintptr *result) override;

private:
    std::unique_ptr<Ui::ViewportWindow> _ui;
    ViewportHost *_host = nullptr;
    D3DWidget *_d3dWidget = nullptr;
    ViewportOverlayWindow *_overlayWindow = nullptr;
    QTimer _rtZoomTimer;
    QTimer _panRedrawTimer;
    QTimer _zoomOutRedrawTimer;
    QTimer _arrowPanTimer;
    QPoint _lastMousePos;
    QPoint _dragOrigin;
    std::optional<QPointF> _rtZoomAnchorPixel;
    std::chrono::steady_clock::time_point _rtZoomLastStepAt{};
    bool _rtZoomZoomIn = true;
    bool _panning = false;
    bool _spacePan = false;
    Qt::MouseButton _panButton = Qt::NoButton;
    bool _zoomOutPending = false;
    bool _zoomOutDragActive = false;
    bool _zoomOutDragMoved = false;
    bool _zoomOutPendingCommit = false;
    bool _arrowPanLeft = false;
    bool _arrowPanRight = false;
    bool _arrowPanUp = false;
    bool _arrowPanDown = false;
    QPoint _zoomOutDragLastPos;
    bool _fullscreenTransitionPending = false;
    bool _closeAllowed = false;
    bool _restoreMaximized = false;
    bool _deferPreviewTransformRefresh = false;
    QRect _restoreGeometry;
    QSize _restoreOutputSize;
    QTimer _fullscreenResizeTimer;

    [[nodiscard]] bool _precisionModifierActive() const;
    [[nodiscard]] bool _speedModifierActive() const;
    [[nodiscard]] int _baseInteractionTickIntervalMs() const;
    [[nodiscard]] int _precisionInteractionTickIntervalMs() const;
    void _syncPanRedrawTimerInterval();
    void _syncRealtimeZoomTimerInterval();
    void _syncArrowPanTimerInterval();
    void _refreshActiveInteractionTimers();
    void _resetZoomOutDragState();
    void _stopRealtimeZoom();
    [[nodiscard]] bool _canBeginPanInteraction() const;
    void _beginPanInteraction(Qt::MouseButton button);
    void _endPanInteraction();
    void _beginRealtimeZoom(bool zoomIn);
    bool _handleArrowPanKeyPress(QKeyEvent *event);
    bool _handleArrowPanKeyRelease(QKeyEvent *event);
    void _applyArrowPanStep();
    void _syncOverlayWindow();
    void _hideOverlayWindow();
    void _syncPresentationSize();
    void _updateWindowTitle();
    [[nodiscard]] QRect _viewportClientGlobalRect() const;
    [[nodiscard]] QPoint _clampToOutputPixel(const QPointF &pixel) const;
    [[nodiscard]] QPoint _mapToOutputPixel(const QPoint &logicalPoint) const;
    [[nodiscard]] QPoint _mapToOutputPixel(const QPointF &logicalPoint) const;
    [[nodiscard]] QPoint _mapToOutputDelta(const QPoint &logicalDelta) const;
    [[nodiscard]] QRect _mapToOutputRect(const QRect &logicalRect) const;
    [[nodiscard]] GUI::NavMode _effectiveMode() const;
    void _updateCursor();
    void _commitPanOffset();
    void _commitZoomOutPreview();
    void _applyRealtimeZoomStep(bool firstStep);
    void _updateRealtimeZoomAnchor(double elapsedSeconds);
};
