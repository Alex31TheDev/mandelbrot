#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <semaphore>
#include <thread>

#include <QObject>
#include <QImage>
#include <QString>
#include <QStringList>

#include "BackendAPI.h"
#include "BackendModule.h"

#include "app/GUITypes.h"
#include "app/GUISessionState.h"

#include "util/GUIUtil.h"

struct RenderCallbackState;
struct DesiredRenderState;

class RenderController final : public QObject {
    Q_OBJECT

public:
    explicit RenderController(QObject *parent = nullptr);
    ~RenderController() override;

    void setAvailableBackends(const QStringList &backendNames);
    void setPreviewDevicePixelRatio(double devicePixelRatio);
    bool loadBackend(const QString &backendName, QString &errorMessage);
    void shutdown(bool forceKillOnTimeout = true,
        int timeoutMs = GUI::Constants::renderShutdownTimeoutMs);

    void requestRender(const GUI::GUIRenderSnapshot &snapshot,
        const std::optional<GUI::PendingPickAction> &pickAction = std::nullopt);
    void cancelQueuedRenders();
    void freezePreview();

    [[nodiscard]] bool renderInFlight() const { return _renderInFlight; }
    [[nodiscard]] bool shouldUseInteractionPreviewFallback() const {
        return _interactionPreviewFallbackLatched;
    }
    [[nodiscard]] bool hasDisplayedViewState() const {
        return _hasDisplayedViewState;
    }
    bool withPreviewImage(const std::function<void(const QImage &)> &visitor) const;
    [[nodiscard]] GUI::ViewTextState displayedViewTextState() const;
    [[nodiscard]] QString statusMessage() const { return _statusText; }
    [[nodiscard]] QString progressText() const { return _progressText; }
    [[nodiscard]] int progressValue() const { return _progressValue; }
    [[nodiscard]] bool progressActive() const { return _progressActive; }
    [[nodiscard]] bool progressCancelled() const { return _progressCancelled; }
    [[nodiscard]] QString pixelsPerSecondText() const {
        return _pixelsPerSecondText;
    }
    [[nodiscard]] QString imageMemoryText() const { return _imageMemoryText; }
    [[nodiscard]] QString viewportFPSText() const { return _viewportFPSText; }
    [[nodiscard]] QString viewportRenderTimeText() const {
        return _viewportRenderTimeText;
    }
    [[nodiscard]] int currentIterationCount() const;

    bool saveImage(const QString &path, bool appendDate,
        const QString &type, QString *savedPath,
        QString &errorMessage);
    [[nodiscard]] QImage renderSinePreview(
        const Backend::SinePaletteConfig &palette,
        const QSize &widgetSize, double rangeMin, double rangeMax
    ) const;
    [[nodiscard]] QImage renderPalettePreview(
        const Backend::PaletteRGBConfig &palette
    ) const;

    bool pointAtPixel(
        const GUI::GUIRenderSnapshot &snapshot, const QPoint &pixel,
        QString &realText, QString &imagText, QString &errorMessage
    );
    bool panPointByDelta(
        const GUI::GUIRenderSnapshot &snapshot, const QPoint &delta,
        QString &realText, QString &imagText, QString &errorMessage
    );
    bool zoomViewAtPixel(
        const GUI::GUIRenderSnapshot &snapshot, const QPoint &pixel,
        double scaleMultiplier, GUI::ViewTextState &view, QString &errorMessage
    );
    bool boxZoomView(const GUI::GUIRenderSnapshot &snapshot, const QRect &rect,
        GUI::ViewTextState &view, QString &errorMessage);
    bool previewPannedViewState(const GUI::GUIRenderSnapshot &snapshot,
        const QPoint &delta, GUI::ViewTextState &view, QString &errorMessage);
    bool previewScaledViewState(const GUI::GUIRenderSnapshot &snapshot,
        const QPoint &pixel, double scaleMultiplier, GUI::ViewTextState &view,
        QString &errorMessage);
    bool previewBoxZoomViewState(const GUI::GUIRenderSnapshot &snapshot,
        const QRect &rect, GUI::ViewTextState &view, QString &errorMessage);
    bool mapViewPixelToViewPixel(const GUI::ViewTextState &sourceView,
        const GUI::ViewTextState &targetView, const QPoint &pixel,
        QPointF &mappedPixel, QString &errorMessage);

signals:
    void previewImageChanged(const GUI::ViewTextState &viewState);
    void renderStateChanged();
    void renderFailed(const QString &message);
    void automaticBackendSwitchRequested(const QString &backendName,
        bool hasPickAction, int pickTarget, const QPoint &pickPixel);

private:
    BackendModule _backend;
    Backend::Session *_renderSession = nullptr;
    Backend::Session *_navigationSession = nullptr;
    Backend::Session *_previewSession = nullptr;
    Backend::Callbacks _callbacks;

    QString _statusText;
    QString _progressText;
    QString _imageMemoryText;
    QString _viewportFPSText = GUI::Util::defaultViewportFPSText();
    QString _viewportRenderTimeText;
    QString _pixelsPerSecondText;
    int _progressValue = 0;
    bool _progressActive = false;
    bool _progressCancelled = false;
    bool _renderInFlight = false;
    double _previewDevicePixelRatio = 1.0;
    QStringList _backendNames;
    std::shared_ptr<RenderCallbackState> _renderCallbackState;

    QString _displayedPointRealText = QStringLiteral("0");
    QString _displayedPointImagText = QStringLiteral("0");
    QString _displayedZoomText;
    QSize _displayedOutputSize{
        GUI::Constants::defaultOutputWidth, GUI::Constants::defaultOutputHeight
    };
    bool _hasDisplayedViewState = false;

    std::thread _renderThread;
    std::atomic<std::shared_ptr<const DesiredRenderState>> _desiredRenderState;
    std::counting_semaphore<> _renderWake{ 0 };
    std::atomic_bool _renderWakePending{ false };
    std::atomic_bool _previewFallbackResetRequested{ false };
    std::atomic_bool _renderStopRequested{ false };
    std::atomic_uint64_t _latestDesiredStateId{ 0 };
    std::atomic_uint64_t _discardBeforeStateId{ 0 };
    std::atomic_uint64_t _backendGeneration{ 1 };
    bool _interactionPreviewFallbackLatched = false;

    void _bindBackendCallbacks();
    void _startRenderWorker();
    void _finishRenderThread(
        bool forceKillOnTimeout = false, int timeoutMs = 0
    );
    bool _ensureBackendReady(QString &errorMessage) const;
    bool _ensureNavigationSessionReady(QString &errorMessage) const;
    bool _applyStateToSession(Backend::Session *session,
        const GUI::GUIRenderSnapshot &snapshot,
        const std::optional<GUI::PendingPickAction> &pickAction,
        QString &errorMessage);
    bool _applyNavigationStateToSession(
        const GUI::GUIRenderSnapshot &snapshot, QString &errorMessage
    );
    [[nodiscard]] QString _backendForRank(
        int rank, const QString &currentBackend
    ) const;
    void _publishCompletedRender(
        const GUI::ViewTextState &viewState,
        uint64_t stateId,
        qint64 renderMs,
        double renderFPS,
        bool previewFallbackLatched
    );
    void _applyPreviewDevicePixelRatio(QImage &image) const;
};
