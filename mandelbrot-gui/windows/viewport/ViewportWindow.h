#pragma once

#include <chrono>
#include <memory>
#include <optional>

#include <QCloseEvent>
#include <QEnterEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QResizeEvent>
#include <QSize>
#include <QTimer>
#include <QWidget>
#include <QWheelEvent>

#include "ViewportHost.h"

#include "app/GUITypes.h"

namespace Ui {
    class ViewportWindow;
}

class ViewportWindow final : public QWidget {
    Q_OBJECT

public:
    struct PreviewTransform {
        QPointF center;
        QPointF translation;
        double scaleX = 1.0;
        double scaleY = 1.0;
    };

    explicit ViewportWindow(ViewportHost *host);
    ~ViewportWindow() override;

    void clearPreviewOffset();
    void cycleGridMode();
    void toggleMinimalUI();
    void toggleFullscreen();
    void finalizeFullscreenTransition();
    void setCloseAllowed(bool allowed) { _closeAllowed = allowed; }
    [[nodiscard]] GUI::ViewTextState displayedPreviewView() const;
    [[nodiscard]] GUI::ViewTextState targetPreviewView() const;
    [[nodiscard]] std::optional<PreviewTransform> previewTransform() const;

signals:
    void closeRequested(bool skipDirtyViewPrompt);
    void viewportScaleAdjustmentRequested(const QSize &logicalSize);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    bool nativeEvent(const QByteArray &eventType,
        void *message, qintptr *result) override;

private:
    std::unique_ptr<Ui::ViewportWindow> _ui;
    ViewportHost *_host = nullptr;
    QTimer _rtZoomTimer;
    QTimer _panRedrawTimer;
    QTimer _zoomOutRedrawTimer;
    QTimer _arrowPanTimer;
    QPoint _lastMousePos;
    QPoint _dragOrigin;
    QPoint _selectionOrigin;
    QPoint _panOffset;
    QRect _selectionRect;
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
    double _zoomOutPreviewScale = 1.0;
    int _gridDivisions = 0;
    bool _minimalUI = false;
    bool _fullscreenManaged = false;
    bool _fullscreenTransitionPending = false;
    bool _closeAllowed = false;
    bool _restoreMaximized = false;
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
    void _drawGrid(QPainter &painter);
    void _updateWindowTitle();
    QPoint _clampToOutputPixel(const QPointF &pixel) const;
    QPoint _mapToOutputPixel(const QPoint &logicalPoint) const;
    QPoint _mapToOutputPixelRaw(const QPoint &logicalPoint) const;
    QPoint _mapToOutputPixelRaw(const QPointF &logicalPoint) const;
    QPoint _mapToOutputPixel(const QPointF &logicalPoint) const;
    QPoint _mapToOutputDelta(const QPoint &logicalDelta) const;
    QRect _mapToOutputRect(const QRect &logicalRect) const;
    GUI::NavMode _effectiveMode() const;
    void _updateCursor();
    void _commitPanOffset();
    void _commitZoomOutPreview();
    void _applyRealtimeZoomStep(bool firstStep);
    void _updateRealtimeZoomAnchor(double elapsedSeconds);
    [[nodiscard]] bool _usePreviewFallback() const;
};
