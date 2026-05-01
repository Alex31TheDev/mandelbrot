#pragma once

#include <optional>

#include <QObject>
#include <QString>

#include "app/GUITypes.h"
#include "app/GUISessionState.h"
#include "runtime/RenderController.h"
#include "settings/Shortcuts.h"
#include "windows/viewport/ViewportHost.h"

class ViewportWindow;

class ViewportController final : public QObject, public ViewportHost {
    Q_OBJECT

public:
    ViewportController(
        GUISessionState &sessionState, RenderController &renderController,
        Shortcuts &shortcuts, QObject *parent = nullptr
    );

    void attachViewport(ViewportWindow *viewport);

    [[nodiscard]] NavMode displayedNavMode() const;
    [[nodiscard]] SelectionTarget selectionTarget() const {
        return _selectionTarget;
    }

    void setNavMode(NavMode mode);
    void setSelectionTarget(SelectionTarget target);

public slots:
    void cycleGridMode();
    void toggleMinimalUI();
    void toggleFullscreen();

signals:
    void sessionStateChanged();
    void renderRequested();
    void renderRequestedWithPickAction(int pickTarget, const QPoint &pixel);
    void quickSaveRequested();
    void statusMessageChanged(const QString &message);

public:
    void applyHomeView() override;
    void scaleAtPixel(const QPoint &pixel, double scaleMultiplier) override;
    void zoomAtPixel(const QPoint &pixel, bool zoomIn) override;
    void boxZoom(const QRect &selectionRect) override;
    void panByPixels(const QPoint &delta) override;
    void pickAtPixel(const QPoint &pixel) override;
    void updateMouseCoords(const QPoint &pixel) override;
    void clearMouseCoords() override;
    void adjustIterationsBy(int delta) override;
    void cycleNavMode() override;
    void cancelQueuedRenders() override;
    void quickSaveImage() override;
    void setDisplayedNavModeOverride(std::optional<NavMode> mode) override;
    void prepareViewportFullscreenTransition() override;
    void applyViewportOutputSize(const QSize &outputSize) override;

    [[nodiscard]] NavMode navMode() const override { return _navMode; }
    [[nodiscard]] bool viewportUsesDirectPick() const override {
        return _selectionTarget != SelectionTarget::zoomPoint;
    }
    [[nodiscard]] bool renderInFlight() const override {
        return _renderController.renderInFlight();
    }
    [[nodiscard]] QSize outputSize() const override {
        return _sessionState.outputSize();
    }
    [[nodiscard]] const QImage &previewImage() const override {
        return _renderController.previewImage();
    }
    [[nodiscard]] bool previewUsesBackendMemory() const override {
        return _renderController.previewUsesBackendMemory();
    }
    [[nodiscard]] bool hasDisplayedViewState() const override {
        return _renderController.hasDisplayedViewState();
    }
    [[nodiscard]] ViewTextState currentViewTextState() const override {
        return _sessionState.currentViewTextState();
    }
    [[nodiscard]] ViewTextState displayedViewTextState() const override {
        return _renderController.displayedViewTextState();
    }
    bool previewPannedViewState(
        const QPoint &delta, ViewTextState &view, QString &errorMessage
    ) override;
    bool previewScaledViewState(const QPoint &pixel, double scaleMultiplier,
        ViewTextState &view, QString &errorMessage) override;
    bool previewBoxZoomViewState(
        const QRect &selectionRect, ViewTextState &view, QString &errorMessage
    ) override;
    bool mapViewPixelToViewPixel(
        const ViewTextState &sourceView, const ViewTextState &targetView,
        const QPoint &pixel, QPointF &mappedPixel, QString &errorMessage
    ) override;
    [[nodiscard]] QString viewportStatusText() const override;
    [[nodiscard]] int interactionFrameIntervalMs() const override;
    [[nodiscard]] bool shouldUseInteractionPreviewFallback() const override {
        return _renderController.shouldUseInteractionPreviewFallback();
    }
    [[nodiscard]] bool matchesShortcut(
        const QString &id, const QKeyEvent *event
    ) const override;
    [[nodiscard]] int panRateValue() const override;
    [[nodiscard]] double panRateFactor() const override;
    [[nodiscard]] double zoomRateFactor() const override;

private:
    GUISessionState &_sessionState;
    RenderController &_renderController;
    Shortcuts &_shortcuts;
    ViewportWindow *_viewport = nullptr;
    NavMode _navMode = NavMode::realtimeZoom;
    std::optional<NavMode> _displayedNavModeOverride;
    SelectionTarget _selectionTarget = SelectionTarget::zoomPoint;
    QString _mouseText;

    [[nodiscard]] QPoint _clampPixelToOutput(const QPoint &pixel) const;
    [[nodiscard]] double _currentZoomFactor(int sliderValue) const;
    [[nodiscard]] GUIRenderSnapshot _snapshot() const;
};
