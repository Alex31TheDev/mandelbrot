#pragma once

#include <functional>
#include <optional>

#include <QImage>
#include <QKeyEvent>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QSize>
#include <QString>

#include "app/GUITypes.h"

class ViewportHost {
public:
    virtual ~ViewportHost() = default;

    virtual void applyHomeView() = 0;
    virtual void scaleAtPixel(const QPoint &pixel, double scaleMultiplier) = 0;
    virtual void zoomAtPixel(const QPoint &pixel, bool zoomIn) = 0;
    virtual void boxZoom(const QRect &selectionRect) = 0;
    virtual void panByPixels(const QPoint &delta) = 0;
    virtual void pickAtPixel(const QPoint &pixel) = 0;
    virtual void updateMouseCoords(const QPoint &pixel) = 0;
    virtual void clearMouseCoords() = 0;
    virtual void adjustIterationsBy(int delta) = 0;
    virtual void cycleNavMode() = 0;
    virtual void cancelQueuedRenders() = 0;
    virtual void quickSaveImage() = 0;
    virtual void setDisplayedNavModeOverride(std::optional<GUI::NavMode> mode) = 0;
    virtual void prepareViewportFullscreenTransition() = 0;
    virtual void applyViewportOutputSize(const QSize &outputSize) = 0;
    [[nodiscard]] virtual QSize constrainViewportSize(
        const QSize &requestedOuterSize, const QSize &nonClientSize
    ) const = 0;

    [[nodiscard]] virtual GUI::NavMode navMode() const = 0;
    [[nodiscard]] virtual bool viewportUsesDirectPick() const = 0;
    [[nodiscard]] virtual bool renderInFlight() const = 0;
    [[nodiscard]] virtual QSize outputSize() const = 0;
    virtual bool withPreviewImage(
        const std::function<void(const QImage &)> &visitor
    ) const = 0;
    [[nodiscard]] virtual bool hasDisplayedViewState() const = 0;
    [[nodiscard]] virtual GUI::ViewTextState currentViewTextState() const = 0;
    virtual bool previewPannedViewState(
        const QPoint &delta, GUI::ViewTextState &view, QString &errorMessage
    ) = 0;
    virtual bool previewScaledViewState(
        const QPoint &pixel, double scaleMultiplier,
        GUI::ViewTextState &view, QString &errorMessage
    ) = 0;
    virtual bool previewBoxZoomViewState(
        const QRect &selectionRect, GUI::ViewTextState &view,
        QString &errorMessage
    ) = 0;
    virtual bool mapViewPixelToViewPixel(const GUI::ViewTextState &sourceView,
        const GUI::ViewTextState &targetView,
        const QPoint &pixel, QPointF &mappedPixel, QString &errorMessage) = 0;
    [[nodiscard]] virtual QString viewportStatusText() const = 0;
    [[nodiscard]] virtual int interactionFrameIntervalMs() const = 0;
    [[nodiscard]] virtual bool shouldUseInteractionPreviewFallback() const = 0;
    [[nodiscard]] virtual bool matchesShortcut(
        const QString &id, const QKeyEvent *event
    ) const = 0;
    [[nodiscard]] virtual int panRateValue() const = 0;
    [[nodiscard]] virtual double panRateFactor() const = 0;
    [[nodiscard]] virtual double zoomRateFactor() const = 0;
};
