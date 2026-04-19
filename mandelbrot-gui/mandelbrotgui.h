#pragma once

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

class mandelbrotgui final : public QWidget
{
public:
    enum class NavMode
    {
        realtimeZoom,
        zoom,
        pan
    };

    enum class SelectionTarget
    {
        zoomPoint,
        seedPoint,
        lightPoint
    };

    mandelbrotgui(QWidget *parent = nullptr);
    ~mandelbrotgui() override;

    void requestRender(bool force = false);
    void applyHomeView();
    void zoomAtPixel(const QPoint &pixel, bool zoomIn, bool realtimeStep = false);
    void boxZoom(const QRect &selectionRect);
    void panByPixels(const QPoint &delta);
    void pickAtPixel(const QPoint &pixel);
    void updateMouseCoords(const QPoint &pixel);
    void onViewportClosed();
    void adjustIterationsBy(int delta);
    void cycleNavMode();
    [[nodiscard]] NavMode navMode() const { return _navMode; }
    [[nodiscard]] SelectionTarget selectionTarget() const { return _selectionTarget; }
    [[nodiscard]] bool viewportUsesDirectPick() const
    {
        return _selectionTarget != SelectionTarget::zoomPoint;
    }
    [[nodiscard]] bool renderInFlight() const { return _renderInFlight; }
    [[nodiscard]] QSize outputSize() const;
    [[nodiscard]] const QImage &previewImage() const { return _previewImage; }
    [[nodiscard]] QString viewportStatusText() const;
    [[nodiscard]] int panRedrawIntervalMs() const { return _state.panRedrawIntervalMs; }
    [[nodiscard]] int realtimeZoomIntervalMs() const { return _state.realtimeZoomIntervalMs; }

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    struct UiState
    {
        QString variant;
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
        int panRedrawIntervalMs = 100;
        int realtimeZoomIntervalMs = 30;
        double zoom = -0.65;
        double exponent = 2.0;
        QPointF point = {0.0, 0.0};
        QPointF seed = {0.0, 0.0};
        QPointF light = {1.0, 1.0};
        Backend::LightColor lightColor;
        Backend::FractalType fractalType = Backend::FractalType::mandelbrot;
        Backend::ColorMethod colorMethod = Backend::ColorMethod::smooth_iterations;
        QString sineName = "default";
        Backend::SinePaletteConfig sinePalette;
        QString paletteName = "default";
        Backend::PaletteHexConfig palette;
    };

    struct PendingPickAction
    {
        SelectionTarget target = SelectionTarget::zoomPoint;
        QPoint pixel;
    };

    void buildUi();
    void populateControls();
    void connectUi();
    void initializeState();
    void loadSelectedBackend();
    void bindBackendCallbacks();
    void startRenderWorker();
    bool applyStateToSession(const UiState &state,
                             const std::optional<PendingPickAction> &pickAction,
                             QString &errorMessage);
    void finishRenderThread();
    void refreshPalettePreview();
    void updateSinePreview();
    void updatePreview(const QImage &image);
    void updateStatusLabels(const QString &rightText = QString());
    void updateLightColorButton();
    void updateModeEnablement();
    void updateViewport();
    void resizeViewportWindowToImageSize();
    void updateControlWindowSize();
    void handleRenderFailure(const QString &message);
    void syncStateFromControls();
    void syncStateReadouts();
    void syncControlsFromState();
    void refreshSineList(const QString &preferredName = QString());
    void refreshPaletteList(const QString &preferredName = QString());
    void importSine();
    void saveSine();
    void openPaletteEditor();
    void saveImage();
    void resizeViewportImage();
    void updateSelectionTarget(int index);
    void updateNavMode(int index);
    void updateAspectLinkedSizes(bool widthChanged);
    bool loadSineByName(const QString &name,
                        bool requestRenderOnSuccess = true,
                        QString *errorMessage = nullptr);
    bool loadPaletteByName(const QString &name,
                           bool requestRenderOnSuccess = true,
                           QString *errorMessage = nullptr);

    [[nodiscard]] QString stateToString(double value, int precision = 17) const;
    [[nodiscard]] QPoint complexToPixel(const QPointF &point) const;
    [[nodiscard]] QPoint clampPixelToOutput(const QPoint &pixel) const;
    [[nodiscard]] QPointF outputPixelToComplex(const QPoint &pixel) const;
    [[nodiscard]] double currentRealScale() const;
    [[nodiscard]] double currentImagScale() const;
    [[nodiscard]] double currentZoomFactor(int sliderValue) const;
    [[nodiscard]] bool ensureBackendReady(QString &errorMessage) const;

    struct RenderRequest
    {
        UiState state;
        std::optional<PendingPickAction> pickAction;
        uint64_t id = 0;
    };

    UiState _state;
    BackendModule _backend;
    BackendModule _previewBackend;
    Backend::Callbacks _callbacks;
    QWidget *_viewport = nullptr;

    QImage _previewImage;
    QPixmap _palettePreviewPixmap;
    std::optional<PendingPickAction> _pendingPickAction;
    QPoint _lastMousePixel;
    QString _progressText;
    QString _statusText;
    QString _lastRenderFailureMessage;
    QString _mouseText;
    QString _viewportFpsText = "FPS -";
    QString _viewportRenderTimeText;
    QString _imageMemoryText;
    int _progressValue = 0;
    bool _progressActive = false;
    bool _renderInFlight = false;
    bool _closing = false;
    bool _controlWindowSized = false;
    bool _renderStopRequested = false;
    bool _palettePreviewDirty = true;
    std::thread _renderThread;
    QSize _palettePreviewSize;
    std::optional<RenderRequest> _queuedRenderRequest;
    uint64_t _latestRenderRequestId = 0;
    std::mutex _renderMutex;
    std::condition_variable _renderCv;
    std::atomic_uint64_t _callbackRenderRequestId{ 0 };

    QLabel *_progressLabel = nullptr;
    QProgressBar *_progressBar = nullptr;
    QLabel *_statusRightLabel = nullptr;
    QScrollArea *_controlScrollArea = nullptr;
    QWidget *_controlScrollContent = nullptr;
    QLineEdit *_infoRealEdit = nullptr;
    QLineEdit *_infoImagEdit = nullptr;
    QLineEdit *_infoZoomEdit = nullptr;
    QLineEdit *_cpuNameEdit = nullptr;
    QLineEdit *_cpuCoresEdit = nullptr;
    QLineEdit *_cpuThreadsEdit = nullptr;
    QGroupBox *_sineGroup = nullptr;
    QGroupBox *_paletteGroup = nullptr;
    QGroupBox *_lightGroup = nullptr;
    QCheckBox *_useThreadsCheckBox = nullptr;
    QComboBox *_variantCombo = nullptr;
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

    NavMode _navMode = NavMode::realtimeZoom;
    SelectionTarget _selectionTarget = SelectionTarget::zoomPoint;
};
