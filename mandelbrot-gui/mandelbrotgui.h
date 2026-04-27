#pragma once

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>

#include <QImage>
#include <QPoint>
#include <QPointF>
#include <QPixmap>
#include <QRect>
#include <QSize>
#include <QStringList>
#include <QTimer>
#include <QWidget>

#include "BackendAPI.h"
#include "BackendModule.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QScrollArea;
class QShowEvent;
class QSpinBox;
class QSlider;
class QCloseEvent;
class QEvent;
class QAction;
class QMenu;
class QMenuBar;

inline constexpr int kPrecisionInteractionDelayMs = 200;
inline constexpr double kBoostedPanSpeedFactor = 2.0;

class mandelbrotgui final : public QWidget {
public:
    static constexpr auto defaultBackendType = "FloatAVX2";
    static constexpr int defaultInteractionTargetFPS = 60;

    enum class NavMode {
        realtimeZoom,
        zoom,
        pan
    };

    enum class SelectionTarget {
        zoomPoint,
        seedPoint,
        lightPoint
    };

    struct ViewTextState {
        QString pointReal = "0";
        QString pointImag = "0";
        QString zoomText = "0";
        QSize outputSize;
        bool valid = false;
    };

    mandelbrotgui(QWidget *parent = nullptr);
    ~mandelbrotgui() override;

    void requestRender(bool force = false, bool syncControls = true);
    void applyHomeView();
    void scaleAtPixel(const QPoint &pixel, double scaleMultiplier, bool realtimeStep = false);
    void zoomAtPixel(const QPoint &pixel, bool zoomIn, bool realtimeStep = false);
    void boxZoom(const QRect &selectionRect);
    void panByPixels(const QPoint &delta);
    void pickAtPixel(const QPoint &pixel);
    void updateMouseCoords(const QPoint &pixel);
    void clearMouseCoords();
    void onViewportClosed();
    void requestApplicationShutdown(bool closeViewportWindow);
    void adjustIterationsBy(int delta);
    void cycleNavMode();
    void cancelQueuedRenders();
    void quickSaveImage();
    void toggleViewportFullscreen();
    void finalizeViewportFullscreenTransition();
    [[nodiscard]] NavMode navMode() const { return _navMode; }
    [[nodiscard]] NavMode displayedNavMode() const;
    void setDisplayedNavModeOverride(std::optional<NavMode> mode);
    [[nodiscard]] SelectionTarget selectionTarget() const { return _selectionTarget; }
    [[nodiscard]] bool viewportUsesDirectPick() const {
        return _selectionTarget != SelectionTarget::zoomPoint;
    }
    [[nodiscard]] bool renderInFlight() const { return _renderInFlight; }
    [[nodiscard]] QSize outputSize() const;
    [[nodiscard]] const QImage &previewImage() const { return _previewImage; }
    [[nodiscard]] bool previewUsesBackendMemory() const { return _previewUsesBackendMemory; }
    [[nodiscard]] bool hasDisplayedViewState() const { return _hasDisplayedViewState; }
    [[nodiscard]] ViewTextState currentViewTextState() const;
    [[nodiscard]] ViewTextState displayedViewTextState() const;
    bool previewPannedViewState(const QPoint &delta,
        ViewTextState &view, QString &errorMessage);
    bool previewScaledViewState(const QPoint &pixel, double scaleMultiplier,
        ViewTextState &view, QString &errorMessage);
    bool previewBoxZoomViewState(const QRect &selectionRect,
        ViewTextState &view, QString &errorMessage);
    bool mapViewPixelToViewPixel(const ViewTextState &sourceView,
        const ViewTextState &targetView, const QPoint &pixel,
        QPointF &mappedPixel, QString &errorMessage);
    [[nodiscard]] QString viewportStatusText() const;
    [[nodiscard]] int interactionFrameIntervalMs() const;
    [[nodiscard]] bool canRenderAtTargetFPS() const;
    [[nodiscard]] bool shouldUseInteractionPreviewFallback() const;
    [[nodiscard]] int panRateValue() const { return std::max(0, _state.panRate); }
    [[nodiscard]] double panRateFactor() const;
    [[nodiscard]] double zoomRateFactor() const;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    static constexpr int kForceKillDelayMs = 2000;

    struct UIState {
        QString backend;
        bool manualBackendSelection = false;
        int iterations = 0;
        int aaPixels = 1;
        bool useThreads = true;
        bool preserveRatio = true;
        bool julia = false;
        bool inverse = false;
        int outputWidth = 1920;
        int outputHeight = 1080;
        int panRate = 8;
        int zoomRate = 8;
        int interactionTargetFPS = defaultInteractionTargetFPS;
        double zoom = -0.65;
        double exponent = 2.0;
        QPointF point = { 0.0, 0.0 };
        QPointF seed = { 0.0, 0.0 };
        QPointF light = { 1.0, 1.0 };
        Backend::LightColor lightColor;
        Backend::FractalType fractalType = Backend::FractalType::mandelbrot;
        Backend::ColorMethod colorMethod = Backend::ColorMethod::smooth_iterations;
        QString sineName = "default";
        Backend::SinePaletteConfig sinePalette;
        QString paletteName = "default";
        Backend::PaletteHexConfig palette;
    };

    struct PendingPickAction {
        SelectionTarget target = SelectionTarget::zoomPoint;
        QPoint pixel;
    };

    void buildUI();
    void populateControls();
    void connectUI();
    void initializeState();
    void loadSelectedBackend();
    void switchBackend(int rank,
        uint64_t requestId,
        uint64_t backendGeneration,
        const std::optional<PendingPickAction> &pick);
    [[nodiscard]] QString backendForRank(int rank) const;
    void bindBackendCallbacks();
    void startRenderWorker();
    bool applyStateToSession(const UIState &state,
        const QString &pointRealText,
        const QString &pointImagText,
        const QString &zoomText,
        const QString &seedRealText,
        const QString &seedImagText,
        const std::optional<PendingPickAction> &pickAction,
        QString &errorMessage);
    void finishRenderThread(bool forceKillOnTimeout = false, int timeoutMs = 0);
    void refreshPalettePreview();
    void updateSinePreview();
    void updatePreview(const QImage &image,
        bool clearInteractionPreview = true,
        bool usesBackendMemory = false);
    void updateStatusLabels(const QString &rightText = QString());
    void setStatusMessage(const QString &message);
    void setStatusSavedPath(const QString &path);
    void updateLightColorButton();
    void updateModeEnablement();
    void updateViewport();
    void requestViewportRepaint();
    void resizeViewportWindowToImageSize();
    void freezeViewportPreview(bool disableViewport = false,
        bool clearInteractionPreview = false);
    void setViewportInteractive(bool enabled);
    void shutdownForExit(bool closeViewportWindow);
    void updateControlWindowSize();
    void positionWindowsForInitialShow();
    void updateStatusRightEdgeAlignment();
    void handleRenderFailure(const QString &message);
    void showHelpDialog();
    void showAboutDialog();
    void syncStateFromControls();
    void syncStateReadouts();
    void syncControlsFromState();
    void refreshSineList(const QString &preferredName = QString());
    void refreshPaletteList(const QString &preferredName = QString());
    void createNewSinePalette(bool requestRenderOnSuccess = true);
    void createNewColorPalette(bool requestRenderOnSuccess = true);
    void importSine();
    void saveSine();
    void savePointView();
    void loadPointView();
    void openPaletteEditor();
    void saveImage();
    void resizeViewportImage();
    void updateSelectionTarget(int index);
    void updateNavMode(int index);
    void updateAspectLinkedSizes(bool widthChanged);
    void prepareInteractionPreview(bool forceFallbackPreview);
    void cycleViewportGrid();
    void toggleViewportMinimalUI();
    bool commitImageSave(const QString &path, bool appendDate, const QString &type);
    bool loadSineByName(const QString &name,
        bool requestRenderOnSuccess = true,
        QString *errorMessage = nullptr);
    bool loadPaletteByName(const QString &name,
        bool requestRenderOnSuccess = true,
        QString *errorMessage = nullptr);
    void markSineSavedState(const QString &name,
        const Backend::SinePaletteConfig &palette);
    void markPaletteSavedState(const QString &name,
        const Backend::PaletteHexConfig &palette);
    void markPointViewSavedState();
    [[nodiscard]] bool isSineDirty() const;
    [[nodiscard]] bool isPaletteDirty() const;
    [[nodiscard]] bool isPointViewDirty() const;
    void refreshDirtyComboLabels();

    [[nodiscard]] QString stateToString(double value, int precision = 17) const;
    void syncZoomTextFromState();
    void syncStateZoomFromText();
    void syncPointTextFromState();
    void syncStatePointFromText();
    void syncSeedTextFromState();
    void syncStateSeedFromText();
    struct SavedPointViewState {
        Backend::FractalType fractalType = Backend::FractalType::mandelbrot;
        bool inverse = false;
        bool julia = false;
        int iterations = 0;
        QString real = "0";
        QString imag = "0";
        QString zoom = "0";
        QString seedReal = "0";
        QString seedImag = "0";
    };
    [[nodiscard]] SavedPointViewState capturePointViewState() const;
    [[nodiscard]] QPoint clampPixelToOutput(const QPoint &pixel) const;
    [[nodiscard]] double currentZoomFactor(int sliderValue) const;
    [[nodiscard]] int resolveCurrentIterationCount() const;
    [[nodiscard]] bool ensureBackendReady(QString &errorMessage) const;
    [[nodiscard]] bool ensureNavigationSessionReady(QString &errorMessage) const;
    [[nodiscard]] bool applyNavigationStateToSession(QString &errorMessage);
    bool backendPanPointByDelta(const QPoint &delta,
        QString &realText, QString &imagText, QString &errorMessage);
    bool backendPointAtPixel(const QPoint &pixel,
        QString &realText, QString &imagText, QString &errorMessage);
    bool backendZoomViewAtPixel(const QPoint &pixel, double scaleMultiplier,
        ViewTextState &view, QString &errorMessage);
    bool backendBoxZoomView(const QRect &selectionRect,
        ViewTextState &view, QString &errorMessage);

    struct RenderRequest {
        UIState state;
        QString pointRealText;
        QString pointImagText;
        QString zoomText;
        QString seedRealText;
        QString seedImagText;
        std::optional<PendingPickAction> pickAction;
        uint64_t id = 0;
    };

    UIState _state;
    BackendModule _backend;
    Backend::Session *_renderSession = nullptr;
    Backend::Session *_navigationSession = nullptr;
    Backend::Session *_previewSession = nullptr;
    Backend::Callbacks _callbacks;
    QWidget *_viewport = nullptr;

    QImage _previewImage;
    bool _previewUsesBackendMemory = false;
    std::atomic_bool _interactionPreviewFallbackLatched{ false };
    std::atomic_bool _interactionPreviewForced{ false };
    QPixmap _palettePreviewPixmap;
    std::optional<PendingPickAction> _pendingPickAction;
    QPoint _lastMousePixel;
    QString _progressText;
    QString _statusText;
    QString _statusLinkPath;
    QString _statusMarqueeSourceText;
    QString _lastRenderFailureMessage;
    QString _mouseText;
    QString _pointRealText = "0";
    QString _pointImagText = "0";
    QString _zoomText = "-0.65";
    QString _seedRealText = "0";
    QString _seedImagText = "0";
    QString _viewportFPSText = "FPS -";
    QString _viewportRenderTimeText;
    QString _imageMemoryText;
    QString _pixelsPerSecondText = "0 pixels/s";
    QStringList _backendNames;
    int _progressValue = 0;
    bool _progressActive = false;
    bool _progressCancelled = false;
    bool _renderInFlight = false;
    bool _closing = false;
    bool _shutdownQueued = false;
    bool _controlWindowSized = false;
    bool _renderStopRequested = false;
    bool _palettePreviewDirty = true;
    bool _startupBackendError = false;
    bool _hasSavedSineState = false;
    bool _hasSavedPaletteState = false;
    bool _hasSavedPointViewState = false;
    QString _savedSineName;
    QString _savedPaletteName;
    Backend::SinePaletteConfig _savedSinePalette;
    Backend::PaletteHexConfig _savedPalette;
    SavedPointViewState _savedPointViewState;
    std::thread _renderThread;
    QSize _palettePreviewSize;
    std::optional<RenderRequest> _queuedRenderRequest;
    std::atomic_uint64_t _latestRenderRequestId{ 0 };
    uint64_t _lastPresentedRenderId = 0;
    std::mutex _renderMutex;
    std::condition_variable _renderCv;
    std::atomic_uint64_t _callbackRenderRequestId{ 0 };
    std::atomic_uint64_t _backendGeneration{ 1 };
    std::atomic_int64_t _lastProgressUIUpdateMs{ 0 };
    std::atomic_int32_t _lastRenderDurationMs{ 0 };
    QString _displayedPointRealText = "0";
    QString _displayedPointImagText = "0";
    QString _displayedZoomText = "-0.65";
    QSize _displayedOutputSize{ 1920, 1080 };
    bool _hasDisplayedViewState = false;
    QTimer _statusMarqueeTimer;
    int _statusMarqueeOffset = 0;

    QLabel *_progressLabel = nullptr;
    QProgressBar *_progressBar = nullptr;
    QLabel *_pixelsPerSecondLabel = nullptr;
    QLabel *_statusRightLabel = nullptr;
    QScrollArea *_controlScrollArea = nullptr;
    QWidget *_controlScrollContent = nullptr;
    QGroupBox *_cpuGroup = nullptr;
    QGroupBox *_renderGroup = nullptr;
    QGroupBox *_infoGroup = nullptr;
    QGroupBox *_viewportGroup = nullptr;
    QLineEdit *_infoRealEdit = nullptr;
    QLineEdit *_infoImagEdit = nullptr;
    QLineEdit *_infoZoomEdit = nullptr;
    QLineEdit *_infoSeedRealEdit = nullptr;
    QLineEdit *_infoSeedImagEdit = nullptr;
    QPushButton *_savePointButton = nullptr;
    QPushButton *_loadPointButton = nullptr;
    QLineEdit *_cpuNameEdit = nullptr;
    QLineEdit *_cpuCoresEdit = nullptr;
    QLineEdit *_cpuThreadsEdit = nullptr;
    QGroupBox *_sineGroup = nullptr;
    QGroupBox *_paletteGroup = nullptr;
    QGroupBox *_lightGroup = nullptr;
    QCheckBox *_useThreadsCheckBox = nullptr;
    QComboBox *_backendCombo = nullptr;
    QSpinBox *_iterationsSpin = nullptr;
    QComboBox *_colorMethodCombo = nullptr;
    QComboBox *_fractalCombo = nullptr;
    QCheckBox *_juliaCheck = nullptr;
    QCheckBox *_inverseCheck = nullptr;
    QSpinBox *_aaSpin = nullptr;
    QComboBox *_navModeCombo = nullptr;
    QComboBox *_pickTargetCombo = nullptr;
    QSlider *_panRateSlider = nullptr;
    QSpinBox *_panRateSpin = nullptr;
    QSlider *_zoomRateSlider = nullptr;
    QSpinBox *_zoomRateSpin = nullptr;
    QSlider *_exponentSlider = nullptr;
    QDoubleSpinBox *_exponentSpin = nullptr;
    QComboBox *_sineCombo = nullptr;
    QDoubleSpinBox *_freqRSpin = nullptr;
    QDoubleSpinBox *_freqGSpin = nullptr;
    QDoubleSpinBox *_freqBSpin = nullptr;
    QDoubleSpinBox *_freqMultSpin = nullptr;
    QWidget *_sinePreview = nullptr;
    QPushButton *_importSineButton = nullptr;
    QPushButton *_saveSineButton = nullptr;
    QPushButton *_randomizeSineButton = nullptr;
    QComboBox *_paletteCombo = nullptr;
    QDoubleSpinBox *_paletteLengthSpin = nullptr;
    QDoubleSpinBox *_paletteOffsetSpin = nullptr;
    QPushButton *_paletteEditorButton = nullptr;
    QWidget *_palettePreviewLabel = nullptr;
    QLineEdit *_lightRealEdit = nullptr;
    QLineEdit *_lightImagEdit = nullptr;
    QPushButton *_lightColorButton = nullptr;
    QCheckBox *_preserveRatioCheck = nullptr;
    QSpinBox *_outputWidthSpin = nullptr;
    QSpinBox *_outputHeightSpin = nullptr;
    QLabel *_imageMemoryLabel = nullptr;
    QPushButton *_calculateButton = nullptr;
    QPushButton *_homeButton = nullptr;
    QPushButton *_zoomButton = nullptr;
    QPushButton *_saveButton = nullptr;
    QPushButton *_resizeButton = nullptr;
    QMenuBar *_menuBar = nullptr;
    QMenu *_helpMenu = nullptr;
    QAction *_helpAction = nullptr;
    QAction *_aboutAction = nullptr;

    NavMode _navMode = NavMode::realtimeZoom;
    std::optional<NavMode> _displayedNavModeOverride;
    SelectionTarget _selectionTarget = SelectionTarget::zoomPoint;
};
