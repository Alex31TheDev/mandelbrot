#pragma once

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

#include <QAction>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QGroupBox>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QPixmap>
#include <QPoint>
#include <QPointF>
#include <QProgressBar>
#include <QPushButton>
#include <QRect>
#include <QScrollArea>
#include <QShowEvent>
#include <QSize>
#include <QSlider>
#include <QSpinBox>
#include <QStringList>
#include <QTimer>

#include "AppSettings.h"
#include "BackendAPI.h"
#include "BackendModule.h"
#include "GUILocale.h"
#include "GUITypes.h"
#include "Shortcuts.h"
#include "windows/viewport/ViewportHost.h"
#include "windows/viewport/ViewportWindow.h"

namespace Ui {
class ControlWindow;
}

class ControlWindow final : public QMainWindow, public ViewportHost {
    Q_OBJECT

public:
    ControlWindow(GUILocale& locale, QWidget* parent = nullptr);
    ~ControlWindow() override;

    void requestRender(bool force = false, bool syncControls = true);
    void applyHomeView() override;
    void scaleAtPixel(const QPoint& pixel, double scaleMultiplier,
        bool realtimeStep = false) override;
    void zoomAtPixel(
        const QPoint& pixel, bool zoomIn, bool realtimeStep = false) override;
    void boxZoom(const QRect& selectionRect) override;
    void panByPixels(const QPoint& delta) override;
    void pickAtPixel(const QPoint& pixel) override;
    void updateMouseCoords(const QPoint& pixel) override;
    void clearMouseCoords() override;
    void onViewportClosed();
    void requestApplicationShutdown(bool closeViewportWindow) override;
    void adjustIterationsBy(int delta) override;
    void cycleNavMode() override;
    void cancelQueuedRenders() override;
    void quickSaveImage() override;
    void toggleViewportFullscreen();
    void prepareViewportFullscreenTransition() override;
    void applyViewportOutputSize(const QSize& outputSize) override;

    [[nodiscard]] NavMode navMode() const override { return _navMode; }
    [[nodiscard]] NavMode displayedNavMode() const;
    void setDisplayedNavModeOverride(std::optional<NavMode> mode) override;
    [[nodiscard]] SelectionTarget selectionTarget() const {
        return _selectionTarget;
    }
    [[nodiscard]] bool viewportUsesDirectPick() const override {
        return _selectionTarget != SelectionTarget::zoomPoint;
    }
    [[nodiscard]] bool renderInFlight() const override {
        return _renderInFlight;
    }
    [[nodiscard]] QSize outputSize() const override;
    [[nodiscard]] const QImage& previewImage() const override {
        return _previewImage;
    }
    [[nodiscard]] bool previewUsesBackendMemory() const override {
        return _previewUsesBackendMemory;
    }
    [[nodiscard]] bool hasDisplayedViewState() const override {
        return _hasDisplayedViewState;
    }
    [[nodiscard]] ViewTextState currentViewTextState() const override;
    [[nodiscard]] ViewTextState displayedViewTextState() const override;
    bool previewPannedViewState(const QPoint& delta, ViewTextState& view,
        QString& errorMessage) override;
    bool previewScaledViewState(const QPoint& pixel, double scaleMultiplier,
        ViewTextState& view, QString& errorMessage) override;
    bool previewBoxZoomViewState(
        const QRect& selectionRect, ViewTextState& view, QString& errorMessage);
    bool mapViewPixelToViewPixel(const ViewTextState& sourceView,
        const ViewTextState& targetView, const QPoint& pixel,
        QPointF& mappedPixel, QString& errorMessage) override;
    [[nodiscard]] QString viewportStatusText() const override;
    [[nodiscard]] int interactionFrameIntervalMs() const override;
    [[nodiscard]] bool canRenderAtTargetFPS() const;
    [[nodiscard]] bool shouldUseInteractionPreviewFallback() const override;
    [[nodiscard]] bool matchesShortcut(
        const QString& id, const QKeyEvent* event) const override;
    [[nodiscard]] int panRateValue() const override {
        return std::max(0, _state.panRate);
    }
    [[nodiscard]] double panRateFactor() const override;
    [[nodiscard]] double zoomRateFactor() const override;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void changeEvent(QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    static constexpr int kForceKillDelayMs = 2000;

    std::unique_ptr<Ui::ControlWindow> _ui;
    GUIState _state;
    BackendModule _backend;
    Backend::Session* _renderSession = nullptr;
    Backend::Session* _navigationSession = nullptr;
    Backend::Session* _previewSession = nullptr;
    Backend::Callbacks _callbacks;
    ViewportWindow* _viewport = nullptr;

    QImage _previewImage;
    bool _previewUsesBackendMemory = false;
    std::atomic_bool _interactionPreviewFallbackLatched { false };
    std::atomic_bool _interactionPreviewForced { false };
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
    std::atomic_uint64_t _latestRenderRequestId { 0 };
    uint64_t _lastPresentedRenderId = 0;
    std::mutex _renderMutex;
    std::condition_variable _renderCv;
    std::atomic_uint64_t _callbackRenderRequestId { 0 };
    std::atomic_uint64_t _backendGeneration { 1 };
    std::atomic_int64_t _lastProgressUIUpdateMs { 0 };
    std::atomic_int32_t _lastRenderDurationMs { 0 };
    QString _displayedPointRealText = "0";
    QString _displayedPointImagText = "0";
    QString _displayedZoomText = "-0.65";
    QSize _displayedOutputSize { 1920, 1080 };
    bool _hasDisplayedViewState = false;
    QTimer _statusMarqueeTimer;
    int _statusMarqueeOffset = 0;

    QLabel* _progressLabel = nullptr;
    QProgressBar* _progressBar = nullptr;
    QLabel* _pixelsPerSecondLabel = nullptr;
    QLabel* _statusRightLabel = nullptr;
    QScrollArea* _controlScrollArea = nullptr;
    QWidget* _controlScrollContent = nullptr;
    QGroupBox* _cpuGroup = nullptr;
    QGroupBox* _renderGroup = nullptr;
    QGroupBox* _infoGroup = nullptr;
    QGroupBox* _viewportGroup = nullptr;
    QLineEdit* _infoRealEdit = nullptr;
    QLineEdit* _infoImagEdit = nullptr;
    QLineEdit* _infoZoomEdit = nullptr;
    QLineEdit* _infoSeedRealEdit = nullptr;
    QLineEdit* _infoSeedImagEdit = nullptr;
    QPushButton* _savePointButton = nullptr;
    QPushButton* _loadPointButton = nullptr;
    QLineEdit* _cpuNameEdit = nullptr;
    QLineEdit* _cpuCoresEdit = nullptr;
    QLineEdit* _cpuThreadsEdit = nullptr;
    QGroupBox* _sineGroup = nullptr;
    QGroupBox* _paletteGroup = nullptr;
    QGroupBox* _lightGroup = nullptr;
    QCheckBox* _useThreadsCheckBox = nullptr;
    QComboBox* _backendCombo = nullptr;
    QSpinBox* _iterationsSpin = nullptr;
    QComboBox* _colorMethodCombo = nullptr;
    QComboBox* _fractalCombo = nullptr;
    QCheckBox* _juliaCheck = nullptr;
    QCheckBox* _inverseCheck = nullptr;
    QSpinBox* _aaSpin = nullptr;
    QComboBox* _navModeCombo = nullptr;
    QComboBox* _pickTargetCombo = nullptr;
    QSlider* _panRateSlider = nullptr;
    QSpinBox* _panRateSpin = nullptr;
    QSlider* _zoomRateSlider = nullptr;
    QSpinBox* _zoomRateSpin = nullptr;
    QSlider* _exponentSlider = nullptr;
    QDoubleSpinBox* _exponentSpin = nullptr;
    QComboBox* _sineCombo = nullptr;
    QDoubleSpinBox* _freqRSpin = nullptr;
    QDoubleSpinBox* _freqGSpin = nullptr;
    QDoubleSpinBox* _freqBSpin = nullptr;
    QDoubleSpinBox* _freqMultSpin = nullptr;
    QWidget* _sinePreview = nullptr;
    QPushButton* _importSineButton = nullptr;
    QPushButton* _saveSineButton = nullptr;
    QPushButton* _randomizeSineButton = nullptr;
    QComboBox* _paletteCombo = nullptr;
    QDoubleSpinBox* _paletteLengthSpin = nullptr;
    QDoubleSpinBox* _paletteOffsetSpin = nullptr;
    QPushButton* _paletteEditorButton = nullptr;
    QWidget* _palettePreviewLabel = nullptr;
    QLineEdit* _lightRealEdit = nullptr;
    QLineEdit* _lightImagEdit = nullptr;
    QPushButton* _lightColorButton = nullptr;
    QCheckBox* _preserveRatioCheck = nullptr;
    QSpinBox* _outputWidthSpin = nullptr;
    QSpinBox* _outputHeightSpin = nullptr;
    QLabel* _imageMemoryLabel = nullptr;
    QPushButton* _calculateButton = nullptr;
    QPushButton* _homeButton = nullptr;
    QPushButton* _zoomButton = nullptr;
    QPushButton* _saveButton = nullptr;
    QPushButton* _resizeButton = nullptr;
    QMenuBar* _menuBar = nullptr;
    QMenu* _fileMenu = nullptr;
    QMenu* _viewMenu = nullptr;
    QMenu* _renderMenu = nullptr;
    QMenu* _helpMenu = nullptr;
    QAction* _homeAction = nullptr;
    QAction* _openViewAction = nullptr;
    QAction* _saveViewAction = nullptr;
    QAction* _saveImageAction = nullptr;
    QAction* _quickSaveAction = nullptr;
    QAction* _settingsAction = nullptr;
    QAction* _exitAction = nullptr;
    QAction* _calculateAction = nullptr;
    QAction* _cancelRenderAction = nullptr;
    QAction* _cycleModeAction = nullptr;
    QAction* _cycleGridAction = nullptr;
    QAction* _minimalUiAction = nullptr;
    QAction* _fullscreenAction = nullptr;
    QAction* _helpAction = nullptr;
    QAction* _aboutAction = nullptr;
    GUILocale& _locale;
    AppSettings _settings;
    Shortcuts _shortcuts;
    NavMode _navMode = NavMode::realtimeZoom;
    std::optional<NavMode> _displayedNavModeOverride;
    SelectionTarget _selectionTarget = SelectionTarget::zoomPoint;

    void _buildUI();
    void _populateControls();
    void _connectUI();
    void _initializeState();
    void _loadSelectedBackend();
    void _switchBackend(int rank, uint64_t requestId,
        uint64_t backendGeneration,
        const std::optional<PendingPickAction>& pick);
    [[nodiscard]] QString _backendForRank(int rank) const;
    void _bindBackendCallbacks();
    void _startRenderWorker();
    bool _applyStateToSession(const GUIState& state,
        const QString& pointRealText, const QString& pointImagText,
        const QString& zoomText, const QString& seedRealText,
        const QString& seedImagText,
        const std::optional<PendingPickAction>& pickAction,
        QString& errorMessage);
    void _finishRenderThread(
        bool forceKillOnTimeout = false, int timeoutMs = 0);
    void _refreshPalettePreview();
    void _updateSinePreview();
    void _updatePreview(const QImage& image,
        bool clearInteractionPreview = true, bool usesBackendMemory = false);
    void _updateStatusLabels(const QString& rightText = QString());
    void _setStatusMessage(const QString& message);
    void _setStatusSavedPath(const QString& path);
    void _updateLightColorButton();
    void _updateModeEnablement();
    void _updateViewport();
    void _requestViewportRepaint();
    void _resizeViewportWindowToImageSize();
    void _freezeViewportPreview(
        bool disableViewport = false, bool clearInteractionPreview = false);
    void _setViewportInteractive(bool enabled);
    void _shutdownForExit(bool closeViewportWindow);
    void _updateControlWindowSize();
    void _positionWindowsForInitialShow();
    void _updateStatusRightEdgeAlignment();
    void _handleRenderFailure(const QString& message);
    void _showHelpDialog();
    void _showAboutDialog();
    void _showSettingsDialog();
    void _applyShortcutSettings();
    void _retranslateMenus();
    void _retranslateDynamicControls();
    void _loadGUISettings();
    void _saveGUISettings();
    void _syncStateFromControls();
    void _syncStateReadouts();
    void _syncControlsFromState();
    void _refreshSineList(const QString& preferredName = QString());
    void _refreshPaletteList(const QString& preferredName = QString());
    void _createNewSinePalette(bool requestRenderOnSuccess = true);
    void _createNewColorPalette(bool requestRenderOnSuccess = true);
    void _importSine();
    void _saveSine();
    void _savePointView();
    void _loadPointView();
    void _openPaletteEditor();
    void _saveImage();
    void _resizeViewportImage();
    void _updateSelectionTarget(int index);
    void _updateNavMode(int index);
    void _updateAspectLinkedSizes(bool widthChanged);
    void _prepareInteractionPreview(bool forceFallbackPreview);
    void _cycleViewportGrid();
    void _toggleViewportMinimalUI();
    bool _commitImageSave(
        const QString& path, bool appendDate, const QString& type);
    bool _loadSineByName(const QString& name,
        bool requestRenderOnSuccess = true, QString* errorMessage = nullptr);
    bool _loadPaletteByName(const QString& name,
        bool requestRenderOnSuccess = true, QString* errorMessage = nullptr);
    void _markSineSavedState(
        const QString& name, const Backend::SinePaletteConfig& palette);
    void _markPaletteSavedState(
        const QString& name, const Backend::PaletteHexConfig& palette);
    void _markPointViewSavedState();
    [[nodiscard]] bool _isSineDirty() const;
    [[nodiscard]] bool _isPaletteDirty() const;
    [[nodiscard]] bool _isPointViewDirty() const;
    void _refreshDirtyComboLabels();
    [[nodiscard]] QString _stateToString(
        double value, int precision = 17) const;
    void _syncZoomTextFromState();
    void _syncStateZoomFromText();
    void _syncPointTextFromState();
    void _syncStatePointFromText();
    void _syncSeedTextFromState();
    void _syncStateSeedFromText();
    [[nodiscard]] SavedPointViewState _capturePointViewState() const;
    [[nodiscard]] QPoint _clampPixelToOutput(const QPoint& pixel) const;
    [[nodiscard]] double _currentZoomFactor(int sliderValue) const;
    [[nodiscard]] int _resolveCurrentIterationCount() const;
    [[nodiscard]] bool _ensureBackendReady(QString& errorMessage) const;
    [[nodiscard]] bool _ensureNavigationSessionReady(
        QString& errorMessage) const;
    [[nodiscard]] bool _applyNavigationStateToSession(QString& errorMessage);
    bool _backendPanPointByDelta(const QPoint& delta, QString& realText,
        QString& imagText, QString& errorMessage);
    bool _backendPointAtPixel(const QPoint& pixel, QString& realText,
        QString& imagText, QString& errorMessage);
    bool _backendZoomViewAtPixel(const QPoint& pixel, double scaleMultiplier,
        ViewTextState& view, QString& errorMessage);
    bool _backendBoxZoomView(
        const QRect& selectionRect, ViewTextState& view, QString& errorMessage);
};
