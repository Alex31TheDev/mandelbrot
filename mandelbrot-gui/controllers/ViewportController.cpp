#include "ViewportController.h"

#include <algorithm>
#include <cmath>

#include "windows/viewport/ViewportWindow.h"

#include "util/GUIUtil.h"
#include "util/NumberUtil.h"

using namespace GUI;

static QSize nonClientSizeForViewport(const ViewportWindow *viewport) {
    if (!viewport) return {};

    const QSize clientSize = viewport->size();
    const QRect frame = viewport->frameGeometry();
    return QSize(std::max(0, frame.width() - clientSize.width()),
        std::max(0, frame.height() - clientSize.height()));
}

static QSize clientSizeFromOuter(
    const QSize &outerSize,
    const QSize &nonClientSize
) {
    return QSize(std::max(1, outerSize.width() - std::max(0, nonClientSize.width())),
        std::max(1, outerSize.height() - std::max(0, nonClientSize.height())));
}

ViewportController::ViewportController(
    GUISessionState &sessionState, RenderController &renderController,
    Shortcuts &shortcuts, QObject *parent
)
    : QObject(parent)
    , _sessionState(sessionState)
    , _renderController(renderController)
    , _shortcuts(shortcuts)
    , _mouseText(tr("Mouse: -")) {}

void ViewportController::attachViewport(ViewportWindow *viewport) {
    _viewport = viewport;
}

NavMode ViewportController::displayedNavMode() const {
    return _displayedNavModeOverride.value_or(_navMode);
}

void ViewportController::setNavMode(NavMode mode) {
    if (_navMode == mode) return;
    _navMode = mode;
    emit sessionStateChanged();
}

void ViewportController::setSelectionTarget(SelectionTarget target) {
    if (_selectionTarget == target) return;
    _selectionTarget = target;
    emit sessionStateChanged();
}

void ViewportController::cycleGridMode() {
    if (_viewport) _viewport->cycleGridMode();
}

void ViewportController::toggleMinimalUI() {
    if (_viewport) _viewport->toggleMinimalUI();
}

void ViewportController::toggleFullscreen() {
    if (_viewport) _viewport->toggleFullscreen();
}

void ViewportController::applyHomeView() {
    _renderController.markPreviewMotion();
    _sessionState.applyHomeView();
    emit sessionStateChanged();
    emit renderRequested();
}

void ViewportController::scaleAtPixel(
    const QPoint &pixel, double scaleMultiplier
) {
    if (!(scaleMultiplier > 0.0) || !std::isfinite(scaleMultiplier)) return;

    ViewTextState nextView;
    QString error;
    if (!_renderController.zoomViewAtPixel(
        _snapshot(), _clampPixelToOutput(pixel), scaleMultiplier, nextView,
        error)) {
        emit statusMessageChanged(error);
        return;
    }
    if (NumberUtil::equalParsedDoubleText(
        _sessionState.zoomText().toStdString(),
        nextView.zoomText.toStdString())
        && _sessionState.pointRealText() == nextView.pointReal
        && _sessionState.pointImagText() == nextView.pointImag) {
        return;
    }

    _sessionState.setPointTexts(nextView.pointReal, nextView.pointImag);
    _sessionState.setZoomText(nextView.zoomText);
    _sessionState.syncStatePointFromText();
    _sessionState.syncStateZoomFromText();
    _renderController.markPreviewMotion();
    emit sessionStateChanged();
    emit renderRequested();
}

void ViewportController::zoomAtPixel(const QPoint &pixel, bool zoomIn) {
    const double factor = _currentZoomFactor(_sessionState.state().zoomRate);
    const double scaleMultiplier = zoomIn ? (1.0 / factor) : factor;
    scaleAtPixel(_clampPixelToOutput(pixel), scaleMultiplier);
}

void ViewportController::boxZoom(const QRect &selectionRect) {
    const QRect rect = selectionRect.normalized();
    if (rect.width() < 2 || rect.height() < 2) {
        zoomAtPixel(rect.center(), true);
        return;
    }

    ViewTextState nextView;
    QString error;
    if (!_renderController.boxZoomView(_snapshot(), rect, nextView, error)) {
        emit statusMessageChanged(error);
        return;
    }

    _sessionState.setPointTexts(nextView.pointReal, nextView.pointImag);
    _sessionState.setZoomText(nextView.zoomText);
    _sessionState.syncStatePointFromText();
    _sessionState.syncStateZoomFromText();
    _renderController.markPreviewMotion();
    emit sessionStateChanged();
    emit renderRequested();
}

void ViewportController::panByPixels(const QPoint &delta) {
    if (delta.isNull()) return;

    QString pointReal;
    QString pointImag;
    QString error;
    if (!_renderController.panPointByDelta(
        _snapshot(), delta, pointReal, pointImag, error)) {
        emit statusMessageChanged(error);
        return;
    }

    _sessionState.setPointTexts(pointReal, pointImag);
    _sessionState.syncStatePointFromText();
    _renderController.markPreviewMotion();
    emit sessionStateChanged();
    emit renderRequested();
}

void ViewportController::pickAtPixel(const QPoint &pixel) {
    const QPoint clampedPixel = _clampPixelToOutput(pixel);
    QString real;
    QString imag;
    QString error;
    if (!_renderController.pointAtPixel(
        _snapshot(), clampedPixel, real, imag, error)) {
        emit statusMessageChanged(error);
        return;
    }

    switch (_selectionTarget) {
        case SelectionTarget::zoomPoint:
            _sessionState.setPointTexts(real, imag);
            _sessionState.syncStatePointFromText();
            break;
        case SelectionTarget::seedPoint:
            _sessionState.setSeedTexts(real, imag);
            _sessionState.syncStateSeedFromText();
            break;
        case SelectionTarget::lightPoint:
        {
            bool okReal = false;
            bool okImag = false;
            const double lightReal = real.toDouble(&okReal);
            const double lightImag = imag.toDouble(&okImag);
            if (okReal) _sessionState.mutableState().light.setX(lightReal);
            if (okImag) _sessionState.mutableState().light.setY(lightImag);
            break;
        }
    }

    emit sessionStateChanged();
    if (_selectionTarget == SelectionTarget::zoomPoint) {
        emit renderRequested();
    } else {
        emit renderRequestedWithPickAction(
            static_cast<int>(_selectionTarget), clampedPixel);
    }
}

void ViewportController::updateMouseCoords(const QPoint &pixel) {
    const QPoint clampedPixel = _clampPixelToOutput(pixel);
    QString real;
    QString imag;
    QString error;
    if (!_renderController.pointAtPixel(_snapshot(), clampedPixel, real, imag, error)) {
        _mouseText = tr("Mouse: %1, %2  |  -")
            .arg(clampedPixel.x())
            .arg(clampedPixel.y());
    } else {
        _mouseText = tr("Mouse: %1, %2  |  %3  %4")
            .arg(clampedPixel.x())
            .arg(clampedPixel.y())
            .arg(real, imag);
    }

    if (_viewport) _viewport->update();
}

void ViewportController::clearMouseCoords() {
    _mouseText = tr("Mouse: -");
    if (_viewport) _viewport->update();
}

void ViewportController::adjustIterationsBy(int delta) {
    _sessionState.mutableState().iterations
        = std::max(0, _sessionState.state().iterations + delta);
    emit sessionStateChanged();
    emit renderRequested();
}

void ViewportController::cycleNavMode() {
    setNavMode(static_cast<NavMode>((static_cast<int>(_navMode) + 1) % 3));
}

void ViewportController::cancelQueuedRenders() {
    _renderController.cancelQueuedRenders();
    if (_viewport) _viewport->clearPreviewOffset();
}

void ViewportController::quickSaveImage() {
    emit quickSaveRequested();
}

void ViewportController::setDisplayedNavModeOverride(
    std::optional<NavMode> mode
) {
    if (_displayedNavModeOverride == mode) return;
    _displayedNavModeOverride = mode;
    emit sessionStateChanged();
}

void ViewportController::prepareViewportFullscreenTransition() {
    _renderController.freezePreview();
    if (_viewport) _viewport->clearPreviewOffset();
}

void ViewportController::resizeViewportForScalePercent(float scalePercent) const {
    if (!_viewport || _viewport->isFullScreen()) {
        return;
    }

    const QSize output = _sessionState.outputSize();
    if (output.width() <= 0 || output.height() <= 0) {
        return;
    }

    const float dpr = std::max(1.0f, static_cast<float>(_viewport->devicePixelRatioF()));
    const float scale = std::max(1.0f, scalePercent) / 100.0f;
    const QSize requestedClientSize(
        std::max(1,
            static_cast<int>(
                std::lroundf(static_cast<float>(output.width()) * scale / dpr))
        ),
        std::max(1,
            static_cast<int>(
                std::lroundf(static_cast<float>(output.height()) * scale / dpr))
        )
    );
    const QSize nonClientSize = nonClientSizeForViewport(_viewport);
    const QSize constrainedOuterSize = constrainViewportSize(
        QSize(requestedClientSize.width() + nonClientSize.width(),
            requestedClientSize.height() + nonClientSize.height()),
        nonClientSize);
    const QSize constrainedClientSize
        = clientSizeFromOuter(constrainedOuterSize, nonClientSize);

    _viewport->showNormal();
    _viewport->setMinimumSize(1, 1);
    _viewport->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    _viewport->resize(constrainedClientSize);
}

float ViewportController::viewportScalePercentForLogicalSize(
    const QSize &logicalSize
) const {
    const QSize output = _sessionState.outputSize();
    if (!logicalSize.isValid() || output.width() <= 0 || output.height() <= 0) {
        return std::max(1.0f, _sessionState.state().viewportScalePercent);
    }

    const float dpr = std::max(1.0f, static_cast<float>(
        _viewport ? _viewport->devicePixelRatioF() : 1.0)
    );
    const QSize nonClientSize = nonClientSizeForViewport(_viewport);
    const QSize constrainedClientSize = clientSizeFromOuter(
        constrainViewportSize(
            QSize(logicalSize.width() + nonClientSize.width(),
                logicalSize.height() + nonClientSize.height()),
            nonClientSize
        ),
        nonClientSize
    );

    const float widthScalePercent = static_cast<float>(constrainedClientSize.width())
        * dpr * 100.0f / static_cast<float>(std::max(1, output.width()));
    const float heightScalePercent = static_cast<float>(constrainedClientSize.height())
        * dpr * 100.0f / static_cast<float>(std::max(1, output.height()));
    return std::max(1.0f, std::min(widthScalePercent, heightScalePercent));
}

void ViewportController::applyViewportOutputSize(const QSize &outputSize) {
    if (!outputSize.isValid()) return;
    _renderController.markPreviewMotion();
    _sessionState.mutableState().outputWidth = outputSize.width();
    _sessionState.mutableState().outputHeight = outputSize.height();
    resizeViewportForScalePercent(_sessionState.state().viewportScalePercent);
    emit sessionStateChanged();
    emit renderRequested();
}

QSize ViewportController::constrainViewportSize(
    const QSize &requestedOuterSize, const QSize &nonClientSize
) const {
    const int requestedOuterWidth = std::max(1, requestedOuterSize.width());
    const int requestedOuterHeight = std::max(1, requestedOuterSize.height());
    const int nonClientWidth = std::max(0, nonClientSize.width());
    const int nonClientHeight = std::max(0, nonClientSize.height());

    const QSize output = _sessionState.outputSize();
    if (output.width() <= 0 || output.height() <= 0) {
        return QSize(requestedOuterWidth, requestedOuterHeight);
    }

    const double aspect = static_cast<double>(output.width())
        / static_cast<double>(output.height());
    if (!(aspect > 0.0) || !std::isfinite(aspect)) {
        return QSize(requestedOuterWidth, requestedOuterHeight);
    }

    const int requestedClientWidth
        = std::max(1, requestedOuterWidth - nonClientWidth);
    const int requestedClientHeight
        = std::max(1, requestedOuterHeight - nonClientHeight);

    int constrainedClientWidth = requestedClientWidth;
    int constrainedClientHeight = requestedClientHeight;
    if (static_cast<double>(requestedClientWidth)
        / static_cast<double>(requestedClientHeight)
    > aspect) {
        constrainedClientWidth = std::max(1,
            static_cast<int>(std::lround(
                static_cast<double>(requestedClientHeight) * aspect)
                )
        );
    } else {
        constrainedClientHeight = std::max(1,
            static_cast<int>(std::lround(
                static_cast<double>(requestedClientWidth) / aspect)
                )
        );
    }

    return QSize(std::max(1, constrainedClientWidth + nonClientWidth),
        std::max(1, constrainedClientHeight + nonClientHeight));
}

bool ViewportController::previewPannedViewState(
    const QPoint &delta, ViewTextState &view, QString &errorMessage
) {
    return _renderController.previewPannedViewState(
        _snapshot(), delta, view, errorMessage);
}

bool ViewportController::previewScaledViewState(
    const QPoint &pixel, double scaleMultiplier,
    ViewTextState &view, QString &errorMessage
) {
    return _renderController.previewScaledViewState(
        _snapshot(), _clampPixelToOutput(pixel), scaleMultiplier, view, errorMessage);
}

bool ViewportController::previewBoxZoomViewState(
    const QRect &selectionRect, ViewTextState &view, QString &errorMessage
) {
    return _renderController.previewBoxZoomViewState(
        _snapshot(), selectionRect.normalized(), view, errorMessage);
}

bool ViewportController::mapViewPixelToViewPixel(
    const ViewTextState &sourceView, const ViewTextState &targetView,
    const QPoint &pixel, QPointF &mappedPixel, QString &errorMessage
) {
    return _renderController.mapViewPixelToViewPixel(
        sourceView, targetView, pixel, mappedPixel, errorMessage);
}

QString ViewportController::viewportStatusText() const {
    QString text;
    if (!_renderController.viewportFPSText().isEmpty()) {
        text = _renderController.viewportFPSText();
        if (!_renderController.viewportRenderTimeText().isEmpty()) {
            text += tr("  |  %1").arg(_renderController.viewportRenderTimeText());
        }
    }
    if (!_mouseText.isEmpty()) {
        text = text.isEmpty() ? _mouseText : QStringLiteral("%1\n%2").arg(text, _mouseText);
    }
    return text;
}

int ViewportController::interactionFrameIntervalMs() const {
    const int fps = std::max(1,
        _sessionState.state().interactionTargetFPS > 0
        ? _sessionState.state().interactionTargetFPS
        : Constants::defaultInteractionTargetFPS);
    return std::max(
        1, static_cast<int>(std::lround(1000.0 / static_cast<double>(fps))));
}

bool ViewportController::matchesShortcut(
    const QString &id, const QKeyEvent *event
) const {
    if (!event) return false;

    const QKeySequence configured = _shortcuts.sequence(id);
    if (configured.isEmpty()) return false;

    const QKeyCombination combo(
        event->modifiers(), static_cast<Qt::Key>(event->key()));
    return configured.matches(QKeySequence(combo)) == QKeySequence::ExactMatch;
}

int ViewportController::panRateValue() const {
    return std::max(0, _sessionState.state().panRate);
}

double ViewportController::panRateFactor() const {
    const int clamped = Util::clampSliderValue(_sessionState.state().panRate);
    return std::pow(
        Constants::panRateBase, static_cast<double>(clamped - 8));
}

double ViewportController::zoomRateFactor() const {
    return _currentZoomFactor(_sessionState.state().zoomRate);
}

QPoint ViewportController::_clampPixelToOutput(const QPoint &pixel) const {
    const QSize size = outputSize();
    return { std::clamp(pixel.x(), 0, std::max(0, size.width() - 1)),
        std::clamp(pixel.y(), 0, std::max(0, size.height() - 1)) };
}

double ViewportController::_currentZoomFactor(int sliderValue) const {
    return Constants::zoomStepTable[Util::clampSliderValue(sliderValue)];
}

GUIRenderSnapshot ViewportController::_snapshot() const {
    return _sessionState.snapshot();
}