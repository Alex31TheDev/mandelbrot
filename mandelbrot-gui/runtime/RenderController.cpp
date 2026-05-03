#include "RenderController.h"

#include <climits>
#include <cmath>
#include <algorithm>
#include <chrono>

#include "util/IncludeWin32.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QMetaObject>

#include "BackendAPI.h"
using namespace Backend;

#include "services/PaletteStore.h"
#include "services/BackendCatalog.h"

#include "util/FileUtil.h"
#include "util/FormatUtil.h"
#include "util/GUIUtil.h"
#include "util/NumberUtil.h"

using namespace GUI;

struct DesiredRenderState {
    GUIRenderSnapshot snapshot;
    std::optional<PendingPickAction> pickAction;
    uint64_t stateId = 0;
};

struct RenderCallbackState {
    std::atomic_uint64_t activeStateId{ 0 };
    std::atomic_uint64_t publishedStateId{ 0 };
    std::atomic_int64_t lastProgressUIUpdateMs{ 0 };
};

static QString stateToString(double value, int precision = 17) {
    return QString::number(value, 'g', precision);
}

RenderController::RenderController(QObject *parent)
    : QObject(parent),
    _renderCallbackState(std::make_shared<RenderCallbackState>()) {
    _displayedZoomText = stateToString(Constants::homeZoom);
    _imageMemoryText = Util::defaultImageMemoryText();
    _pixelsPerSecondText = Util::defaultPixelsPerSecondText();
}

RenderController::~RenderController() {
    cancelQueuedRenders();
    shutdown(true, 0);
}

void RenderController::setAvailableBackends(const QStringList &backendNames) {
    _backendNames = backendNames;
}

void RenderController::setPreviewDevicePixelRatio(double devicePixelRatio) {
    _previewDevicePixelRatio = std::max(1.0, devicePixelRatio);
}

bool RenderController::loadBackend(
    const QString &backendName, QString &errorMessage
) {
    errorMessage.clear();
    _backendGeneration.fetch_add(1, std::memory_order_acq_rel);
    _discardBeforeStateId.store(
        _latestDesiredStateId.fetch_add(1, std::memory_order_acq_rel) + 1,
        std::memory_order_release
    );
    _desiredRenderState.store({}, std::memory_order_release);
    _previewFallbackResetRequested.store(true, std::memory_order_release);
    _renderCallbackState->activeStateId.store(0, std::memory_order_release);
    _renderCallbackState->publishedStateId.store(0, std::memory_order_release);
    if (_backend && _renderSession) {
        _renderSession->setCallbacks({});
    }

    freezePreview();
    _finishRenderThread();
    _interactionPreviewFallbackLatched = false;
    _renderSession = nullptr;
    _navigationSession = nullptr;
    _previewSession = nullptr;
    _backend.reset();
    _progressCancelled = false;

    if (backendName.isEmpty()) {
        errorMessage = tr("No backend is selected.");
        freezePreview();
        emit renderStateChanged();
        return false;
    }

    std::string error;
    _backend = loadBackendModule(FileUtil::executableDir(),
        backendName.toStdString(), error);
    if (_backend) {
        _renderSession = _backend.makeSession();
        if (!_renderSession && error.empty()) {
            error = tr("Failed to create backend session.").toStdString();
        }
    }
    if (_backend) {
        _navigationSession = _backend.makeSession();
        if (!_navigationSession && error.empty()) {
            error = tr("Failed to create navigation session.").toStdString();
        }
    }
    if (_backend) {
        _previewSession = _backend.makeSession();
        if (!_previewSession && error.empty()) {
            error = tr("Failed to create preview session.").toStdString();
        }
    }

    if (_renderSession && _navigationSession && _previewSession) {
        _bindBackendCallbacks();
        _startRenderWorker();
        _statusText = tr("Ready");
        emit renderStateChanged();
        return true;
    }

    errorMessage = error.empty() ? tr("Failed to load backend.")
        : QString::fromStdString(error);
    freezePreview();
    emit renderStateChanged();
    return false;
}

void RenderController::shutdown(bool forceKillOnTimeout, int timeoutMs) {
    _backendGeneration.fetch_add(1, std::memory_order_acq_rel);
    _discardBeforeStateId.store(
        _latestDesiredStateId.fetch_add(1, std::memory_order_acq_rel) + 1,
        std::memory_order_release
    );
    _desiredRenderState.store({}, std::memory_order_release);
    _previewFallbackResetRequested.store(true, std::memory_order_release);
    _renderCallbackState->activeStateId.store(0, std::memory_order_release);
    _renderCallbackState->publishedStateId.store(0, std::memory_order_release);
    if (_backend && _renderSession) {
        _renderSession->setCallbacks({});
    }
    if (_backend) {
        _backend.forceKill();
    }
    _finishRenderThread(forceKillOnTimeout, timeoutMs);
    _interactionPreviewFallbackLatched = false;
    _renderSession = nullptr;
    _navigationSession = nullptr;
    _previewSession = nullptr;
    _backend.reset();
    freezePreview();
}

void RenderController::requestRender(
    const GUIRenderSnapshot &snapshot,
    const std::optional<PendingPickAction> &pickAction
) {
    if (!_backend || !_renderSession) return;

    const uint64_t stateId
        = _latestDesiredStateId.fetch_add(1, std::memory_order_acq_rel) + 1;
    auto desired = std::make_shared<DesiredRenderState>(DesiredRenderState{
        .snapshot = snapshot,
        .pickAction = pickAction,
        .stateId = stateId
        });
    _desiredRenderState.store(std::move(desired), std::memory_order_release);

    const bool wasInFlight = _renderInFlight;
    _renderInFlight = true;
    if (!wasInFlight) {
        _progressActive = true;
        _progressCancelled = false;
        _progressValue = 0;
        _progressText = tr("Rendering");
        _pixelsPerSecondText = Util::defaultPixelsPerSecondText();
        emit renderStateChanged();
    }

    if (!_renderWakePending.exchange(true, std::memory_order_acq_rel)) {
        _renderWake.release();
    }
}

void RenderController::cancelQueuedRenders() {
    const bool hasQueuedRender = static_cast<bool>(
        _desiredRenderState.load(std::memory_order_acquire)
        );
    const bool hasActiveRender
        = _renderCallbackState->activeStateId.load(std::memory_order_acquire) != 0;
    if (!_renderInFlight && !hasQueuedRender && !hasActiveRender) {
        return;
    }

    const int cancelledProgressValue = _progressValue;
    const uint64_t cancelledStateId
        = _latestDesiredStateId.fetch_add(1, std::memory_order_acq_rel) + 1;
    _discardBeforeStateId.store(cancelledStateId, std::memory_order_release);
    _desiredRenderState.store({}, std::memory_order_release);
    _previewFallbackResetRequested.store(true, std::memory_order_release);
    _renderCallbackState->activeStateId.store(0, std::memory_order_release);
    _renderCallbackState->publishedStateId.store(0, std::memory_order_release);
    _renderCallbackState->lastProgressUIUpdateMs.store(0, std::memory_order_release);
    if (_backend) {
        _backend.forceKill();
    }

    _interactionPreviewFallbackLatched = false;
    _renderInFlight = false;
    _progressValue = std::clamp(cancelledProgressValue, 0, 100);
    _progressActive = false;
    _progressCancelled = true;
    _progressText.clear();
    _pixelsPerSecondText = Util::defaultPixelsPerSecondText();
    _statusText = tr("Render cancelled");
    freezePreview();
    emit renderStateChanged();
}

void RenderController::freezePreview() {
    _hasDisplayedViewState = false;
    _interactionPreviewFallbackLatched = false;
    _previewFallbackResetRequested.store(true, std::memory_order_release);
    _viewportFPSText = Util::defaultViewportFPSText();
    _viewportRenderTimeText.clear();
    emit previewImageChanged({});
}

bool RenderController::withPreviewImage(
    const std::function<void(const QImage &)> &visitor
) const {
    if (!visitor || !_renderSession || !_hasDisplayedViewState) {
        return false;
    }

    QImage image = Util::wrapImageViewToImage(_renderSession->imageView());
    if (image.isNull()) {
        return false;
    }

    _applyPreviewDevicePixelRatio(image);
    visitor(image);
    return true;
}

ViewTextState RenderController::displayedViewTextState() const {
    if (!_hasDisplayedViewState) {
        return {};
    }

    return { .pointReal = _displayedPointRealText,
        .pointImag = _displayedPointImagText,
        .zoomText = _displayedZoomText,
        .outputSize = _displayedOutputSize,
        .valid = _displayedOutputSize.width() > 0
            && _displayedOutputSize.height() > 0 };
}

int RenderController::currentIterationCount() const {
    if (_backend && _renderSession) {
        return std::max(1, _renderSession->currentIterationCount());
    }

    return 0;
}

bool RenderController::saveImage(
    const QString &path, bool appendDate,
    const QString &type, QString *savedPath,
    QString &errorMessage
) {
    if (!_ensureBackendReady(errorMessage)) return false;
    if (!_hasDisplayedViewState) {
        errorMessage = tr("No image is available.");
        return false;
    }

    const std::string outputPath = appendDate
        ? FileUtil::appendIsoDate(path.toStdString())
        : path.toStdString();
    if (savedPath) {
        *savedPath = QString::fromStdString(FileUtil::getAbsolutePath(outputPath));
    }

    const Status status = _renderSession->saveImage(path.toStdString(),
        appendDate, type.toStdString());
    if (status) return true;

    errorMessage = QString::fromStdString(status.message);
    return false;
}

QImage RenderController::renderSinePreview(
    const SinePaletteConfig &palette, const QSize &widgetSize,
    double rangeMin, double rangeMax
) const {
    if (!_backend || !_previewSession || !widgetSize.isValid()) {
        return {};
    }

    const Status status = _previewSession->setSinePalette(palette);
    if (!status) return {};

    const int previewWidth = std::max(1, widgetSize.width() - 12);
    const int previewHeight = std::max(1, widgetSize.height() - 28);
    return Util::imageViewToImage(_previewSession->renderSinePreview(
        previewWidth, previewHeight, static_cast<float>(rangeMin),
        static_cast<float>(rangeMax)
    ));
}

QImage RenderController::renderPalettePreview(
    const PaletteRGBConfig &palette
) const {
    return PaletteStore::makePreviewImage(_previewSession, palette, 256, 20);
}

bool RenderController::pointAtPixel(
    const GUIRenderSnapshot &snapshot,
    const QPoint &pixel, QString &realText, QString &imagText,
    QString &errorMessage
) {
    if (!_applyNavigationStateToSession(snapshot, errorMessage)) return false;

    std::string real;
    std::string imag;
    const Status status
        = _navigationSession->getPointAtPixel(pixel.x(), pixel.y(), real, imag);
    if (!status) {
        errorMessage = QString::fromStdString(status.message);
        return false;
    }

    realText = QString::fromStdString(real);
    imagText = QString::fromStdString(imag);
    return true;
}

bool RenderController::panPointByDelta(
    const GUIRenderSnapshot &snapshot,
    const QPoint &delta, QString &realText, QString &imagText,
    QString &errorMessage
) {
    if (!_applyNavigationStateToSession(snapshot, errorMessage)) return false;

    std::string real;
    std::string imag;
    const Status status = _navigationSession->getPanPointByDelta(delta.x(),
        delta.y(), real, imag);
    if (!status) {
        errorMessage = QString::fromStdString(status.message);
        return false;
    }

    realText = QString::fromStdString(real);
    imagText = QString::fromStdString(imag);
    return true;
}

bool RenderController::zoomViewAtPixel(
    const GUIRenderSnapshot &snapshot,
    const QPoint &pixel, double scaleMultiplier, ViewTextState &view,
    QString &errorMessage
) {
    if (!_applyNavigationStateToSession(snapshot, errorMessage)) return false;

    std::string zoom;
    std::string real;
    std::string imag;
    const Status status = _navigationSession->getZoomPointByScale(pixel.x(),
        pixel.y(), scaleMultiplier, zoom, real, imag);
    if (!status) {
        errorMessage = QString::fromStdString(status.message);
        return false;
    }

    view.pointReal = QString::fromStdString(real);
    view.pointImag = QString::fromStdString(imag);
    view.zoomText = QString::fromStdString(zoom);
    view.outputSize
        = QSize(snapshot.state.outputWidth, snapshot.state.outputHeight);
    view.valid = true;
    return true;
}

bool RenderController::boxZoomView(
    const GUIRenderSnapshot &snapshot,
    const QRect &rect, ViewTextState &view, QString &errorMessage
) {
    if (!_applyNavigationStateToSession(snapshot, errorMessage)) return false;

    const QRect normalized = rect.normalized();
    std::string zoom;
    std::string real;
    std::string imag;
    const Status status = _navigationSession->getBoxZoomPoint(normalized.left(),
        normalized.top(), normalized.right(), normalized.bottom(), zoom, real,
        imag);
    if (!status) {
        errorMessage = QString::fromStdString(status.message);
        return false;
    }

    view.pointReal = QString::fromStdString(real);
    view.pointImag = QString::fromStdString(imag);
    view.zoomText = QString::fromStdString(zoom);
    view.outputSize
        = QSize(snapshot.state.outputWidth, snapshot.state.outputHeight);
    view.valid = true;
    return true;
}

bool RenderController::previewPannedViewState(
    const GUIRenderSnapshot &snapshot,
    const QPoint &delta, ViewTextState &view, QString &errorMessage
) {
    view = { .pointReal = snapshot.pointRealText,
        .pointImag = snapshot.pointImagText,
        .zoomText = snapshot.zoomText,
        .outputSize = QSize(snapshot.state.outputWidth,
            snapshot.state.outputHeight),
        .valid = snapshot.state.outputWidth > 0
            && snapshot.state.outputHeight > 0 };
    if (!view.valid || delta.isNull()) {
        return view.valid;
    }

    QString real;
    QString imag;
    if (!panPointByDelta(snapshot, delta, real, imag, errorMessage)) {
        view = {};
        return false;
    }

    view.pointReal = real;
    view.pointImag = imag;
    return true;
}

bool RenderController::previewScaledViewState(
    const GUIRenderSnapshot &snapshot,
    const QPoint &pixel, double scaleMultiplier, ViewTextState &view,
    QString &errorMessage
) {
    if (NumberUtil::almostEqual(scaleMultiplier, 1.0)) {
        view = { .pointReal = snapshot.pointRealText,
            .pointImag = snapshot.pointImagText,
            .zoomText = snapshot.zoomText,
            .outputSize = QSize(snapshot.state.outputWidth,
                snapshot.state.outputHeight),
            .valid = snapshot.state.outputWidth > 0
                && snapshot.state.outputHeight > 0 };
        return view.valid;
    }

    return zoomViewAtPixel(snapshot, pixel, scaleMultiplier, view, errorMessage);
}

bool RenderController::previewBoxZoomViewState(
    const GUIRenderSnapshot &snapshot, const QRect &rect, ViewTextState &view,
    QString &errorMessage
) {
    const QRect normalized = rect.normalized();
    if (normalized.width() < 2 || normalized.height() < 2) {
        view = { .pointReal = snapshot.pointRealText,
            .pointImag = snapshot.pointImagText,
            .zoomText = snapshot.zoomText,
            .outputSize = QSize(snapshot.state.outputWidth,
                snapshot.state.outputHeight),
            .valid = snapshot.state.outputWidth > 0
                && snapshot.state.outputHeight > 0 };
        return view.valid;
    }

    return boxZoomView(snapshot, normalized, view, errorMessage);
}

bool RenderController::mapViewPixelToViewPixel(
    const ViewTextState &sourceView,
    const ViewTextState &targetView, const QPoint &pixel, QPointF &mappedPixel,
    QString &errorMessage
) {
    if (!_ensureNavigationSessionReady(errorMessage)) return false;

    ViewportState source{ .outputWidth = sourceView.outputSize.width(),
        .outputHeight = sourceView.outputSize.height(),
        .pointReal = sourceView.pointReal.toStdString(),
        .pointImag = sourceView.pointImag.toStdString(),
        .zoom = sourceView.zoomText.toStdString() };
    ViewportState target{ .outputWidth = targetView.outputSize.width(),
        .outputHeight = targetView.outputSize.height(),
        .pointReal = targetView.pointReal.toStdString(),
        .pointImag = targetView.pointImag.toStdString(),
        .zoom = targetView.zoomText.toStdString() };

    double mappedX = 0.0;
    double mappedY = 0.0;
    const Status status = _navigationSession->mapViewPixelToViewPixel(source,
        target, pixel.x(), pixel.y(), mappedX, mappedY);
    if (!status) {
        errorMessage = QString::fromStdString(status.message);
        return false;
    }

    mappedPixel = QPointF(mappedX, mappedY);
    return true;
}

void RenderController::_bindBackendCallbacks() {
    _callbacks = {};
    const uint64_t backendGeneration
        = _backendGeneration.load(std::memory_order_acquire);
    _renderCallbackState = std::make_shared<RenderCallbackState>();
    const std::shared_ptr<RenderCallbackState> callbackState = _renderCallbackState;

    _callbacks.onProgress = [this, backendGeneration, callbackState](
        const ProgressEvent &event
        ) {
            if (backendGeneration
                != _backendGeneration.load(std::memory_order_acquire)) {
                return;
            }
            const uint64_t stateId
                = callbackState->activeStateId.load(std::memory_order_acquire);
            if (stateId == 0) return;

            const auto now = std::chrono::steady_clock::now();
            const int64_t nowMs
                = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch()
                )
                .count();
            if (!event.completed && event.percentage < 100) {
                const int64_t lastMs
                    = callbackState->lastProgressUIUpdateMs.load(
                        std::memory_order_acquire
                    );
                if (lastMs > 0 && (nowMs - lastMs) < 33) {
                    return;
                }
            }
            callbackState->lastProgressUIUpdateMs.store(nowMs,
                std::memory_order_release);

            const QString progress = QStringLiteral("%1%").arg(event.percentage);
            const QString pixelsPerSecond = Util::formatPixelsPerSecondText(
                QString::fromStdString(
                    FormatUtil::formatBigNumber(event.opsPerSecond)
                )
            );

            QMetaObject::invokeMethod(this,
                [this, progress, pixelsPerSecond,
                stateId, backendGeneration, callbackState]() {
                    if (backendGeneration
                        != _backendGeneration.load(std::memory_order_acquire)) {
                        return;
                    }
                    const uint64_t activeStateId
                        = callbackState->activeStateId.load(
                            std::memory_order_acquire
                        );
                    const uint64_t publishedStateId
                        = callbackState->publishedStateId.load(
                            std::memory_order_acquire
                        );
                    if (stateId != activeStateId && stateId != publishedStateId) {
                        return;
                    }

                    _progressText = progress;
                    _progressValue = std::clamp(
                        progress.left(progress.size() - 1).toInt(), 0, 100
                    );
                    _progressActive = true;
                    _progressCancelled = false;
                    _pixelsPerSecondText = pixelsPerSecond;
                    emit renderStateChanged();
                });
        };

    _callbacks.onImage = [this, backendGeneration](const ImageEvent &event) {
        if (backendGeneration
            != _backendGeneration.load(std::memory_order_acquire)) {
            return;
        }
        if (event.kind == ImageEventKind::allocated) {
            const QString imageMemoryText
                = Util::formatImageMemoryText(event);
            QMetaObject::invokeMethod(this,
                [this, imageMemoryText, backendGeneration]() {
                    if (backendGeneration
                        != _backendGeneration.load(
                            std::memory_order_acquire
                        )) {
                        return;
                    }

                    _imageMemoryText = imageMemoryText;
                    emit renderStateChanged();
                });
        }
        };

    _callbacks.onInfo = [this, backendGeneration, callbackState](
        const InfoEvent &event
        ) {
            if (backendGeneration
                != _backendGeneration.load(std::memory_order_acquire)) {
                return;
            }
            const uint64_t stateId
                = callbackState->activeStateId.load(std::memory_order_acquire);
            if (stateId == 0) return;

            const QString text = tr("Iterations: %1 | %2 GI/s")
                .arg(QString::fromStdString(
                    FormatUtil::formatNumber(event.totalIterations)
                ),
                    QString::number(event.opsPerSecond, 'f', 2));

            QMetaObject::invokeMethod(this,
                [this, text, stateId, backendGeneration, callbackState]() {
                    if (backendGeneration
                        != _backendGeneration.load(std::memory_order_acquire)) {
                        return;
                    }
                    const uint64_t activeStateId
                        = callbackState->activeStateId.load(
                            std::memory_order_acquire
                        );
                    const uint64_t publishedStateId
                        = callbackState->publishedStateId.load(
                            std::memory_order_acquire
                        );
                    if (stateId != activeStateId && stateId != publishedStateId) {
                        return;
                    }

                    _statusText = text;
                    emit renderStateChanged();
                });
        };

    _callbacks.onDebug = [this, backendGeneration](const DebugEvent &event) {
        if (backendGeneration
            != _backendGeneration.load(std::memory_order_acquire)) {
            return;
        }
        if (!event.message) return;
        const QString message = QString::fromUtf8(event.message);
        QMetaObject::invokeMethod(this, [this, message, backendGeneration]() {
            if (backendGeneration
                != _backendGeneration.load(std::memory_order_acquire)) {
                return;
            }
            _statusText = message;
            emit renderStateChanged();
            });
        };

    _renderSession->setCallbacks(_callbacks);
}

void RenderController::_startRenderWorker() {
    if (_renderThread.joinable()) return;

    _renderStopRequested.store(false, std::memory_order_release);
    _renderWakePending.store(false, std::memory_order_release);
    _desiredRenderState.store({}, std::memory_order_release);

    const uint64_t backendGeneration
        = _backendGeneration.load(std::memory_order_acquire);
    const std::shared_ptr<RenderCallbackState> callbackState = _renderCallbackState;
    _renderThread = std::thread([this, backendGeneration, callbackState]() {
        bool previewFallbackLatched = false;
        for (;;) {
            _renderWake.acquire();
            _renderWakePending.store(false, std::memory_order_release);

            if (_renderStopRequested.load(std::memory_order_acquire)) {
                break;
            }
            if (backendGeneration
                != _backendGeneration.load(std::memory_order_acquire)) {
                break;
            }

            std::shared_ptr<const DesiredRenderState> desired
                = _desiredRenderState.exchange({}, std::memory_order_acq_rel);
            while (desired) {
                if (_previewFallbackResetRequested.exchange(false,
                    std::memory_order_acq_rel)) {
                    previewFallbackLatched = false;
                }
                if (_renderStopRequested.load(std::memory_order_acquire)) {
                    callbackState->activeStateId.store(0, std::memory_order_release);
                    return;
                }
                if (backendGeneration
                    != _backendGeneration.load(std::memory_order_acquire)) {
                    callbackState->activeStateId.store(0, std::memory_order_release);
                    return;
                }

                const DesiredRenderState active = *desired;
                callbackState->publishedStateId.store(0, std::memory_order_release);
                callbackState->activeStateId.store(active.stateId, std::memory_order_release);

                QMetaObject::invokeMethod(this,
                    [this, backendGeneration, stateId = active.stateId, callbackState]() {
                        if (backendGeneration
                            != _backendGeneration.load(std::memory_order_acquire)) {
                            return;
                        }
                        if (stateId
                            != callbackState->activeStateId.load(
                                std::memory_order_acquire
                            )) {
                            return;
                        }

                        _renderInFlight = true;
                        _progressActive = true;
                        _progressCancelled = false;
                        _progressValue = 0;
                        _progressText = tr("Rendering");
                        _pixelsPerSecondText = Util::defaultPixelsPerSecondText();
                        callbackState->lastProgressUIUpdateMs.store(0,
                            std::memory_order_release);
                        emit renderStateChanged();
                    });

                QString error;
                int rank = 0;
                QString targetBackend;
                Status status;
                auto renderStart = std::chrono::steady_clock::now();
                auto renderEnd = renderStart;
                if (!_applyStateToSession(_renderSession,
                    active.snapshot, active.pickAction, error)) {
                    status = Status::failure(error.toStdString());
                } else {
                    rank = _renderSession->precisionRank();
                    targetBackend = _backendForRank(rank, active.snapshot.state.backend);
                    if (!active.snapshot.state.manualBackendSelection
                        && targetBackend != active.snapshot.state.backend) {
                        status = Status::success();
                    } else {
                        renderStart = std::chrono::steady_clock::now();
                        status = _renderSession->render();
                        renderEnd = std::chrono::steady_clock::now();
                    }
                }

                callbackState->activeStateId.store(0, std::memory_order_release);

                if (backendGeneration
                    != _backendGeneration.load(std::memory_order_acquire)) {
                    return;
                }

                if (!status) {
                    const QString failureMessage = error.isEmpty()
                        ? QString::fromStdString(status.message)
                        : error;
                    QMetaObject::invokeMethod(this,
                        [this, failureMessage, backendGeneration,
                        stateId = active.stateId]() {
                            if (backendGeneration
                                != _backendGeneration.load(
                                    std::memory_order_acquire
                                )) {
                                return;
                            }
                            if (stateId
                                < _discardBeforeStateId.load(
                                    std::memory_order_acquire
                                )) {
                                return;
                            }
                            if (stateId
                                != _latestDesiredStateId.load(
                                    std::memory_order_acquire
                                )) {
                                return;
                            }

                            _renderInFlight = false;
                            _progressActive = false;
                            _progressCancelled = false;
                            _progressValue = 0;
                            _progressText.clear();
                            _pixelsPerSecondText = Util::defaultPixelsPerSecondText();
                            _statusText = failureMessage;
                            emit renderStateChanged();
                            emit renderFailed(failureMessage);
                        });
                } else if (!active.snapshot.state.manualBackendSelection
                    && targetBackend != active.snapshot.state.backend) {
                    QMetaObject::invokeMethod(this,
                        [this, targetBackend, pick = active.pickAction,
                        stateId = active.stateId, backendGeneration]() {
                            if (backendGeneration
                                != _backendGeneration.load(
                                    std::memory_order_acquire
                                )) {
                                return;
                            }
                            if (stateId
                                < _discardBeforeStateId.load(
                                    std::memory_order_acquire
                                )) {
                                return;
                            }
                            if (stateId
                                != _latestDesiredStateId.load(
                                    std::memory_order_acquire
                                )) {
                                return;
                            }

                            emit automaticBackendSwitchRequested(targetBackend,
                                pick.has_value(),
                                pick ? static_cast<int>(pick->target)
                                : static_cast<int>(SelectionTarget::zoomPoint),
                                pick ? pick->pixel : QPoint());
                        });
                } else {
                    callbackState->publishedStateId.store(active.stateId,
                        std::memory_order_release);
                    const auto renderElapsed
                        = std::chrono::duration_cast<std::chrono::milliseconds>(
                            renderEnd - renderStart
                        );
                    const qint64 renderMs = std::max<qint64>(1, renderElapsed.count());
                    const double renderFPS = 1000.0 / static_cast<double>(renderMs);
                    const int fallbackFPS = std::max(0,
                        active.snapshot.state.interactionTargetFPS);
                    const int targetFrameMs = fallbackFPS > 0
                        ? std::max(1,
                            static_cast<int>(std::lround(
                                1000.0 / static_cast<double>(fallbackFPS)
                            )))
                        : INT_MAX;
                    const int recoveryFrameMs
                        = fallbackFPS > 0 ? std::max(1, targetFrameMs / 2) : INT_MAX;
                    if (fallbackFPS <= 0) {
                        previewFallbackLatched = false;
                    } else if (previewFallbackLatched) {
                        if (renderMs <= recoveryFrameMs) {
                            previewFallbackLatched = false;
                        }
                    } else if (renderMs > targetFrameMs) {
                        previewFallbackLatched = true;
                    }

                    const ViewTextState viewState{
                        .pointReal = active.snapshot.pointRealText,
                        .pointImag = active.snapshot.pointImagText,
                        .zoomText = active.snapshot.zoomText,
                        .outputSize = QSize(active.snapshot.state.outputWidth,
                            active.snapshot.state.outputHeight),
                        .valid = active.snapshot.state.outputWidth > 0
                            && active.snapshot.state.outputHeight > 0
                    };

                    QMetaObject::invokeMethod(this,
                        [this, viewState, stateId = active.stateId,
                        renderMs, renderFPS, previewFallbackLatched,
                        backendGeneration]() {
                            if (backendGeneration
                                != _backendGeneration.load(
                                    std::memory_order_acquire
                                )) {
                                return;
                            }
                            _publishCompletedRender(viewState, stateId,
                                renderMs, renderFPS, previewFallbackLatched);
                        });
                }

                desired = _desiredRenderState.exchange({}, std::memory_order_acq_rel);
            }
        }
        });
}

void RenderController::_finishRenderThread(
    bool forceKillOnTimeout, int timeoutMs
) {
    _renderStopRequested.store(true, std::memory_order_release);
    _desiredRenderState.store({}, std::memory_order_release);
    _previewFallbackResetRequested.store(true, std::memory_order_release);
    _renderCallbackState->activeStateId.store(0, std::memory_order_release);
    _renderCallbackState->publishedStateId.store(0, std::memory_order_release);
    _renderCallbackState->lastProgressUIUpdateMs.store(0, std::memory_order_release);
    if (!_renderWakePending.exchange(true, std::memory_order_acq_rel)) {
        _renderWake.release();
    }

    if (_renderThread.joinable()) {
#ifdef _WIN32
        HANDLE renderThreadHandle
            = static_cast<HANDLE>(_renderThread.native_handle());
        const auto waitForThread = [renderThreadHandle](int waitMs) {
            if (!renderThreadHandle) return true;
            const DWORD waitResult = WaitForSingleObject(renderThreadHandle,
                waitMs <= 0 ? 0u : static_cast<DWORD>(waitMs));
            return waitResult == WAIT_OBJECT_0;
            };
        const auto waitForThreadWithEvents = [&](int totalWaitMs) {
            if (!renderThreadHandle) return true;

            if (totalWaitMs <= 0) {
                const bool stopped = waitForThread(0);
                QCoreApplication::processEvents(QEventLoop::AllEvents, 0);
                return stopped || waitForThread(0);
            }

            int remainingWaitMs = totalWaitMs;
            for (;;) {
                const int sliceWaitMs = std::min(remainingWaitMs, 25);
                if (waitForThread(sliceWaitMs)) {
                    return true;
                }

                QCoreApplication::processEvents(QEventLoop::AllEvents,
                    sliceWaitMs);

                remainingWaitMs -= sliceWaitMs;
                if (remainingWaitMs <= 0) {
                    return waitForThread(0);
                }
            }
            };

        const int boundedWaitMs = std::max(0, timeoutMs);
        bool stopped = waitForThreadWithEvents(boundedWaitMs);
        if (!stopped && forceKillOnTimeout) {
            _backend.forceKill();
            stopped = waitForThreadWithEvents(
                boundedWaitMs > 0 ? std::max(250, boundedWaitMs) : 0
            );
            if (!stopped && renderThreadHandle) {
                TerminateThread(renderThreadHandle, 1);
                stopped = waitForThreadWithEvents(50);
            }
        } else if (!stopped) {
            while (!stopped) {
                stopped = waitForThreadWithEvents(250);
            }
        }
#else
        (void)forceKillOnTimeout;
        (void)timeoutMs;
#endif
        _renderThread.join();
    }
    _renderStopRequested.store(false, std::memory_order_release);
    _renderWakePending.store(false, std::memory_order_release);
    _desiredRenderState.store({}, std::memory_order_release);

    _renderInFlight = false;
    _progressActive = false;
    _progressCancelled = false;
    _progressValue = 0;
    _progressText.clear();
    _pixelsPerSecondText = Util::defaultPixelsPerSecondText();
}

bool RenderController::_ensureBackendReady(QString &errorMessage) const {
    if (_backend && _renderSession) return true;
    errorMessage = tr("Backend not loaded.");
    return false;
}

bool RenderController::_ensureNavigationSessionReady(
    QString &errorMessage
) const {
    if (_navigationSession) return true;
    errorMessage = tr("Navigation session unavailable.");
    return false;
}

bool RenderController::_applyStateToSession(
    Session *session,
    const GUIRenderSnapshot &snapshot,
    const std::optional<PendingPickAction> &pickAction,
    QString &errorMessage
) {
    if (!session) {
        errorMessage = tr("Backend session unavailable.");
        return false;
    }

    auto failIfNeeded = [&errorMessage](const Status &status) {
        if (status) return false;
        errorMessage = QString::fromStdString(status.message);
        return true;
        };

    if (failIfNeeded(session->setImageSize(snapshot.state.outputWidth,
        snapshot.state.outputHeight, snapshot.state.aaPixels))) {
        return false;
    }
    session->setUseThreads(snapshot.state.useThreads);
    if (failIfNeeded(
        session->setZoom(snapshot.state.iterations, snapshot.zoomText.toStdString())
    )) {
        return false;
    }
    if (failIfNeeded(session->setPoint(snapshot.pointRealText.toStdString(),
        snapshot.pointImagText.toStdString()))) {
        return false;
    }
    if (failIfNeeded(session->setSeed(snapshot.seedRealText.toStdString(),
        snapshot.seedImagText.toStdString()))) {
        return false;
    }
    if (failIfNeeded(session->setFractalType(snapshot.state.fractalType))) {
        return false;
    }
    session->setFractalMode(snapshot.state.julia, snapshot.state.inverse);
    if (failIfNeeded(session->setFractalExponent(
        stateToString(snapshot.state.exponent).toStdString()
    ))) {
        return false;
    }
    if (failIfNeeded(session->setColorMethod(snapshot.state.colorMethod))) {
        return false;
    }
    if (failIfNeeded(session->setSinePalette(snapshot.state.sinePalette))) {
        return false;
    }
    if (failIfNeeded(session->setColorPalette(snapshot.state.palette))) {
        return false;
    }
    if (failIfNeeded(session->setLight(
        static_cast<float>(snapshot.state.light.x()),
        static_cast<float>(snapshot.state.light.y())
    ))) {
        return false;
    }
    if (failIfNeeded(session->setLightColor(snapshot.state.lightColor))) {
        return false;
    }

    if (!pickAction) return true;

    Status status;
    switch (pickAction->target) {
        case SelectionTarget::zoomPoint:
            status = session->setPoint(pickAction->pixel.x(), pickAction->pixel.y());
            break;
        case SelectionTarget::seedPoint:
            status = session->setSeed(pickAction->pixel.x(), pickAction->pixel.y());
            break;
        case SelectionTarget::lightPoint:
            status = session->setLight(pickAction->pixel.x(), pickAction->pixel.y());
            break;
    }
    return !failIfNeeded(status);
}

bool RenderController::_applyNavigationStateToSession(
    const GUIRenderSnapshot &snapshot, QString &errorMessage
) {
    if (!_ensureNavigationSessionReady(errorMessage)) return false;
    return _applyStateToSession(_navigationSession, snapshot, std::nullopt, errorMessage);
}

QString RenderController::_backendForRank(
    int rank, const QString &currentBackend
) const {
    if (_backendNames.isEmpty()) return QString();

    const int targetPrecision = std::clamp(rank, 0, 3);
    const int currentType = BackendCatalog::typeRank(currentBackend);

    QString fallback;
    int fallbackPrecision = -1;
    int fallbackTypeDist = INT_MAX;
    QString best;
    int bestPenalty = INT_MAX;
    int bestTypeDist = INT_MAX;

    for (const QString &backendName : _backendNames) {
        const QString name = backendName.trimmed();
        const int precision = BackendCatalog::precisionRank(name);
        const int typeDist
            = std::abs(BackendCatalog::typeRank(name) - currentType);

        if (precision > fallbackPrecision
            || (precision == fallbackPrecision
                && typeDist < fallbackTypeDist)) {
            fallback = name;
            fallbackPrecision = precision;
            fallbackTypeDist = typeDist;
        }

        if (precision < targetPrecision) continue;

        const int penalty = precision - targetPrecision;
        if (penalty < bestPenalty
            || (penalty == bestPenalty && typeDist < bestTypeDist)) {
            best = name;
            bestPenalty = penalty;
            bestTypeDist = typeDist;
        }
    }

    return best.isEmpty() ? fallback : best;
}

void RenderController::_publishCompletedRender(
    const ViewTextState &viewState,
    uint64_t stateId,
    qint64 renderMs,
    double renderFPS,
    bool previewFallbackLatched
) {
    if (stateId < _discardBeforeStateId.load(std::memory_order_acquire)) {
        return;
    }

    _viewportFPSText = Util::formatViewportFPSText(renderFPS);
    _viewportRenderTimeText = QString::fromStdString(
        FormatUtil::formatDuration(renderMs)
    );
    _interactionPreviewFallbackLatched = previewFallbackLatched;

    _displayedPointRealText = viewState.pointReal;
    _displayedPointImagText = viewState.pointImag;
    _displayedZoomText = viewState.zoomText;
    _displayedOutputSize = viewState.outputSize;
    _hasDisplayedViewState = viewState.valid;

    const uint64_t latestStateId
        = _latestDesiredStateId.load(std::memory_order_acquire);
    if (stateId >= latestStateId) {
        _renderInFlight = false;
        _progressActive = false;
        _progressCancelled = false;
        _progressValue = 0;
        _progressText.clear();
    } else {
        _renderInFlight = true;
        _progressActive = true;
        _progressCancelled = false;
        _progressValue = 0;
        _progressText = tr("Rendering");
    }

    emit previewImageChanged(displayedViewTextState());
    emit renderStateChanged();
}

void RenderController::_applyPreviewDevicePixelRatio(QImage &image) const {
    if (!image.isNull()) {
        image.setDevicePixelRatio(_previewDevicePixelRatio);
    }
}