#pragma once

#include <functional>
#include <optional>

#include <QObject>
#include <QString>

#include "runtime/RenderController.h"
#include "settings/Shortcuts.h"
#include "windows/viewport/ViewportHost.h"

#include "app/GUITypes.h"
#include "app/GUISessionState.h"

class ViewportWindow;

class ViewportController final : public QObject, public ViewportHost {
    Q_OBJECT

public:
    ViewportController(
        GUISessionState &sessionState, RenderController &renderController,
        Shortcuts &shortcuts, QObject *parent = nullptr
    );

    void attachViewport(ViewportWindow *viewport);

    [[nodiscard]] GUI::NavMode displayedNavMode() const;
    [[nodiscard]] GUI::SelectionTarget selectionTarget() const {
        return _selectionTarget;
    }

    void setNavMode(GUI::NavMode mode);
    void setSelectionTarget(GUI::SelectionTarget target);

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
    void setDisplayedNavModeOverride(std::optional<GUI::NavMode> mode) override;
    void prepareViewportFullscreenTransition() override;
    void applyViewportOutputSize(const QSize &outputSize) override;
    void resizeViewportForScalePercent(float scalePercent) const;
    [[nodiscard]] float viewportScalePercentForLogicalSize(
        const QSize &logicalSize
    ) const;
    [[nodiscard]] QSize constrainViewportSize(
        const QSize &requestedOuterSize, const QSize &nonClientSize
    ) const override;

    [[nodiscard]] GUI::NavMode navMode() const override { return _navMode; }
    [[nodiscard]] bool viewportUsesDirectPick() const override {
        return _selectionTarget != GUI::SelectionTarget::zoomPoint;
    }
    [[nodiscard]] bool renderInFlight() const override {
        return _renderController.renderInFlight();
    }
    [[nodiscard]] QSize outputSize() const override {
        return _sessionState.outputSize();
    }
    bool withPreviewImage(
        const std::function<void(const QImage &)> &visitor
    ) const override {
        return _renderController.withPreviewImage(visitor);
    }
    [[nodiscard]] bool hasDisplayedViewState() const override {
        return _renderController.hasDisplayedViewState();
    }
    [[nodiscard]] GUI::ViewTextState currentViewTextState() const override {
        return _sessionState.currentViewTextState();
    }
    bool previewPannedViewState(
        const QPoint &delta, GUI::ViewTextState &view, QString &errorMessage
    ) override;
    bool previewScaledViewState(const QPoint &pixel, double scaleMultiplier,
        GUI::ViewTextState &view, QString &errorMessage) override;
    bool previewBoxZoomViewState(
        const QRect &selectionRect, GUI::ViewTextState &view,
        QString &errorMessage
    ) override;
    bool mapViewPixelToViewPixel(
        const GUI::ViewTextState &sourceView,
        const GUI::ViewTextState &targetView,
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
    GUI::NavMode _navMode = GUI::NavMode::realtimeZoom;
    std::optional<GUI::NavMode> _displayedNavModeOverride;
    GUI::SelectionTarget _selectionTarget = GUI::SelectionTarget::zoomPoint;
    QString _mouseText;

    [[nodiscard]] QPoint _clampPixelToOutput(const QPoint &pixel) const;
    [[nodiscard]] double _currentZoomFactor(int sliderValue) const;
    [[nodiscard]] GUI::GUIRenderSnapshot _snapshot() const;
};
