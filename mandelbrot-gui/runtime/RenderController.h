#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

#include <QObject>
#include <QImage>
#include <QString>
#include <QStringList>
#include <QTimer>

#include "BackendAPI.h"
#include "BackendModule.h"

#include "app/GUITypes.h"
#include "app/GUISessionState.h"

#include "util/GUIUtil.h"

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
    void markPreviewMotion();

    [[nodiscard]] bool renderInFlight() const { return _renderInFlight; }
    [[nodiscard]] bool shouldUseInteractionPreviewFallback() const {
        return _presentingCopiedPreview;
    }
    [[nodiscard]] bool hasDisplayedViewState() const {
        return _hasDisplayedViewState;
    }
    [[nodiscard]] const QImage &previewImage() const { return _previewImage; }
    [[nodiscard]] bool previewUsesBackendMemory() const {
        return _previewUsesBackendMemory;
    }
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
    void previewImageChanged();
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

    QImage _previewImage;
    bool _previewUsesBackendMemory = false;
    bool _directFrameDetachedForRender = false;
    bool _presentingCopiedPreview = false;

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

    QString _displayedPointRealText = QStringLiteral("0");
    QString _displayedPointImagText = QStringLiteral("0");
    QString _displayedZoomText;
    QSize _displayedOutputSize{
        GUI::Constants::defaultOutputWidth, GUI::Constants::defaultOutputHeight
    };
    bool _hasDisplayedViewState = false;

    std::thread _renderThread;
    std::optional<GUI::RenderRequest> _queuedRenderRequest;
    std::mutex _renderMutex;
    std::condition_variable _renderCv;
    bool _renderStopRequested = false;
    std::atomic_uint64_t _latestRenderRequestId{ 0 };
    std::atomic_uint64_t _callbackRenderRequestId{ 0 };
    std::atomic_uint64_t _backendGeneration{ 1 };
    std::atomic_uint64_t _lastPresentedRenderId{ 0 };
    std::atomic_bool _interactionPreviewFallbackLatched{ false };
    std::atomic_int64_t _lastProgressUIUpdateMs{ 0 };
    std::chrono::steady_clock::time_point _lastPreviewMotionAt{};
    QTimer _previewStillTimer;

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
    [[nodiscard]] int _interactionFrameIntervalMs(int fallbackFPS) const;
    void _applyPreviewDevicePixelRatio(QImage &image) const;
    void _tryResumeDirectPreview();
};
