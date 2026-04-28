#include "ControlWindow.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <climits>
#include <filesystem>
#include <cmath>
#include <optional>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "util/IncludeWin32.h"

#ifdef _WIN32
#include <shobjidl.h>
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Shell32.lib")
#endif

#include <QApplication>
#include <QBrush>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QInputDialog>
#include <QIntValidator>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QPushButton>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QScreen>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStringList>
#include <QStyle>
#include <QUrl>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWindow>

#include "dialogs/about/AboutDialog.h"
#include "dialogs/help/HelpDialog.h"
#include "dialogs/palette/PaletteDialog.h"
#include "dialogs/settings/SettingsDialog.h"
#include "services/BackendCatalog.h"
#include "services/NativeFileDialog.h"
#include "services/PaletteStore.h"
#include "services/PointStore.h"
#include "services/SineStore.h"
#include "util/GUIUtil.h"
#include "windows/viewport/ViewportWindow.h"
#include "ui_ControlWindow.h"
#include "widgets/AdaptiveDoubleSpinBox.h"
#include "widgets/CollapsibleGroupBox.h"
#include "widgets/IterationSpinBox.h"
#include "widgets/MarqueeLabel.h"
#include "widgets/PalettePreviewWidget.h"
#include "widgets/SinePreviewWidget.h"
#include "CPUInfo.h"
#include "parsers/palette/PaletteParser.h"
#include "parsers/palette/PaletteWriter.h"
#include "parsers/point/PointParser.h"
#include "parsers/point/PointWriter.h"
#include "parsers/sine/SineParser.h"
#include "parsers/sine/SineWriter.h"
#include "util/FormatUtil.h"
#include "util/NumberUtil.h"
#include "util/PathUtil.h"
#include "util/StringUtil.h"

static constexpr auto kNewEntryLabel = "+ New";
static constexpr auto kNewColorPaletteBaseName = "New Palette";
static constexpr auto kNewSinePaletteBaseName = "New Sine";
static constexpr int kControlWindowScreenPadding = 48;

static int controlScrollBarExtent(const QScrollArea* area) {
    if (!area) return 0;
    if (const QScrollBar* scrollBar = area->verticalScrollBar()) {
        return std::max(0, scrollBar->sizeHint().width());
    }
    return 0;
}

ControlWindow::ControlWindow(GUILocale& locale, QWidget* parent)
    : QMainWindow(parent)
    , _locale(locale)
    , _shortcuts(_settings) {
    qApp->installEventFilter(this);
    _buildUI();
    _populateControls();
    if (_startupBackendError) return;
    _initializeState();
    if (_startupBackendError) return;
    _connectUI();
    _loadSelectedBackend();
    if (_startupBackendError) return;
    _syncControlsFromState();
    _updateModeEnablement();
    _updateControlWindowSize();
    _loadGUISettings();

    _viewport = new ViewportWindow(this);
    _viewport->show();
    _resizeViewportWindowToImageSize();
    requestRender(true);
}

ControlWindow::~ControlWindow() {
    qApp->removeEventFilter(this);
    _shutdownForExit(true);
}

void ControlWindow::_buildUI() {
    setWindowFlag(Qt::Window, true);
    _ui = std::make_unique<Ui::ControlWindow>();
    _ui->setupUi(this);

    _menuBar = _ui->menuBar;
    _fileMenu = _ui->menuFile;
    _viewMenu = _ui->menuView;
    _renderMenu = _ui->menuRender;
    _helpMenu = _ui->menuHelp;
    _homeAction = _ui->actionHome;
    _openViewAction = _ui->actionOpenView;
    _saveViewAction = _ui->actionSaveView;
    _saveImageAction = _ui->actionSaveImage;
    _quickSaveAction = _ui->actionQuickSave;
    _settingsAction = _ui->actionSettings;
    _exitAction = _ui->actionExit;
    _cycleModeAction = _ui->actionCycleMode;
    _cycleGridAction = _ui->actionCycleGrid;
    _minimalUiAction = _ui->actionMinimalUi;
    _fullscreenAction = _ui->actionFullscreen;
    _calculateAction = _ui->actionCalculate;
    _cancelRenderAction = _ui->actionCancelRender;
    _helpAction = _ui->actionHelp;
    _aboutAction = _ui->actionAbout;

    _controlScrollArea = _ui->controlScrollArea;
    _controlScrollContent = _ui->controlScrollContent;
    _cpuGroup = _ui->cpuGroup;
    _renderGroup = _ui->renderGroup;
    _infoGroup = _ui->infoGroup;
    _viewportGroup = _ui->viewportGroup;
    _sineGroup = _ui->sineGroup;
    _paletteGroup = _ui->paletteGroup;
    _lightGroup = _ui->lightGroup;
    _cpuNameEdit = _ui->cpuNameEdit;
    _cpuCoresEdit = _ui->cpuCoresEdit;
    _cpuThreadsEdit = _ui->cpuThreadsEdit;
    _useThreadsCheckBox = _ui->useThreadsCheckBox;
    _backendCombo = _ui->backendCombo;
    _iterationsSpin = _ui->iterationsSpin;
    _colorMethodCombo = _ui->colorMethodCombo;
    _fractalCombo = _ui->fractalCombo;
    _juliaCheck = _ui->juliaCheck;
    _inverseCheck = _ui->inverseCheck;
    _infoRealEdit = _ui->infoRealEdit;
    _infoImagEdit = _ui->infoImagEdit;
    _infoZoomEdit = _ui->infoZoomEdit;
    _infoSeedRealEdit = _ui->infoSeedRealEdit;
    _infoSeedImagEdit = _ui->infoSeedImagEdit;
    _savePointButton = _ui->savePointButton;
    _loadPointButton = _ui->loadPointButton;
    _navModeCombo = _ui->navModeCombo;
    _pickTargetCombo = _ui->pickTargetCombo;
    _panRateSlider = _ui->panRateSlider;
    _panRateSpin = _ui->panRateSpin;
    _zoomRateSlider = _ui->zoomRateSlider;
    _zoomRateSpin = _ui->zoomRateSpin;
    _exponentSlider = _ui->exponentSlider;
    _exponentSpin = _ui->exponentSpin;
    _sineCombo = _ui->sineCombo;
    _freqRSpin = _ui->freqRSpin;
    _freqGSpin = _ui->freqGSpin;
    _freqBSpin = _ui->freqBSpin;
    _freqMultSpin = _ui->freqMultSpin;
    _sinePreview = _ui->sinePreview;
    _randomizeSineButton = _ui->randomizeSineButton;
    _importSineButton = _ui->importSineButton;
    _saveSineButton = _ui->saveSineButton;
    _paletteCombo = _ui->paletteCombo;
    _paletteLengthSpin = _ui->paletteLengthSpin;
    _paletteOffsetSpin = _ui->paletteOffsetSpin;
    _paletteLoadButton = _ui->paletteLoadButton;
    _paletteSaveButton = _ui->paletteSaveButton;
    _paletteEditorButton = _ui->paletteEditorButton;
    _palettePreviewLabel = _ui->palettePreviewLabel;
    _lightRealEdit = _ui->lightRealEdit;
    _lightImagEdit = _ui->lightImagEdit;
    _lightColorButton = _ui->lightColorButton;
    _outputWidthSpin = _ui->outputWidthSpin;
    _outputHeightSpin = _ui->outputHeightSpin;
    _resizeButton = _ui->resizeButton;
    _aaSpin = _ui->aaSpin;
    _preserveRatioCheck = _ui->preserveRatioCheck;
    _imageMemoryLabel = _ui->imageMemoryLabel;
    _calculateButton = _ui->calculateButton;
    _homeButton = _ui->homeButton;
    _zoomButton = _ui->zoomButton;
    _saveButton = _ui->saveButton;
    _progressLabel = _ui->progressLabel;
    _progressBar = _ui->progressBar;
    _pixelsPerSecondLabel = _ui->pixelsPerSecondLabel;
    _statusRightLabel = _ui->statusRightLabel;

    const auto onSectionToggled = [this](bool) { _updateControlWindowSize(); };
    const std::array<QGroupBox*, 8> groups = { _cpuGroup, _renderGroup,
        _infoGroup, _viewportGroup, _sineGroup, _paletteGroup, _lightGroup,
        static_cast<QGroupBox*>(_ui->imageGroup) };
    for (QGroupBox* group : groups) {
        auto* collapsible = qobject_cast<::CollapsibleGroupBox*>(group);
        if (!collapsible) continue;
        QObject::connect(collapsible, &::CollapsibleGroupBox::expandedChanged,
            this, onSectionToggled);
        collapsible->applyExpandedState(collapsible->isChecked());
    }

    if (auto* spin = qobject_cast<::IterationSpinBox*>(_iterationsSpin)) {
        spin->resolveAutoIterations
            = [this]() { return _resolveCurrentIterationCount(); };
    }
    if (QLineEdit* iterationsEdit = _iterationsSpin->findChild<QLineEdit*>()) {
        iterationsEdit->installEventFilter(this);
    }

    if (auto* spin = qobject_cast<::AdaptiveDoubleSpinBox*>(_exponentSpin)) {
        spin->setDefaultDisplayDecimals(2);
    }

    QDoubleSpinBox compactSpinProbe;
    compactSpinProbe.setRange(0.0001, 1000.0);
    compactSpinProbe.setDecimals(4);
    compactSpinProbe.setSingleStep(0.01);
    const int compactSpinWidth = compactSpinProbe.sizeHint().width();
    for (QDoubleSpinBox* spin : { _freqRSpin, _freqGSpin, _freqBSpin,
             _freqMultSpin, _paletteLengthSpin, _paletteOffsetSpin }) {
        spin->setFixedWidth(compactSpinWidth);
        if (auto* adaptive = qobject_cast<::AdaptiveDoubleSpinBox*>(spin)) {
            adaptive->setDefaultDisplayDecimals(4);
        }
    }

    for (QPushButton* button : { _savePointButton, _loadPointButton,
             _randomizeSineButton, _importSineButton, _saveSineButton,
             _paletteLoadButton, _paletteSaveButton, _paletteEditorButton,
             _lightColorButton, _resizeButton, _calculateButton, _homeButton,
             _zoomButton, _saveButton }) {
        GUI::Util::stabilizePushButton(button);
    }

    QObject::connect(qobject_cast<::SinePreviewWidget*>(_sinePreview),
        &::SinePreviewWidget::rangeChanged, this,
        [this](double, double) { _updateSinePreview(); });

    _ui->statusLayout->setStretch(
        _ui->statusLayout->indexOf(_statusRightLabel), 1);
    QObject::connect(_statusRightLabel, &MarqueeLabel::layoutWidthChanged, this,
        [this]() {
            _updateStatusRightEdgeAlignment();
            _updateStatusLabels();
        });

    _previewStillTimer.setSingleShot(true);
    _previewStillTimer.setInterval(kPreviewStillMs);
    QObject::connect(&_previewStillTimer, &QTimer::timeout, this, [this]() {
        _tryResumeDirectPreview();
    });

    _applyShortcutSettings();
}
void ControlWindow::_populateControls() {
    QString backendError;
    const QStringList backendNames
        = GUI::BackendCatalog::listNames(backendError);
    _backendNames = backendNames;
    _backendCombo->addItems(backendNames);
    if (backendNames.isEmpty()) {
        _startupBackendError = true;
        QMessageBox::critical(this, tr("Backend"), backendError);
        QTimer::singleShot(0, this, [this]() { close(); });
        return;
    }

    _colorMethodCombo->addItems({ tr("Iterations"), tr("Smooth Iterations"),
        tr("Palette"), tr("Light") });
    _fractalCombo->addItems(
        { tr("Mandelbrot"), tr("Perpendicular"), tr("Burning Ship") });
    _navModeCombo->addItems({ tr("Realtime Zoom"), tr("Box Zoom"), tr("Pan") });
    _pickTargetCombo->addItems(
        { tr("Zoom Point"), tr("Seed Point"), tr("Light Point") });
}

void ControlWindow::_initializeState() {
    const QString preferredType = QString::fromUtf8(kDefaultBackendType);
    int defaultIndex = -1;
    const QString suffixNeedle = QString(" - %1").arg(preferredType);
    for (int i = 0; i < _backendCombo->count(); ++i) {
        const QString backendName = _backendCombo->itemText(i).trimmed();
        if (backendName.compare(preferredType, Qt::CaseInsensitive) == 0
            || backendName.endsWith(suffixNeedle, Qt::CaseInsensitive)) {
            defaultIndex = i;
            break;
        }
    }
    const QString preferredBackend = _settings.preferredBackend();
    if (!preferredBackend.isEmpty()) {
        const int savedIndex = _backendCombo->findText(preferredBackend);
        if (savedIndex >= 0) {
            defaultIndex = savedIndex;
        }
    }
    if (defaultIndex < 0) {
        defaultIndex
            = _backendCombo->findText(preferredType, Qt::MatchContains);
    }
    if (defaultIndex < 0 && _backendCombo->count() > 0) {
        defaultIndex = 0;
    }
    if (defaultIndex < 0) {
        _startupBackendError = true;
        QMessageBox::critical(
            this, tr("Backend"), tr("No backend options are available."));
        QTimer::singleShot(0, this, [this]() { close(); });
        return;
    }

    _backendCombo->setCurrentIndex(defaultIndex);
    _state.backend = _backendCombo->itemText(defaultIndex);
    _state.iterations = 0;
    _state.interactionTargetFPS = _settings.previewFallbackFPS();
    _state.outputWidth = _settings.selectedOutputWidth();
    _state.outputHeight = _settings.selectedOutputHeight();
    _state.sineName = GUI::SineStore::kDefaultName;
    _refreshSineList(_state.sineName);
    QString sineError;
    if (!_loadSineByName(_state.sineName, false, &sineError)) {
        bool loaded = false;
        const QStringList sineNames = GUI::SineStore::listNames();
        if (!sineNames.isEmpty()) {
            loaded = _loadSineByName(sineNames.front(), false, &sineError);
        }
        if (!loaded) {
            _createNewSinePalette(false);
        }
    }

    _state.paletteName = GUI::PaletteStore::kDefaultName;
    _refreshPaletteList(_state.paletteName);
    QString paletteError;
    if (!_loadPaletteByName(_state.paletteName, false, &paletteError)) {
        bool loaded = false;
        const QStringList paletteNames = GUI::PaletteStore::listNames();
        if (!paletteNames.isEmpty()) {
            loaded = _loadPaletteByName(
                paletteNames.front(), false, &paletteError);
        }
        if (!loaded) {
            _createNewColorPalette(false);
        }
    }

    const CPUInfo cpu = queryCPUInfo();
    _cpuNameEdit->setText(QString::fromStdString(cpu.name));
    if (cpu.cores > 0) _cpuCoresEdit->setText(QString::number(cpu.cores));
    if (cpu.threads > 0) _cpuThreadsEdit->setText(QString::number(cpu.threads));

    _setStatusMessage(tr("Ready"));
    _mouseText = tr("Mouse: -");
    _viewportRenderTimeText.clear();
    _imageMemoryText = tr("Render: -  Output: -");
    _lastPreviewMotionAt = std::chrono::steady_clock::now();
    _syncZoomTextFromState();
    _syncPointTextFromState();
    _syncSeedTextFromState();
    _displayedPointRealText = _pointRealText;
    _displayedPointImagText = _pointImagText;
    _displayedZoomText = _zoomText;
    _displayedOutputSize = outputSize();
    _hasDisplayedViewState = true;
    _markPointViewSavedState();
}

void ControlWindow::_connectUI() {
    QObject::connect(_backendCombo, &QComboBox::currentTextChanged, this,
        [this](const QString& text) {
            _state.backend = text;
            _state.manualBackendSelection = true;
            _loadSelectedBackend();
        });

    QObject::connect(
        _useThreadsCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
            _state.useThreads = checked;
            _freezeViewportPreview(false, false);
        });

    auto requestFromControls = [this]() {
        _syncStateFromControls();
        _updateSinePreview();
        requestRender();
    };
    auto confirmDiscardDirtySine = [this]() {
        if (!_isSineDirty()) return true;

        const QMessageBox::StandardButton choice
            = QMessageBox::question(this, "Sine Color",
                "Current sine palette has unsaved changes. Discard them?",
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (choice != QMessageBox::Yes) {
            _refreshSineList(_state.sineName);
            return false;
        }

        if (_hasSavedSineState) {
            _state.sineName = _savedSineName;
            _state.sinePalette = _savedSinePalette;
        }
        _refreshSineList(_state.sineName);
        _syncControlsFromState();
        _updateViewport();
        return true;
    };
    auto confirmDiscardDirtyPalette = [this]() {
        if (!_isPaletteDirty()) return true;

        const QMessageBox::StandardButton choice = QMessageBox::question(this,
            "Palette", "Current palette has unsaved changes. Discard them?",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (choice != QMessageBox::Yes) {
            _refreshPaletteList(_state.paletteName);
            return false;
        }

        if (_hasSavedPaletteState) {
            _state.paletteName = _savedPaletteName;
            _state.palette = _savedPalette;
            _palettePreviewDirty = true;
        }
        _refreshPaletteList(_state.paletteName);
        _syncControlsFromState();
        _updateViewport();
        return true;
    };

    QObject::connect(_iterationsSpin,
        QOverload<int>::of(&QSpinBox::valueChanged), this,
        [requestFromControls](int) { requestFromControls(); });
    QObject::connect(_colorMethodCombo,
        QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
            _syncStateFromControls();
            _updateModeEnablement();
            requestRender();
        });
    QObject::connect(_fractalCombo,
        QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [requestFromControls](int) { requestFromControls(); });
    QObject::connect(_juliaCheck, &QCheckBox::toggled, this,
        [requestFromControls](bool) { requestFromControls(); });
    QObject::connect(_inverseCheck, &QCheckBox::toggled, this,
        [requestFromControls](bool) { requestFromControls(); });
    QObject::connect(_aaSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
        [requestFromControls](int) { requestFromControls(); });

    QObject::connect(_navModeCombo,
        QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this](int index) { _updateNavMode(index); });
    QObject::connect(_pickTargetCombo,
        QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this](int index) { _updateSelectionTarget(index); });

    QObject::connect(
        _panRateSlider, &QSlider::valueChanged, this, [this](int value) {
            _panRateSpin->blockSignals(true);
            _panRateSpin->setValue(value);
            _panRateSpin->blockSignals(false);
            _state.panRate = value;
        });
    QObject::connect(_panRateSpin, QOverload<int>::of(&QSpinBox::valueChanged),
        this, [this](int value) {
            const QSignalBlocker sliderBlocker(_panRateSlider);
            _panRateSlider->setValue(std::clamp(
                value, _panRateSlider->minimum(), _panRateSlider->maximum()));
            _state.panRate = value;
        });

    QObject::connect(
        _zoomRateSlider, &QSlider::valueChanged, this, [this](int value) {
            _zoomRateSpin->blockSignals(true);
            _zoomRateSpin->setValue(value);
            _zoomRateSpin->blockSignals(false);
            _state.zoomRate = value;
        });
    QObject::connect(_zoomRateSpin, QOverload<int>::of(&QSpinBox::valueChanged),
        this, [this](int value) {
            const QSignalBlocker sliderBlocker(_zoomRateSlider);
            _zoomRateSlider->setValue(std::clamp(
                value, _zoomRateSlider->minimum(), _zoomRateSlider->maximum()));
            _state.zoomRate = value;
        });

    QObject::connect(
        _exponentSlider, &QSlider::valueChanged, this, [this](int value) {
            const double exponent = value / 100.0;
            _exponentSpin->blockSignals(true);
            GUI::Util::setAdaptiveSpinValue(_exponentSpin, exponent);
            _exponentSpin->blockSignals(false);
            _state.exponent = exponent;
            requestRender();
        });
    QObject::connect(_exponentSpin,
        QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
        [this](double value) {
            const QSignalBlocker sliderBlocker(_exponentSlider);
            const int sliderValue
                = std::clamp(static_cast<int>(std::round(value * 100.0)),
                    _exponentSlider->minimum(), _exponentSlider->maximum());
            _exponentSlider->setValue(sliderValue);
            _state.exponent = value;
            requestRender();
        });
    QObject::connect(_sineCombo, &QComboBox::currentTextChanged, this,
        [this, confirmDiscardDirtySine](const QString& name) {
            if (!confirmDiscardDirtySine()) return;

            const QString normalizedName = GUI::Util::undecoratedLabel(name);
            if (normalizedName == kNewEntryLabel) {
                _createNewSinePalette();
                return;
            }
            if (normalizedName.isEmpty()) return;

            QString errorMessage;
            if (!_loadSineByName(normalizedName, true, &errorMessage)
                && !errorMessage.isEmpty()) {
                QMessageBox::warning(this, "Sine Color", errorMessage);
                _refreshSineList(_state.sineName);
                requestRender();
            }
        });
    QObject::connect(_freqRSpin,
        QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
        [requestFromControls](double) { requestFromControls(); });
    QObject::connect(_freqGSpin,
        QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
        [requestFromControls](double) { requestFromControls(); });
    QObject::connect(_freqBSpin,
        QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
        [requestFromControls](double) { requestFromControls(); });
    QObject::connect(_freqMultSpin,
        QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
        [requestFromControls](double) { requestFromControls(); });
    QObject::connect(
        _randomizeSineButton, &QPushButton::clicked, this, [this]() {
            auto randomFrequency = []() {
                constexpr double minValue = 0.5;
                constexpr double maxValue = 1.5;
                const double value = minValue
                    + (maxValue - minValue)
                        * QRandomGenerator::global()->generateDouble();
                return std::round(value * 10000.0) / 10000.0;
            };

            const QSignalBlocker rBlocker(_freqRSpin);
            const QSignalBlocker gBlocker(_freqGSpin);
            const QSignalBlocker bBlocker(_freqBSpin);
            GUI::Util::setAdaptiveSpinValue(_freqRSpin, randomFrequency());
            GUI::Util::setAdaptiveSpinValue(_freqGSpin, randomFrequency());
            GUI::Util::setAdaptiveSpinValue(_freqBSpin, randomFrequency());

            _syncStateFromControls();
            _updateSinePreview();
            requestRender();
        });
    QObject::connect(_saveSineButton, &QPushButton::clicked, this,
        [this]() { _saveSine(); });
    QObject::connect(_importSineButton, &QPushButton::clicked, this,
        [this]() { _importSine(); });
    QObject::connect(_savePointButton, &QPushButton::clicked, this,
        [this]() { _savePointView(); });
    QObject::connect(_loadPointButton, &QPushButton::clicked, this,
        [this]() { _loadPointView(); });
    auto applyViewTextEdits = [this]() {
        _syncStateFromControls();
        _syncStateReadouts();
        requestRender();
    };
    auto connectViewTextEdit = [this, applyViewTextEdits](QLineEdit* edit) {
        if (!edit) return;
        GUI::Util::markLineEditTextCommitted(edit);
        QObject::connect(edit, &QLineEdit::editingFinished, this,
            [edit, applyViewTextEdits]() {
                if (!GUI::Util::hasUncommittedLineEditChange(edit)) return;
                applyViewTextEdits();
            });
    };
    connectViewTextEdit(_infoRealEdit);
    connectViewTextEdit(_infoImagEdit);
    connectViewTextEdit(_infoZoomEdit);
    connectViewTextEdit(_infoSeedRealEdit);
    connectViewTextEdit(_infoSeedImagEdit);

    QObject::connect(_paletteCombo, &QComboBox::currentTextChanged, this,
        [this, confirmDiscardDirtyPalette](const QString& name) {
            if (!confirmDiscardDirtyPalette()) return;

            const QString normalizedName = GUI::Util::undecoratedLabel(name);
            if (normalizedName == kNewEntryLabel) {
                _createNewColorPalette();
                return;
            }
            if (normalizedName.isEmpty()) return;

            QString errorMessage;
            if (!_loadPaletteByName(normalizedName, true, &errorMessage)
                && !errorMessage.isEmpty()) {
                QMessageBox::warning(this, "Palette", errorMessage);
                _refreshPaletteList(_state.paletteName);
                requestRender();
            }
        });
    QObject::connect(_paletteLengthSpin,
        QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
        [this](double value) {
            _state.palette.totalLength = static_cast<float>(value);
            _state.palette = GUI::PaletteStore::stopsToConfig(
                GUI::PaletteStore::configToStops(_state.palette),
                _state.palette.totalLength, _state.palette.offset,
                _state.palette.blendEnds);
            _palettePreviewDirty = true;
            _updateViewport();
            requestRender();
        });
    QObject::connect(_paletteOffsetSpin,
        QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
        [this](double value) {
            _state.palette.offset = static_cast<float>(value);
            _palettePreviewDirty = true;
            _updateViewport();
            requestRender();
        });
    QObject::connect(_paletteLoadButton, &QPushButton::clicked, this,
        [this]() { _loadPalette(); });
    QObject::connect(_paletteSaveButton, &QPushButton::clicked, this,
        [this]() { _savePalette(); });
    QObject::connect(_paletteEditorButton, &QPushButton::clicked, this,
        [this]() { _openPaletteEditor(); });
    QObject::connect(_lightColorButton, &QPushButton::clicked, this, [this]() {
        const QColor initial = GUI::Util::lightColorToQColor(_state.lightColor);
        const QColor color
            = QColorDialog::getColor(initial, this, "Light Color");
        if (!color.isValid()) return;

        _state.lightColor = GUI::Util::lightColorFromQColor(color);
        _updateLightColorButton();
        requestRender();
    });

    QObject::connect(_outputWidthSpin,
        QOverload<int>::of(&QSpinBox::valueChanged), this,
        [this](int) { _updateAspectLinkedSizes(true); });
    QObject::connect(_outputHeightSpin,
        QOverload<int>::of(&QSpinBox::valueChanged), this,
        [this](int) { _updateAspectLinkedSizes(false); });
    QObject::connect(_preserveRatioCheck, &QCheckBox::toggled, this,
        [this](bool checked) { _state.preserveRatio = checked; });

    QObject::connect(_calculateButton, &QPushButton::clicked, this, [this]() {
        _syncStateFromControls();
        requestRender(true);
    });
    QObject::connect(_homeButton, &QPushButton::clicked, this,
        [this]() { applyHomeView(); });
    QObject::connect(_zoomButton, &QPushButton::clicked, this, [this]() {
        if (_viewport) {
            const QSize size = outputSize();
            zoomAtPixel(QPoint(size.width() / 2, size.height() / 2), true);
        }
    });
    QObject::connect(
        _saveButton, &QPushButton::clicked, this, [this]() { _saveImage(); });
    QObject::connect(_resizeButton, &QPushButton::clicked, this,
        [this]() { _resizeViewportImage(); });
    QObject::connect(
        _homeAction, &QAction::triggered, this, [this]() { applyHomeView(); });
    QObject::connect(_openViewAction, &QAction::triggered, this,
        [this]() { _loadPointView(); });
    QObject::connect(_saveViewAction, &QAction::triggered, this,
        [this]() { _savePointView(); });
    QObject::connect(_saveImageAction, &QAction::triggered, this,
        [this]() { _saveImage(); });
    QObject::connect(_quickSaveAction, &QAction::triggered, this,
        [this]() { quickSaveImage(); });
    QObject::connect(_settingsAction, &QAction::triggered, this,
        [this]() { _showSettingsDialog(); });
    QObject::connect(_exitAction, &QAction::triggered, this,
        [this]() { requestApplicationShutdown(true); });
    QObject::connect(_calculateAction, &QAction::triggered, this, [this]() {
        _syncStateFromControls();
        requestRender(true);
    });
    QObject::connect(_cancelRenderAction, &QAction::triggered, this,
        [this]() { cancelQueuedRenders(); });
    QObject::connect(_cycleModeAction, &QAction::triggered, this,
        [this]() { cycleNavMode(); });
    QObject::connect(_cycleGridAction, &QAction::triggered, this,
        [this]() { _cycleViewportGrid(); });
    QObject::connect(_minimalUiAction, &QAction::triggered, this,
        [this]() { _toggleViewportMinimalUI(); });
    QObject::connect(_fullscreenAction, &QAction::triggered, this,
        [this]() { toggleViewportFullscreen(); });
    QObject::connect(_helpAction, &QAction::triggered, this,
        [this]() { _showHelpDialog(); });
    QObject::connect(_aboutAction, &QAction::triggered, this,
        [this]() { _showAboutDialog(); });
}

void ControlWindow::_loadSelectedBackend() {
    _backendGeneration.fetch_add(1, std::memory_order_acq_rel);
    _callbackRenderRequestId.store(0, std::memory_order_release);
    if (_backend && _renderSession) {
        _renderSession->setCallbacks({});
    }
    if (_state.backend.isEmpty()) {
        _startupBackendError = true;
        QMessageBox::critical(this, "Backend", "No backend is selected.");
        QTimer::singleShot(0, this, [this]() { close(); });
        return;
    }

    _freezeViewportPreview(true, true);
    _finishRenderThread();
    _interactionPreviewFallbackLatched.store(false, std::memory_order_release);
    _renderSession = nullptr;
    _navigationSession = nullptr;
    _previewSession = nullptr;
    _backend.reset();
    _lastPresentedRenderId = 0;
    _progressCancelled = false;

    std::string error;
    _backend = loadBackendModule(
        PathUtil::executableDir(), _state.backend.toStdString(), error);
    if (_backend) {
        _renderSession = _backend.makeSession();
        if (!_renderSession && error.empty()) {
            error = "Failed to create backend session.";
        }
    }
    if (_backend) {
        _navigationSession = _backend.makeSession();
        if (!_navigationSession && error.empty()) {
            error = "Failed to create navigation session.";
        }
    }
    if (_backend) {
        _previewSession = _backend.makeSession();
        if (!_previewSession && error.empty()) {
            error = "Failed to create preview session.";
        }
    }
    if (_renderSession && _navigationSession && _previewSession) {
        _bindBackendCallbacks();
        _startRenderWorker();
        _setViewportInteractive(true);
        _updateViewport();
        return;
    }

    const QString message = QString::fromStdString(
        error.empty() ? "Failed to load backend." : error);
    if (!_viewport) {
        _startupBackendError = true;
        QMessageBox::critical(this, "Backend", message);
        QTimer::singleShot(0, this, [this]() { close(); });
        return;
    }

    QMessageBox::warning(this, "Backend", message);
    _setViewportInteractive(true);
}

QString ControlWindow::_backendForRank(int rank) const {
    if (_backendNames.isEmpty()) return QString();

    const int targetPrecision = std::clamp(rank, 0, 3);
    const int currentType = GUI::BackendCatalog::typeRank(_state.backend);

    QString fallback;
    int fallbackPrecision = -1;
    int fallbackTypeDist = INT_MAX;
    QString best;
    int bestPenalty = INT_MAX;
    int bestTypeDist = INT_MAX;

    for (const QString& backendName : _backendNames) {
        const QString name = backendName.trimmed();
        const int precision = GUI::BackendCatalog::precisionRank(name);
        const int typeDist
            = std::abs(GUI::BackendCatalog::typeRank(name) - currentType);

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

void ControlWindow::_switchBackend(int rank, uint64_t requestId,
    uint64_t backendGeneration, const std::optional<PendingPickAction>& pick) {
    if (backendGeneration != _backendGeneration.load(std::memory_order_acquire))
        return;
    if (requestId != _latestRenderRequestId.load(std::memory_order_acquire))
        return;

    const QString backend = _backendForRank(rank);
    if (backend.isEmpty() || backend == _state.backend) return;

    _state.backend = backend;
    {
        const QSignalBlocker blocker(_backendCombo);
        _backendCombo->setCurrentText(_state.backend);
    }
    _loadSelectedBackend();
    _pendingPickAction = pick;
    requestRender(false);
}

void ControlWindow::_bindBackendCallbacks() {
    _callbacks = {};
    const uint64_t backendGeneration
        = _backendGeneration.load(std::memory_order_acquire);

    _callbacks.onProgress = [this, backendGeneration](
                                const Backend::ProgressEvent& event) {
        if (backendGeneration
            != _backendGeneration.load(std::memory_order_acquire))
            return;
        const uint64_t renderId
            = _callbackRenderRequestId.load(std::memory_order_acquire);
        if (renderId == 0) return;

        const auto now = std::chrono::steady_clock::now();
        const int64_t nowMs
            = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch())
                  .count();
        if (!event.completed && event.percentage < 100) {
            const int64_t lastMs
                = _lastProgressUIUpdateMs.load(std::memory_order_acquire);
            if (lastMs > 0 && (nowMs - lastMs) < 33) {
                return;
            }
        }
        _lastProgressUIUpdateMs.store(nowMs, std::memory_order_release);

        const QString progress = QString("%1%").arg(event.percentage);
        const QString pixelsPerSecond
            = QString("%1 pixels/s")
                  .arg(QString::fromStdString(
                      FormatUtil::formatBigNumber(event.opsPerSecond)));

        QMetaObject::invokeMethod(this,
            [this, progress, pixelsPerSecond, event, renderId,
                backendGeneration]() {
                if (backendGeneration
                    != _backendGeneration.load(std::memory_order_acquire))
                    return;
                if (renderId
                    != _callbackRenderRequestId.load(std::memory_order_acquire))
                    return;

                _progressText = progress;
                _progressValue = std::clamp(event.percentage, 0, 100);
                _progressActive = !event.completed;
                _progressCancelled = false;
                _pixelsPerSecondText = pixelsPerSecond;
                _updateStatusLabels();
            });
    };

    _callbacks.onImage = [this, backendGeneration](
                             const Backend::ImageEvent& event) {
        if (backendGeneration
            != _backendGeneration.load(std::memory_order_acquire))
            return;
        const uint64_t renderId
            = _callbackRenderRequestId.load(std::memory_order_acquire);
        if (event.kind == Backend::ImageEventKind::allocated) {
            if (renderId == 0) return;
            const QString imageMemoryText
                = GUI::Util::formatImageMemoryText(event);

            QMetaObject::invokeMethod(
                this, [this, imageMemoryText, backendGeneration]() {
                    if (backendGeneration
                        != _backendGeneration.load(std::memory_order_acquire))
                        return;
                    _imageMemoryText = imageMemoryText;
                    _updateStatusLabels();
                });
            return;
        }

        if (event.kind != Backend::ImageEventKind::saved || !event.path) return;
        const QString path = QString::fromUtf8(event.path);

        QMetaObject::invokeMethod(this, [this, path, backendGeneration]() {
            if (backendGeneration
                != _backendGeneration.load(std::memory_order_acquire))
                return;
            _setStatusSavedPath(path);
            _updateStatusLabels();
        });
    };

    _callbacks.onInfo = [this, backendGeneration](
                            const Backend::InfoEvent& event) {
        if (backendGeneration
            != _backendGeneration.load(std::memory_order_acquire))
            return;
        const uint64_t renderId
            = _callbackRenderRequestId.load(std::memory_order_acquire);
        if (renderId == 0) return;
        const QString text
            = QString("Iterations: %1 | %2 GI/s")
                  .arg(QString::fromStdString(
                      FormatUtil::formatNumber(event.totalIterations)))
                  .arg(QString::number(event.opsPerSecond, 'f', 2));

        QMetaObject::invokeMethod(
            this, [this, text, renderId, backendGeneration]() {
                if (backendGeneration
                    != _backendGeneration.load(std::memory_order_acquire))
                    return;
                if (renderId
                    != _callbackRenderRequestId.load(std::memory_order_acquire))
                    return;

                _setStatusMessage(text);
                _updateStatusLabels();
            });
    };

    _callbacks.onDebug = [this, backendGeneration](
                             const Backend::DebugEvent& event) {
        if (backendGeneration
            != _backendGeneration.load(std::memory_order_acquire))
            return;
        const uint64_t renderId
            = _callbackRenderRequestId.load(std::memory_order_acquire);
        if (renderId == 0) return;
        if (!event.message) return;
        const QString message = QString::fromUtf8(event.message);

        QMetaObject::invokeMethod(this, [this, message, backendGeneration]() {
            if (backendGeneration
                != _backendGeneration.load(std::memory_order_acquire))
                return;
            _setStatusMessage(message);
            _updateStatusLabels();
        });
    };
    _renderSession->setCallbacks(_callbacks);
}

void ControlWindow::_startRenderWorker() {
    if (_renderThread.joinable()) return;

    {
        const std::scoped_lock lock(_renderMutex);
        _renderStopRequested = false;
        _queuedRenderRequest.reset();
    }

    const uint64_t backendGeneration
        = _backendGeneration.load(std::memory_order_acquire);
    _renderThread = std::thread([this, backendGeneration]() {
        for (;;) {
            if (backendGeneration
                != _backendGeneration.load(std::memory_order_acquire))
                break;
            RenderRequest request;

            {
                std::unique_lock lock(_renderMutex);
                _renderCv.wait(lock, [this]() {
                    return _renderStopRequested
                        || _queuedRenderRequest.has_value();
                });

                if (_renderStopRequested) break;
                if (backendGeneration
                    != _backendGeneration.load(std::memory_order_acquire))
                    break;

                request = std::move(*_queuedRenderRequest);
                _queuedRenderRequest.reset();
            }

            if (_interactionPreviewFallbackLatched.load(
                    std::memory_order_acquire)) {
                std::unique_lock lock(_renderMutex);
                _renderCv.wait_for(lock,
                    std::chrono::milliseconds(interactionFrameIntervalMs()),
                    [this]() {
                        return _renderStopRequested
                            || _queuedRenderRequest.has_value();
                    });

                if (_renderStopRequested) break;
                if (backendGeneration
                    != _backendGeneration.load(std::memory_order_acquire))
                    break;
                if (_queuedRenderRequest) {
                    request = std::move(*_queuedRenderRequest);
                    _queuedRenderRequest.reset();
                }
            }

            _callbackRenderRequestId.store(
                request.id, std::memory_order_release);

            QMetaObject::invokeMethod(
                this, [this, requestId = request.id, backendGeneration]() {
                    if (backendGeneration
                        != _backendGeneration.load(std::memory_order_acquire))
                        return;
                    if (requestId
                        != _callbackRenderRequestId.load(
                            std::memory_order_acquire))
                        return;

                    _renderInFlight = true;
                    _progressActive = true;
                    _progressCancelled = false;
                    _progressValue = 0;
                    _progressText = "Rendering";
                    _pixelsPerSecondText = "0 pixels/s";
                    _lastProgressUIUpdateMs.store(0, std::memory_order_release);
                    _updateStatusLabels();
                });

            QString error;
            int rank = 0;
            Backend::Status status;
            Backend::ImageView view;
            auto renderStart = std::chrono::steady_clock::now();
            auto renderEnd = renderStart;
            {
                if (!_applyStateToSession(request.state, request.pointRealText,
                        request.pointImagText, request.zoomText,
                        request.seedRealText, request.seedImagText,
                        request.pickAction, error)) {
                    status = Backend::Status::failure(error.toStdString());
                } else {
                    rank = _renderSession->precisionRank();
                    if (!request.state.manualBackendSelection
                        && _backendForRank(rank) != request.state.backend) {
                        status = Backend::Status::success();
                    } else {
                        renderStart = std::chrono::steady_clock::now();
                        status = _renderSession->render();
                        renderEnd = std::chrono::steady_clock::now();
                    }
                }
            }

            const bool staleRequest = backendGeneration
                    != _backendGeneration.load(std::memory_order_acquire)
                || request.id
                    != _callbackRenderRequestId.load(std::memory_order_acquire);
            if (staleRequest) {
                continue;
            }

            if (!status) {
                const QString failureMessage = error.isEmpty()
                    ? QString::fromStdString(status.message)
                    : error;
                {
                    const std::scoped_lock lock(_renderMutex);
                    _queuedRenderRequest.reset();
                }

                QMetaObject::invokeMethod(this,
                    [this, failureMessage, requestId = request.id,
                        backendGeneration]() {
                        if (backendGeneration
                            != _backendGeneration.load(
                                std::memory_order_acquire))
                            return;
                        if (requestId
                            != _latestRenderRequestId.load(
                                std::memory_order_acquire))
                            return;

                        _renderInFlight = false;
                        _progressActive = false;
                        _progressCancelled = false;
                        _progressValue = 0;
                        _progressText.clear();
                        _pixelsPerSecondText = "0 pixels/s";
                        _handleRenderFailure(failureMessage);
                        _updateStatusLabels();
                    });
                continue;
            }

            view = _renderSession->imageView();
            if (!request.state.manualBackendSelection
                && _backendForRank(rank) != request.state.backend) {
                QMetaObject::invokeMethod(this,
                    [this, rank, requestId = request.id, backendGeneration,
                        pick = request.pickAction]() {
                        _switchBackend(
                            rank, requestId, backendGeneration, pick);
                    });
                continue;
            }

            const auto renderElapsed
                = std::chrono::duration_cast<std::chrono::milliseconds>(
                    renderEnd - renderStart);
            const qint64 renderMs = std::max<qint64>(1, renderElapsed.count());
            const double renderFPS = 1000.0 / static_cast<double>(renderMs);
            _lastRenderDurationMs.store(
                static_cast<int32_t>(renderMs), std::memory_order_release);
            const int fallbackFPS = std::max(0, _state.interactionTargetFPS);
            const int targetFrameMs = fallbackFPS > 0
                ? std::max(1,
                      static_cast<int>(std::lround(
                          1000.0 / static_cast<double>(fallbackFPS))))
                : INT_MAX;
            const int recoveryFrameMs
                = fallbackFPS > 0 ? std::max(1, targetFrameMs / 2) : INT_MAX;
            bool previewFallbackLatched
                = _interactionPreviewFallbackLatched.load(
                    std::memory_order_acquire);
            if (fallbackFPS <= 0) {
                previewFallbackLatched = false;
            } else if (previewFallbackLatched) {
                if (renderMs <= recoveryFrameMs) {
                    previewFallbackLatched = false;
                }
            } else if (renderMs > targetFrameMs) {
                previewFallbackLatched = true;
            }
            _interactionPreviewFallbackLatched.store(
                previewFallbackLatched, std::memory_order_release);

            const GUIState renderedState = request.state;
            const QString renderedPointReal = request.pointRealText;
            const QString renderedPointImag = request.pointImagText;
            const QString renderedZoomText = request.zoomText;
            if (backendGeneration
                != _backendGeneration.load(std::memory_order_acquire)) {
                continue;
            }
            QMetaObject::invokeMethod(
                this,
                [this, view, previewFallbackLatched, renderFPS, renderMs,
                    renderedState, renderedPointReal, renderedPointImag,
                    renderedZoomText, requestId = request.id,
                    backendGeneration]() {
                    if (backendGeneration
                        != _backendGeneration.load(std::memory_order_acquire))
                        return;
                    if (requestId <= _lastPresentedRenderId) return;
                    _lastPresentedRenderId = requestId;

                    const bool currentRender = requestId
                        == _callbackRenderRequestId.load(
                            std::memory_order_acquire);
                    const bool latestRender = requestId
                        == _latestRenderRequestId.load(
                            std::memory_order_acquire);
                    if (currentRender) {
                        _renderInFlight = false;
                        _progressActive = false;
                        _progressCancelled = false;
                        _progressValue = 0;
                        _progressText.clear();
                    }
                    _lastRenderFailureMessage.clear();
                    _viewportFPSText = QString("%1 FPS").arg(
                        QString::number(renderFPS, 'f', 1));
                    _viewportRenderTimeText = QString::fromStdString(
                        FormatUtil::formatDuration(renderMs));
                    const bool presentDirect
                        = !previewFallbackLatched && latestRender;
                    const bool presentPreview
                        = previewFallbackLatched || _presentingCopiedPreview;
                    if (presentDirect && _presentingCopiedPreview) {
                        QImage releasedImage;
                        _previewImage.swap(releasedImage);
                        _previewUsesBackendMemory = false;
                    }
                    if (presentDirect) {
                        _previewImage = GUI::Util::wrapImageViewToImage(view);
                        _previewUsesBackendMemory = true;
                        _directFrameDetachedForRender = false;
                        _presentingCopiedPreview = false;
                    } else if (presentPreview) {
                        _previewImage = GUI::Util::imageViewToImage(view);
                        _previewUsesBackendMemory = false;
                        _directFrameDetachedForRender = false;
                        _presentingCopiedPreview = true;
                    } else {
                        return;
                    }
                    const QWidget* dprSource = _viewport
                        ? static_cast<QWidget*>(_viewport)
                        : static_cast<QWidget*>(this);
                    _previewImage.setDevicePixelRatio(
                        GUI::Util::effectiveDevicePixelRatio(dprSource));
                    _displayedPointRealText = renderedPointReal;
                    _displayedPointImagText = renderedPointImag;
                    _displayedZoomText = renderedZoomText;
                    _displayedOutputSize = QSize(
                        renderedState.outputWidth, renderedState.outputHeight);
                    _hasDisplayedViewState = true;
                    _updateViewport();
                    _updateStatusLabels();
                    if (_presentingCopiedPreview) {
                        _previewStillTimer.start();
                    } else {
                        _previewStillTimer.stop();
                    }
                },
                Qt::BlockingQueuedConnection);
        }
    });
}

bool ControlWindow::_ensureBackendReady(QString& errorMessage) const {
    if (_backend && _renderSession) return true;

    errorMessage = "Backend not loaded.";
    return false;
}

bool ControlWindow::_ensureNavigationSessionReady(QString& errorMessage) const {
    if (_navigationSession) return true;

    errorMessage = "Navigation session unavailable.";
    return false;
}

bool ControlWindow::_applyNavigationStateToSession(QString& errorMessage) {
    if (!_ensureNavigationSessionReady(errorMessage)) return false;

    auto failIfNeeded = [&errorMessage](const Backend::Status& status) {
        if (status) return false;
        errorMessage = QString::fromStdString(status.message);
        return true;
    };

    if (failIfNeeded(_navigationSession->setImageSize(
            _state.outputWidth, _state.outputHeight, _state.aaPixels))) {
        return false;
    }
    _navigationSession->setUseThreads(_state.useThreads);
    if (failIfNeeded(_navigationSession->setZoom(
            _state.iterations, _zoomText.toStdString()))) {
        return false;
    }
    if (failIfNeeded(_navigationSession->setPoint(
            _pointRealText.toStdString(), _pointImagText.toStdString()))) {
        return false;
    }
    if (failIfNeeded(_navigationSession->setSeed(
            _seedRealText.toStdString(), _seedImagText.toStdString()))) {
        return false;
    }
    return true;
}

bool ControlWindow::_backendPanPointByDelta(const QPoint& delta,
    QString& realText, QString& imagText, QString& errorMessage) {
    if (!_applyNavigationStateToSession(errorMessage)) return false;

    std::string real;
    std::string imag;
    const Backend::Status status = _navigationSession->getPanPointByDelta(
        delta.x(), delta.y(), real, imag);
    if (!status) {
        errorMessage = QString::fromStdString(status.message);
        return false;
    }

    realText = QString::fromStdString(real);
    imagText = QString::fromStdString(imag);
    return true;
}

bool ControlWindow::_backendPointAtPixel(const QPoint& pixel, QString& realText,
    QString& imagText, QString& errorMessage) {
    if (!_applyNavigationStateToSession(errorMessage)) return false;

    std::string real;
    std::string imag;
    const Backend::Status status
        = _navigationSession->getPointAtPixel(pixel.x(), pixel.y(), real, imag);
    if (!status) {
        errorMessage = QString::fromStdString(status.message);
        return false;
    }

    realText = QString::fromStdString(real);
    imagText = QString::fromStdString(imag);
    return true;
}

bool ControlWindow::_backendZoomViewAtPixel(const QPoint& pixel,
    double scaleMultiplier, ViewTextState& view, QString& errorMessage) {
    if (!_applyNavigationStateToSession(errorMessage)) return false;

    std::string zoom;
    std::string real;
    std::string imag;
    const Backend::Status status = _navigationSession->getZoomPointByScale(
        pixel.x(), pixel.y(), scaleMultiplier, zoom, real, imag);
    if (!status) {
        errorMessage = QString::fromStdString(status.message);
        return false;
    }

    view.pointReal = QString::fromStdString(real);
    view.pointImag = QString::fromStdString(imag);
    view.zoomText = QString::fromStdString(zoom);
    view.outputSize = outputSize();
    view.valid = true;
    return true;
}

bool ControlWindow::_backendBoxZoomView(
    const QRect& selectionRect, ViewTextState& view, QString& errorMessage) {
    if (!_applyNavigationStateToSession(errorMessage)) return false;

    const QRect rect = selectionRect.normalized();
    std::string zoom;
    std::string real;
    std::string imag;
    const Backend::Status status = _navigationSession->getBoxZoomPoint(
        rect.left(), rect.top(), rect.right(), rect.bottom(), zoom, real, imag);
    if (!status) {
        errorMessage = QString::fromStdString(status.message);
        return false;
    }

    view.pointReal = QString::fromStdString(real);
    view.pointImag = QString::fromStdString(imag);
    view.zoomText = QString::fromStdString(zoom);
    view.outputSize = outputSize();
    view.valid = true;
    return true;
}

bool ControlWindow::_applyStateToSession(const GUIState& state,
    const QString& pointRealText, const QString& pointImagText,
    const QString& zoomText, const QString& seedRealText,
    const QString& seedImagText,
    const std::optional<PendingPickAction>& pickAction, QString& errorMessage) {
    if (!_ensureBackendReady(errorMessage)) return false;

    auto failIfNeeded = [&errorMessage](const Backend::Status& status) {
        if (status) return false;
        errorMessage = QString::fromStdString(status.message);
        return true;
    };

    if (failIfNeeded(_renderSession->setImageSize(
            state.outputWidth, state.outputHeight, state.aaPixels)))
        return false;
    _renderSession->setUseThreads(state.useThreads);
    if (failIfNeeded(
            _renderSession->setZoom(state.iterations, zoomText.toStdString())))
        return false;
    if (failIfNeeded(_renderSession->setPoint(
            pointRealText.toStdString(), pointImagText.toStdString())))
        return false;
    if (failIfNeeded(_renderSession->setSeed(
            seedRealText.toStdString(), seedImagText.toStdString())))
        return false;
    if (failIfNeeded(_renderSession->setFractalType(state.fractalType)))
        return false;
    _renderSession->setFractalMode(state.julia, state.inverse);
    if (failIfNeeded(_renderSession->setFractalExponent(
            _stateToString(state.exponent).toStdString())))
        return false;
    if (failIfNeeded(_renderSession->setColorMethod(state.colorMethod)))
        return false;
    if (failIfNeeded(_renderSession->setSinePalette(state.sinePalette)))
        return false;
    if (failIfNeeded(_renderSession->setColorPalette(state.palette)))
        return false;
    if (failIfNeeded(
            _renderSession->setLight(static_cast<float>(state.light.x()),
                static_cast<float>(state.light.y()))))
        return false;
    if (failIfNeeded(_renderSession->setLightColor(state.lightColor)))
        return false;

    if (pickAction) {
        Backend::Status status;
        switch (pickAction->target) {
        case SelectionTarget::zoomPoint:
            status = _renderSession->setPoint(
                pickAction->pixel.x(), pickAction->pixel.y());
            break;
        case SelectionTarget::seedPoint:
            status = _renderSession->setSeed(
                pickAction->pixel.x(), pickAction->pixel.y());
            break;
        case SelectionTarget::lightPoint:
            status = _renderSession->setLight(
                pickAction->pixel.x(), pickAction->pixel.y());
            break;
        }

        if (failIfNeeded(status)) return false;
    }

    return true;
}

void ControlWindow::_finishRenderThread(
    bool forceKillOnTimeout, int timeoutMs) {
    _latestRenderRequestId.fetch_add(1, std::memory_order_acq_rel);
    _lastPresentedRenderId = 0;
    _callbackRenderRequestId.store(0, std::memory_order_release);

    {
        const std::scoped_lock lock(_renderMutex);
        _renderStopRequested = true;
        _queuedRenderRequest.reset();
    }
    _renderCv.notify_all();

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

        const int boundedWaitMs = std::max(0, timeoutMs);
        bool stopped = waitForThread(boundedWaitMs);
        if (!stopped && forceKillOnTimeout) {
            _backend.forceKill();
            stopped = waitForThread(std::max(250, boundedWaitMs));
            if (!stopped && renderThreadHandle) {
                TerminateThread(renderThreadHandle, 1);
                stopped = waitForThread(1000);
            }
        } else if (!stopped) {
            while (!stopped) {
                stopped = waitForThread(250);
            }
        }
#else
        (void)forceKillOnTimeout;
        (void)timeoutMs;
#endif
        _renderThread.join();
    }

    {
        const std::scoped_lock lock(_renderMutex);
        _renderStopRequested = false;
        _queuedRenderRequest.reset();
    }

    _renderInFlight = false;
    _progressActive = false;
    _progressCancelled = false;
    _progressValue = 0;
    _progressText.clear();
    _pixelsPerSecondText = "0 pixels/s";
    _lastRenderDurationMs.store(0, std::memory_order_release);
}

void ControlWindow::requestRender(bool syncControls) {
    if (syncControls) {
        _syncStateFromControls();
    }
    _state.zoom = GUI::Util::clampGUIZoom(_state.zoom);
    _syncStateReadouts();
    if (!_backend || !_renderSession) return;
    if (_previewUsesBackendMemory && !_previewImage.isNull()
        && !_directFrameDetachedForRender) {
        _previewImage = _previewImage.copy();
        _previewUsesBackendMemory = false;
        _directFrameDetachedForRender = true;
        const QWidget* dprSource = _viewport ? static_cast<QWidget*>(_viewport)
                                             : static_cast<QWidget*>(this);
        _previewImage.setDevicePixelRatio(
            GUI::Util::effectiveDevicePixelRatio(dprSource));
    }

    const auto pickAction = _pendingPickAction;
    _pendingPickAction.reset();
    const uint64_t requestId
        = _latestRenderRequestId.fetch_add(1, std::memory_order_acq_rel) + 1;

    {
        const std::scoped_lock lock(_renderMutex);
        _queuedRenderRequest = RenderRequest { .state = _state,
            .pointRealText = _pointRealText,
            .pointImagText = _pointImagText,
            .zoomText = _zoomText,
            .seedRealText = _seedRealText,
            .seedImagText = _seedImagText,
            .pickAction = pickAction,
            .id = requestId };
    }

    const bool wasInFlight = _renderInFlight;
    _renderInFlight = true;
    if (!wasInFlight) {
        _progressActive = true;
        _progressCancelled = false;
        _progressValue = 0;
        _progressText = "Rendering";
        _pixelsPerSecondText = "0 pixels/s";
        _updateStatusLabels();
    }

    _renderCv.notify_one();
}

void ControlWindow::applyHomeView() {
    _markPreviewMotion();
    _state.iterations = 0;
    _state.zoom = -0.65;
    _state.point = QPointF(0.0, 0.0);
    _state.seed = QPointF(0.0, 0.0);
    _state.light = QPointF(1.0, 1.0);
    _syncZoomTextFromState();
    _syncPointTextFromState();
    _syncSeedTextFromState();
    _syncControlsFromState();
    requestRender(true);
}

void ControlWindow::scaleAtPixel(
    const QPoint& pixel, double scaleMultiplier) {
    if (!(scaleMultiplier > 0.0) || !std::isfinite(scaleMultiplier)) return;

    ViewTextState nextView;
    QString error;
    if (!_backendZoomViewAtPixel(
            _clampPixelToOutput(pixel), scaleMultiplier, nextView, error)) {
        _setStatusMessage(error);
        _updateStatusLabels();
        return;
    }
    if (NumberUtil::equalParsedDoubleText(
            _zoomText.toStdString(), nextView.zoomText.toStdString())
        && _pointRealText == nextView.pointReal
        && _pointImagText == nextView.pointImag) {
        return;
    }

    _pointRealText = nextView.pointReal;
    _pointImagText = nextView.pointImag;
    _zoomText = nextView.zoomText;
    _syncStatePointFromText();
    _syncStateZoomFromText();
    _markPreviewMotion();
    if (_pendingPickAction
        && _pendingPickAction->target == SelectionTarget::zoomPoint) {
        _pendingPickAction.reset();
    }
    _syncStateReadouts();
    requestRender(false);
    _requestViewportRepaint();
}

void ControlWindow::zoomAtPixel(
    const QPoint& pixel, bool zoomIn) {
    const QPoint clampedPixel = _clampPixelToOutput(pixel);
    const double factor = _currentZoomFactor(_state.zoomRate);
    const double scaleMultiplier = zoomIn ? (1.0 / factor) : factor;
    scaleAtPixel(clampedPixel, scaleMultiplier);
}

void ControlWindow::boxZoom(const QRect& selectionRect) {
    const QRect rect = selectionRect.normalized();
    if (rect.width() < 2 || rect.height() < 2) {
        zoomAtPixel(rect.center(), true);
        return;
    }

    ViewTextState nextView;
    QString error;
    if (!_backendBoxZoomView(rect, nextView, error)) {
        _setStatusMessage(error);
        _updateStatusLabels();
        return;
    }

    _pointRealText = nextView.pointReal;
    _pointImagText = nextView.pointImag;
    _zoomText = nextView.zoomText;
    _syncStatePointFromText();
    _syncStateZoomFromText();
    _markPreviewMotion();
    _syncStateReadouts();
    requestRender(false);
    _requestViewportRepaint();
}

void ControlWindow::panByPixels(const QPoint& delta) {
    if (delta.isNull()) return;

    QString pointReal;
    QString pointImag;
    QString error;
    if (!_backendPanPointByDelta(delta, pointReal, pointImag, error)) {
        _setStatusMessage(error);
        _updateStatusLabels();
        return;
    }

    _pointRealText = pointReal;
    _pointImagText = pointImag;
    _syncStatePointFromText();
    _markPreviewMotion();
    _syncStateReadouts();
    requestRender(false);
    _requestViewportRepaint();
}

void ControlWindow::pickAtPixel(const QPoint& pixel) {
    const QPoint clampedPixel = _clampPixelToOutput(pixel);
    switch (_selectionTarget) {
    case SelectionTarget::zoomPoint: {
        QString real;
        QString imag;
        QString error;
        if (!_backendPointAtPixel(clampedPixel, real, imag, error)) {
            _setStatusMessage(error);
            _updateStatusLabels();
            return;
        }
        _pointRealText = real;
        _pointImagText = imag;
        _syncStatePointFromText();
        break;
    }
    case SelectionTarget::seedPoint: {
        QString real;
        QString imag;
        QString error;
        if (!_backendPointAtPixel(clampedPixel, real, imag, error)) {
            _setStatusMessage(error);
            _updateStatusLabels();
            return;
        }
        _seedRealText = real;
        _seedImagText = imag;
        _syncStateSeedFromText();
        break;
    }
    case SelectionTarget::lightPoint: {
        QString real;
        QString imag;
        QString error;
        if (!_backendPointAtPixel(clampedPixel, real, imag, error)) {
            _setStatusMessage(error);
            _updateStatusLabels();
            return;
        }

        bool okReal = false, okImag = false;
        const double lightReal = real.toDouble(&okReal);
        const double lightImag = imag.toDouble(&okImag);
        if (okReal) _state.light.setX(lightReal);
        if (okImag) _state.light.setY(lightImag);
        break;
    }
    }

    if (_selectionTarget == SelectionTarget::zoomPoint) {
        _pendingPickAction.reset();
    } else {
        _pendingPickAction
            = PendingPickAction { _selectionTarget, clampedPixel };
    }
    _syncStateReadouts();
    requestRender(false);
}

void ControlWindow::updateMouseCoords(const QPoint& pixel) {
    _lastMousePixel = _clampPixelToOutput(pixel);
    QString real;
    QString imag;
    QString error;
    if (!_backendPointAtPixel(_lastMousePixel, real, imag, error)) {
        _mouseText = QString("Mouse: %1, %2  |  -")
                         .arg(_lastMousePixel.x())
                         .arg(_lastMousePixel.y());
        _requestViewportRepaint();
        return;
    }

    _mouseText = QString("Mouse: %1, %2  |  %3  %4")
                     .arg(_lastMousePixel.x())
                     .arg(_lastMousePixel.y())
                     .arg(real)
                     .arg(imag);
    _requestViewportRepaint();
}

void ControlWindow::clearMouseCoords() {
    _mouseText = "Mouse: -";
    _requestViewportRepaint();
}

void ControlWindow::onViewportClosed() {
    requestApplicationShutdown(false);
}

void ControlWindow::adjustIterationsBy(int delta) {
    if (!_iterationsSpin) return;
    _iterationsSpin->setValue(std::max(0, _iterationsSpin->value() + delta));
}

void ControlWindow::cycleNavMode() {
    if (!_navModeCombo) return;
    _navModeCombo->setCurrentIndex((_navModeCombo->currentIndex() + 1) % 3);
}

void ControlWindow::_cycleViewportGrid() {
    if (!_viewport) return;
    _viewport->cycleGridMode();
}

void ControlWindow::_toggleViewportMinimalUI() {
    if (!_viewport) return;
    _viewport->toggleMinimalUI();
}

void ControlWindow::toggleViewportFullscreen() {
    if (_viewport) _viewport->toggleFullscreen();
}

void ControlWindow::prepareViewportFullscreenTransition() {
    _freezeViewportPreview(false, true);
}

void ControlWindow::applyViewportOutputSize(const QSize& outputSize) {
    if (!outputSize.isValid()) return;

    _markPreviewMotion();
    _state.outputWidth = outputSize.width();
    _state.outputHeight = outputSize.height();
    _syncControlsFromState();
    if (_viewport && !_viewport->isFullScreen()) {
        _resizeViewportWindowToImageSize();
    }
    requestRender(false);
}

NavMode ControlWindow::displayedNavMode() const {
    return _displayedNavModeOverride.value_or(_navMode);
}

void ControlWindow::setDisplayedNavModeOverride(std::optional<NavMode> mode) {
    if (_displayedNavModeOverride == mode) return;

    _displayedNavModeOverride = mode;
    _syncControlsFromState();
}

void ControlWindow::cancelQueuedRenders() {
    const int cancelledProgressValue = _progressValue;
    _pendingPickAction.reset();
    _freezeViewportPreview(true, true);

    const uint64_t cancelledRequestBoundary
        = _latestRenderRequestId.fetch_add(1, std::memory_order_acq_rel) + 1;
    _lastPresentedRenderId
        = std::max(_lastPresentedRenderId, cancelledRequestBoundary);
    _callbackRenderRequestId.store(0, std::memory_order_release);
    {
        const std::scoped_lock lock(_renderMutex);
        _queuedRenderRequest.reset();
    }
    _renderCv.notify_all();
    if (_backend) {
        _backend.forceKill();
    }
    _interactionPreviewFallbackLatched.store(false, std::memory_order_release);
    _lastRenderFailureMessage.clear();
    _renderInFlight = false;
    _progressValue = std::clamp(cancelledProgressValue, 0, 100);
    _progressActive = false;
    _progressCancelled = true;
    _progressText.clear();
    _pixelsPerSecondText = "0 pixels/s";
    _setStatusMessage("Render cancelled");
    _updateStatusLabels();
    if (!_closing) {
        _setViewportInteractive(true);
        _requestViewportRepaint();
    }
}

QSize ControlWindow::outputSize() const {
    return { _state.outputWidth, _state.outputHeight };
}

ViewTextState ControlWindow::currentViewTextState() const {
    const QSize size = outputSize();
    return { .pointReal = _pointRealText,
        .pointImag = _pointImagText,
        .zoomText = _zoomText,
        .outputSize = size,
        .valid = size.width() > 0 && size.height() > 0 };
}

ViewTextState ControlWindow::displayedViewTextState() const {
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

bool ControlWindow::previewPannedViewState(
    const QPoint& delta, ViewTextState& view, QString& errorMessage) {
    view = currentViewTextState();
    if (!view.valid || delta.isNull()) {
        return view.valid;
    }

    QString real;
    QString imag;
    if (!_backendPanPointByDelta(delta, real, imag, errorMessage)) {
        view = {};
        return false;
    }

    view.pointReal = real;
    view.pointImag = imag;
    return true;
}

bool ControlWindow::previewScaledViewState(const QPoint& pixel,
    double scaleMultiplier, ViewTextState& view, QString& errorMessage) {
    if (NumberUtil::almostEqual(scaleMultiplier, 1.0)) {
        view = currentViewTextState();
        return view.valid;
    }

    return _backendZoomViewAtPixel(
        _clampPixelToOutput(pixel), scaleMultiplier, view, errorMessage);
}

bool ControlWindow::previewBoxZoomViewState(
    const QRect& selectionRect, ViewTextState& view, QString& errorMessage) {
    const QRect rect = selectionRect.normalized();
    if (rect.width() < 2 || rect.height() < 2) {
        view = currentViewTextState();
        return view.valid;
    }

    return _backendBoxZoomView(rect, view, errorMessage);
}

bool ControlWindow::mapViewPixelToViewPixel(const ViewTextState& sourceView,
    const ViewTextState& targetView, const QPoint& pixel, QPointF& mappedPixel,
    QString& errorMessage) {
    if (!_ensureNavigationSessionReady(errorMessage)) {
        return false;
    }

    Backend::ViewportState source { .outputWidth
        = sourceView.outputSize.width(),
        .outputHeight = sourceView.outputSize.height(),
        .pointReal = sourceView.pointReal.toStdString(),
        .pointImag = sourceView.pointImag.toStdString(),
        .zoom = sourceView.zoomText.toStdString() };
    Backend::ViewportState target { .outputWidth
        = targetView.outputSize.width(),
        .outputHeight = targetView.outputSize.height(),
        .pointReal = targetView.pointReal.toStdString(),
        .pointImag = targetView.pointImag.toStdString(),
        .zoom = targetView.zoomText.toStdString() };

    double mappedX = 0.0;
    double mappedY = 0.0;
    const Backend::Status status = _navigationSession->mapViewPixelToViewPixel(
        source, target, pixel.x(), pixel.y(), mappedX, mappedY);
    if (!status) {
        errorMessage = QString::fromStdString(status.message);
        return false;
    }

    mappedPixel = QPointF(mappedX, mappedY);
    return true;
}

QString ControlWindow::viewportStatusText() const {
    QString text;
    if (!_viewportFPSText.isEmpty()) {
        text = _viewportFPSText;
        if (!_viewportRenderTimeText.isEmpty()) {
            text += QString("  |  %1").arg(_viewportRenderTimeText);
        }
    }
    if (!_mouseText.isEmpty()) {
        text = text.isEmpty() ? _mouseText
                              : QString("%1\n%2").arg(text, _mouseText);
    }
    return text;
}

bool ControlWindow::eventFilter(QObject* watched, QEvent* event) {
    if (_viewport
        && (event->type() == QEvent::KeyPress
            || event->type() == QEvent::KeyRelease)) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        const bool routePress = event->type() == QEvent::KeyPress
            && (matchesShortcut("cancel", keyEvent)
                || matchesShortcut("cycleMode", keyEvent)
                || matchesShortcut("home", keyEvent)
                || matchesShortcut("cycleGrid", keyEvent)
                || matchesShortcut("overlay", keyEvent)
                || matchesShortcut("quickSave", keyEvent)
                || matchesShortcut("fullscreen", keyEvent)
                || keyEvent->key() == Qt::Key_Space
                || keyEvent->key() == Qt::Key_Shift
                || keyEvent->key() == Qt::Key_Control
                || keyEvent->key() == Qt::Key_Left
                || keyEvent->key() == Qt::Key_Right
                || keyEvent->key() == Qt::Key_Up
                || keyEvent->key() == Qt::Key_Down);
        const bool routeRelease = event->type() == QEvent::KeyRelease
            && (keyEvent->key() == Qt::Key_Space
                || keyEvent->key() == Qt::Key_Shift
                || keyEvent->key() == Qt::Key_Control
                || keyEvent->key() == Qt::Key_Left
                || keyEvent->key() == Qt::Key_Right
                || keyEvent->key() == Qt::Key_Up
                || keyEvent->key() == Qt::Key_Down);
        if (routePress || routeRelease) {
            QWidget* targetWidget = qobject_cast<QWidget*>(watched);
            const bool targetIsViewport = targetWidget
                && (targetWidget == _viewport
                    || _viewport->isAncestorOf(targetWidget));
            if (!targetIsViewport) {
                QKeyEvent forwarded(keyEvent->type(), keyEvent->key(),
                    keyEvent->modifiers(), keyEvent->text(),
                    keyEvent->isAutoRepeat(), keyEvent->count());
                QApplication::sendEvent(_viewport, &forwarded);
                if (forwarded.isAccepted()) {
                    return true;
                }
            }
        }
    }

    QLineEdit* iterationsEdit
        = _iterationsSpin ? _iterationsSpin->findChild<QLineEdit*>() : nullptr;
    if (iterationsEdit && watched == iterationsEdit
        && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        const QString text = iterationsEdit->text().trimmed();
        if (text.compare(
                _iterationsSpin->specialValueText(), Qt::CaseInsensitive)
            == 0) {
            const bool printableInput = !keyEvent->text().isEmpty()
                && !keyEvent->text().at(0).isSpace()
                && !(keyEvent->modifiers()
                    & (Qt::ControlModifier | Qt::AltModifier
                        | Qt::MetaModifier));
            if (keyEvent->key() == Qt::Key_Backspace
                || keyEvent->key() == Qt::Key_Delete || printableInput) {
                iterationsEdit->selectAll();
            }
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void ControlWindow::changeEvent(QEvent* event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) {
        if (_ui) _ui->retranslateUi(this);
        _retranslateMenus();
        _retranslateDynamicControls();
        _applyShortcutSettings();
    }
}

void ControlWindow::closeEvent(QCloseEvent* event) {
    event->accept();
    requestApplicationShutdown(true);
}

void ControlWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);

    if (!_controlWindowSized) {
        _updateControlWindowSize();
        _positionWindowsForInitialShow();
        _controlWindowSized = true;
    }
    QTimer::singleShot(0, this, [this]() {
        if (!_viewport || !_viewport->isVisible()) return;
        _viewport->raise();
        _viewport->activateWindow();
        _viewport->setFocus(Qt::ActiveWindowFocusReason);
    });
}

void ControlWindow::requestApplicationShutdown(bool closeViewportWindow) {
    if (_shutdownQueued || _closing) return;
    _shutdownQueued = true;
    _saveGUISettings();

    setEnabled(false);
    hide();
    if (_viewport) {
        _viewport->setEnabled(false);
        _viewport->hide();
    }

    QTimer::singleShot(0, this, [this, closeViewportWindow]() {
        _shutdownForExit(closeViewportWindow);
        QApplication::quit();
    });
}

void ControlWindow::_shutdownForExit(bool closeViewportWindow) {
    if (_closing) return;
    _closing = true;
    _shutdownQueued = false;
    _backendGeneration.fetch_add(1, std::memory_order_acq_rel);
    _callbackRenderRequestId.store(0, std::memory_order_release);

    _freezeViewportPreview(true, true);
    _pendingPickAction.reset();
    if (_backend && _renderSession) {
        _renderSession->setCallbacks({});
    }
    if (_backend) {
        _backend.forceKill();
    }

    if (closeViewportWindow && _viewport) {
        QWidget* viewport = _viewport;
        _viewport = nullptr;
        viewport->hide();
        viewport->deleteLater();
    }

    _finishRenderThread(true, kForceKillDelayMs);
}

void ControlWindow::_freezeViewportPreview(
    bool disableViewport, bool clearInteractionPreview) {
    if (disableViewport) {
        _setViewportInteractive(false);
    }

    _previewImage = GUI::Util::makeBlankViewportImage();
    _previewUsesBackendMemory = false;
    _directFrameDetachedForRender = false;
    _presentingCopiedPreview = false;
    _previewStillTimer.stop();
    const QWidget* dprSource = _viewport ? static_cast<QWidget*>(_viewport)
                                         : static_cast<QWidget*>(this);
    _previewImage.setDevicePixelRatio(
        GUI::Util::effectiveDevicePixelRatio(dprSource));

    if (_viewport && clearInteractionPreview) {
        _viewport->clearPreviewOffset();
    }
    _requestViewportRepaint();
}

void ControlWindow::_setViewportInteractive(bool enabled) {
    if (!_viewport) return;
    _viewport->setEnabled(enabled);
}

void ControlWindow::_updateStatusRightEdgeAlignment() {
    if (!_saveButton || !_statusRightLabel) return;
    constexpr int statusRowLeftShift = 3;

    QWidget* statusPanel = _statusRightLabel->parentWidget();
    if (!statusPanel) return;

    auto* statusRow = qobject_cast<QHBoxLayout*>(statusPanel->layout());
    if (!statusRow) return;

    const QPoint saveTopLeft = _saveButton->mapTo(this, QPoint(0, 0));
    const int targetRight
        = saveTopLeft.x() + _saveButton->width() - statusRowLeftShift;
    const QPoint panelTopLeft = statusPanel->mapTo(this, QPoint(0, 0));
    const int panelRight = panelTopLeft.x() + statusPanel->width();
    const int rightInset = std::max(0, panelRight - targetRight);
    const int leftInset = -statusRowLeftShift;

    const QMargins margins = statusRow->contentsMargins();
    if (margins.left() == leftInset && margins.right() == rightInset) return;

    statusRow->setContentsMargins(
        leftInset, margins.top(), rightInset, margins.bottom());
}

void ControlWindow::_setStatusMessage(const QString& message) {
    _statusText = message;
    _statusLinkPath.clear();
    _updateStatusLabels();
}

void ControlWindow::_setStatusSavedPath(const QString& path) {
    const QString absolutePath = QFileInfo(path).absoluteFilePath();
    _statusText
        = QString("Saved: %1").arg(QDir::toNativeSeparators(absolutePath));
    _statusLinkPath = absolutePath;
    _updateStatusLabels();
}

void ControlWindow::_updateStatusLabels(const QString& rightText) {
    const int shownProgressValue
        = (_progressActive || _progressCancelled) ? _progressValue : 0;
    if (_progressLabel) {
        _progressLabel->setText(QString("%1%").arg(shownProgressValue));
        _progressLabel->setStyleSheet(
            _progressCancelled ? "color: rgb(215, 80, 80);" : QString());
    }
    if (_progressBar) {
        _progressBar->setValue(shownProgressValue);
        _progressBar->setTextVisible(false);
        _progressBar->setStyleSheet(_progressCancelled
                ? "QProgressBar::chunk { background-color: rgb(215, 80, 80); }"
                : QString());
    }
    if (_statusRightLabel) {
        _statusRightLabel->setEmphasisEnabled(_progressCancelled);
    }
    const QString fullPixelsPerSecondText
        = ((_progressActive || _pixelsPerSecondText != "0 pixels/s")
              && !_pixelsPerSecondText.isEmpty())
        ? _pixelsPerSecondText
        : QString();
    QString pixelsPerSecondText = fullPixelsPerSecondText;
    if (_imageMemoryLabel) {
        _imageMemoryLabel->setText(_imageMemoryText);
    }
    if (!_statusRightLabel) {
        if (_pixelsPerSecondLabel)
            _pixelsPerSecondLabel->setText(pixelsPerSecondText);
        return;
    }

    _refreshDirtyComboLabels();

    const QString baseText = rightText.isEmpty() ? _statusText : rightText;
    const QString sourceLink
        = rightText.isEmpty() ? _statusLinkPath : QString();
    const QString sourceText = baseText;
    if (_pixelsPerSecondLabel)
        _pixelsPerSecondLabel->setText(pixelsPerSecondText);

    bool shouldMarquee = _statusRightLabel->wouldMarquee(sourceText);
    if (shouldMarquee && _progressActive && _pixelsPerSecondLabel) {
        const int separatorIndex = fullPixelsPerSecondText.indexOf(' ');
        if (separatorIndex > 0) {
            pixelsPerSecondText = fullPixelsPerSecondText.left(separatorIndex);
            if (_ui && _ui->statusLayout) _ui->statusLayout->activate();
            shouldMarquee = _statusRightLabel->wouldMarquee(sourceText);
        }
    }
    if (_pixelsPerSecondLabel)
        _pixelsPerSecondLabel->setText(pixelsPerSecondText);
    _statusRightLabel->setSource(sourceText, sourceLink);
}

void ControlWindow::_updateLightColorButton() {
    if (!_lightColorButton) return;

    const QColor color = GUI::Util::lightColorToQColor(_state.lightColor);
    _lightColorButton->setText(
        QString::fromStdString(FormatUtil::formatHexColor(
            _state.lightColor.R, _state.lightColor.G, _state.lightColor.B)));
    _lightColorButton->setStyleSheet(GUI::Util::lightColorButtonStyle(color));
}

void ControlWindow::_updateModeEnablement() {
    const bool paletteMode
        = _state.colorMethod == Backend::ColorMethod::palette;
    const bool sineMode = _state.colorMethod == Backend::ColorMethod::iterations
        || _state.colorMethod == Backend::ColorMethod::smooth_iterations;
    const bool lightMode = _state.colorMethod == Backend::ColorMethod::light;
    if (_sineGroup) {
        static_cast<::CollapsibleGroupBox*>(_sineGroup)
            ->setContentEnabled(sineMode);
    }
    if (_paletteGroup) {
        static_cast<::CollapsibleGroupBox*>(_paletteGroup)
            ->setContentEnabled(paletteMode);
    }
    if (_lightGroup) {
        static_cast<::CollapsibleGroupBox*>(_lightGroup)
            ->setContentEnabled(lightMode);
    }
    _updateViewport();
}

void ControlWindow::_updateViewport() {
    if (_viewport) {
        _resizeViewportWindowToImageSize();
        _requestViewportRepaint();
    }

    _updateSinePreview();
    _refreshPalettePreview();
    _updateStatusLabels();
}

void ControlWindow::_requestViewportRepaint() {
    if (!_viewport) return;
    _viewport->update();
}

void ControlWindow::_updateSinePreview() {
    if (!_sinePreview) return;

    auto* preview = static_cast<::SinePreviewWidget*>(_sinePreview);
    const QSize widgetSize = preview->size();
    if (!widgetSize.isValid()) return;

    if (!_backend || !_previewSession) {
        preview->setPreviewImage({});
        return;
    }

    const auto [rangeMin, rangeMax] = preview->range();
    const Backend::Status colorStatus
        = _previewSession->setSinePalette(_state.sinePalette);
    if (!colorStatus) {
        preview->setPreviewImage({});
        return;
    }

    const int previewWidth = std::max(1, widgetSize.width() - 12);
    const int previewHeight = std::max(1, widgetSize.height() - 28);
    const QImage image = GUI::Util::imageViewToImage(
        _previewSession->renderSinePreview(previewWidth, previewHeight,
            static_cast<float>(rangeMin), static_cast<float>(rangeMax)));
    preview->setPreviewImage(image);
}

void ControlWindow::_refreshPalettePreview() {
    if (!_palettePreviewLabel) return;

    const QSize targetSize = _palettePreviewLabel->size();
    if (!targetSize.isValid()) return;
    auto* preview = static_cast<::PalettePreviewWidget*>(_palettePreviewLabel);

    if (!_palettePreviewDirty && !_palettePreviewPixmap.isNull()
        && _palettePreviewSize == targetSize) {
        preview->setPreviewPixmap(_palettePreviewPixmap);
        return;
    }

    const QImage image = GUI::PaletteStore::makePreviewImage(
        _previewSession, _state.palette, 256, 20);
    if (image.isNull()) {
        preview->clearPreview();
        return;
    }

    _palettePreviewPixmap = QPixmap::fromImage(image).scaled(
        targetSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    _palettePreviewSize = targetSize;
    _palettePreviewDirty = false;
    preview->setPreviewPixmap(_palettePreviewPixmap);
}

void ControlWindow::_updateControlWindowSize() {
    if (layout()) layout()->activate();
    if (_controlScrollContent && _controlScrollContent->layout()) {
        _controlScrollContent->layout()->activate();
        _controlScrollContent->updateGeometry();
    }

    QSize target = sizeHint().expandedTo(minimumSizeHint());
    const QSize packedTarget = minimumSizeHint().expandedTo(QSize(1, 1));
    const QMargins outerMargins
        = layout() ? layout()->contentsMargins() : QMargins();
    const int scrollBarExtent = controlScrollBarExtent(_controlScrollArea);
    int controlContentHeight = 0, defaultVisibleContentHeight = 0;
    int packedWidth = packedTarget.width();
    if (_controlScrollContent) {
        const QSize contentSize = _controlScrollContent->sizeHint().expandedTo(
            _controlScrollContent->minimumSizeHint());
        const QSize contentMinSize
            = _controlScrollContent->minimumSizeHint().expandedTo(QSize(1, 1));
        const int contentWidth = contentSize.width();
        const int contentMinWidth = contentMinSize.width();
        controlContentHeight = contentSize.height();
        if (_viewportGroup) {
            const QMargins contentMargins
                = _controlScrollContent->contentsMargins();
            defaultVisibleContentHeight = std::max(0,
                _viewportGroup->geometry().bottom() + 1
                    + contentMargins.bottom());
        }
        target.setWidth(std::max(target.width(),
            contentWidth + outerMargins.left() + outerMargins.right()
                + scrollBarExtent));
        packedWidth = std::max(packedWidth,
            contentMinWidth + outerMargins.left() + outerMargins.right()
                + scrollBarExtent);
    } else {
        target += QSize(scrollBarExtent, 0);
        packedWidth = std::max(packedWidth, target.width());
    }

    int fixedPanelsMinWidth = 0;
    if (QLayout* outerLayout = layout()) {
        for (int i = 0; i < outerLayout->count(); ++i) {
            QLayoutItem* item = outerLayout->itemAt(i);
            if (!item) continue;
            QWidget* widget = item->widget();
            if (!widget || widget == _controlScrollArea) continue;

            fixedPanelsMinWidth
                = std::max(fixedPanelsMinWidth, item->minimumSize().width());
            fixedPanelsMinWidth = std::max(
                fixedPanelsMinWidth, widget->minimumSizeHint().width());
        }
    }
    packedWidth = std::max(packedWidth,
        fixedPanelsMinWidth + outerMargins.left() + outerMargins.right());

    const int minHeight = std::max(1, minimumSizeHint().height());

    const int effectiveMinWidth = _controlWindowSized
        ? std::min(std::max(1, width()), std::max(1, packedWidth))
        : std::max(1, packedWidth);
    setMinimumWidth(effectiveMinWidth);
    setMaximumWidth(QWIDGETSIZE_MAX);
    setMinimumHeight(minHeight);
    setMaximumHeight(QWIDGETSIZE_MAX);

    if (!_controlWindowSized) {
        int fixedPanelsHeight = 0;
        if (QLayout* outerLayout = layout()) {
            const QMargins margins = outerLayout->contentsMargins();
            fixedPanelsHeight += margins.top() + margins.bottom();

            int visibleWidgets = 0;
            for (int i = 0; i < outerLayout->count(); ++i) {
                QLayoutItem* item = outerLayout->itemAt(i);
                if (!item) continue;

                QWidget* widget = item->widget();
                if (!widget) continue;

                ++visibleWidgets;
                if (widget == _controlScrollArea) continue;
                fixedPanelsHeight += item->sizeHint().height();
            }

            fixedPanelsHeight
                += std::max(0, visibleWidgets - 1) * outerLayout->spacing();
        }

        const int desiredContentHeight = defaultVisibleContentHeight > 0
            ? defaultVisibleContentHeight
            : controlContentHeight;
        int desiredHeight
            = std::max(minHeight, fixedPanelsHeight + desiredContentHeight);
        if (const QScreen* screen = (windowHandle() && windowHandle()->screen())
                ? windowHandle()->screen()
                : (this->screen() ? this->screen()
                                  : QApplication::primaryScreen())) {
            const int maxOnScreenHeight = std::max(minHeight,
                screen->availableGeometry().height()
                    - kControlWindowScreenPadding);
            desiredHeight = std::min(desiredHeight, maxOnScreenHeight);
        }

        resize(std::max(1, packedWidth), desiredHeight);
    }

    if (layout()) layout()->activate();
    _updateStatusRightEdgeAlignment();
}

void ControlWindow::_positionWindowsForInitialShow() {
    if (!_viewport) return;

    const QScreen* screen
        = (_viewport->windowHandle() && _viewport->windowHandle()->screen())
        ? _viewport->windowHandle()->screen()
        : ((windowHandle() && windowHandle()->screen())
                  ? windowHandle()->screen()
                  : (this->screen() ? this->screen()
                                    : QApplication::primaryScreen()));
    if (!screen) return;

    const QRect available = screen->availableGeometry();
    const QSize controlFrameSize = frameGeometry().size();
    const QSize viewportFrameSize = _viewport->frameGeometry().size();
    constexpr int gap = 8;

    const int controlY = std::clamp(
        available.top() + (available.height() - controlFrameSize.height()) / 2,
        available.top(),
        std::max(available.top(),
            available.bottom() - controlFrameSize.height() + 1));
    const int viewportY = std::clamp(
        available.top() + (available.height() - viewportFrameSize.height()) / 2,
        available.top(),
        std::max(available.top(),
            available.bottom() - viewportFrameSize.height() + 1));

    int controlX = available.left();
    int viewportX = controlX + controlFrameSize.width() + gap;
    const int totalWidth
        = controlFrameSize.width() + gap + viewportFrameSize.width();
    if (totalWidth <= available.width()) {
        controlX = available.left() + (available.width() - totalWidth) / 2;
        viewportX = controlX + controlFrameSize.width() + gap;
    } else {
        viewportX = std::min(controlX + controlFrameSize.width() + gap,
            available.right() - viewportFrameSize.width() + 1);
        viewportX = std::max(controlX, viewportX);
    }

    move(controlX, controlY);
    _viewport->move(viewportX, viewportY);
}

void ControlWindow::_applyShortcutSettings() {
    const auto setShortcut = [this](QAction* action, const QString& id) {
        if (action) action->setShortcut(_shortcuts.sequence(id));
    };

    setShortcut(_cancelRenderAction, "cancel");
    setShortcut(_homeAction, "home");
    setShortcut(_cycleModeAction, "cycleMode");
    setShortcut(_cycleGridAction, "cycleGrid");
    setShortcut(_minimalUiAction, "overlay");
    setShortcut(_quickSaveAction, "quickSave");
    setShortcut(_fullscreenAction, "fullscreen");
    setShortcut(_calculateAction, "calculate");
    setShortcut(_saveImageAction, "saveImage");
    setShortcut(_openViewAction, "openView");
    setShortcut(_saveViewAction, "saveView");
    setShortcut(_settingsAction, "settings");
    setShortcut(_exitAction, "exit");
}

void ControlWindow::_retranslateMenus() {
    if (_fileMenu) _fileMenu->setTitle(tr("File"));
    if (_viewMenu) _viewMenu->setTitle(tr("View"));
    if (_renderMenu) _renderMenu->setTitle(tr("Render"));
    if (_helpMenu) _helpMenu->setTitle(tr("Help"));

    if (_homeAction) _homeAction->setText(tr("New / Home"));
    if (_openViewAction) _openViewAction->setText(tr("Open View"));
    if (_saveViewAction) _saveViewAction->setText(tr("Save View"));
    if (_saveImageAction) _saveImageAction->setText(tr("Save Image"));
    if (_quickSaveAction) _quickSaveAction->setText(tr("Quick Save"));
    if (_settingsAction) _settingsAction->setText(tr("Settings"));
    if (_exitAction) _exitAction->setText(tr("Exit"));
    if (_cycleModeAction)
        _cycleModeAction->setText(tr("Cycle Navigation Mode"));
    if (_cycleGridAction) _cycleGridAction->setText(tr("Cycle Grid"));
    if (_minimalUiAction)
        _minimalUiAction->setText(tr("Toggle Viewport Overlay"));
    if (_fullscreenAction) _fullscreenAction->setText(tr("Toggle Fullscreen"));
    if (_calculateAction) _calculateAction->setText(tr("Calculate"));
    if (_cancelRenderAction) _cancelRenderAction->setText(tr("Cancel Render"));
    if (_helpAction) _helpAction->setText(tr("Help"));
    if (_aboutAction) _aboutAction->setText(tr("About"));
}

void ControlWindow::_retranslateDynamicControls() {
    if (_colorMethodCombo && _colorMethodCombo->count() >= 4) {
        _colorMethodCombo->setItemText(0, tr("Iterations"));
        _colorMethodCombo->setItemText(1, tr("Smooth Iterations"));
        _colorMethodCombo->setItemText(2, tr("Palette"));
        _colorMethodCombo->setItemText(3, tr("Light"));
    }
    if (_fractalCombo && _fractalCombo->count() >= 3) {
        _fractalCombo->setItemText(0, tr("Mandelbrot"));
        _fractalCombo->setItemText(1, tr("Perpendicular"));
        _fractalCombo->setItemText(2, tr("Burning Ship"));
    }
    if (_navModeCombo && _navModeCombo->count() >= 3) {
        _navModeCombo->setItemText(0, tr("Realtime Zoom"));
        _navModeCombo->setItemText(1, tr("Box Zoom"));
        _navModeCombo->setItemText(2, tr("Pan"));
    }
    if (_pickTargetCombo && _pickTargetCombo->count() >= 3) {
        _pickTargetCombo->setItemText(0, tr("Zoom Point"));
        _pickTargetCombo->setItemText(1, tr("Seed Point"));
        _pickTargetCombo->setItemText(2, tr("Light Point"));
    }
}

void ControlWindow::_loadGUISettings() {
    const QByteArray geometry = _settings.controlWindowGeometry();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }
    const QByteArray state = _settings.controlWindowState();
    if (!state.isEmpty()) {
        restoreState(state);
    }
    if (!geometry.isEmpty() || !state.isEmpty()) {
        _controlWindowSized = true;
    }
}

void ControlWindow::_saveGUISettings() {
    _settings.setControlWindowGeometry(saveGeometry());
    _settings.setControlWindowState(saveState());
    if (!_state.backend.isEmpty()) {
        _settings.setPreferredBackend(_state.backend);
    }
    _settings.setPreviewFallbackFPS(_state.interactionTargetFPS);
    _settings.setSelectedOutputWidth(_state.outputWidth);
    _settings.setSelectedOutputHeight(_state.outputHeight);
    _settings.sync();
}

void ControlWindow::_showSettingsDialog() {
    SettingsDialog dialog(
        _settings.language(), _state.interactionTargetFPS, _shortcuts, this);
    if (dialog.exec() != QDialog::Accepted) return;

    Shortcuts edited = dialog.shortcuts(_settings);
    const QString duplicateError = edited.duplicateError();
    if (!duplicateError.isEmpty()) {
        QMessageBox::warning(this, tr("Settings"), duplicateError);
        return;
    }

    _settings.setLanguage(dialog.language());
    _state.interactionTargetFPS = dialog.previewFallbackFPS();
    _settings.setPreviewFallbackFPS(_state.interactionTargetFPS);
    if (!_locale.setLanguage(dialog.language())) {
        QMessageBox::warning(this, tr("Settings"),
            tr("The selected language could not be loaded."));
    }
    edited.save();
    _shortcuts.reload();
    _applyShortcutSettings();
    if (_ui) _ui->retranslateUi(this);
    _retranslateMenus();
}

void ControlWindow::_showHelpDialog() {
    HelpDialog dialog(this);
    dialog.exec();
}

void ControlWindow::_showAboutDialog() {
    AboutDialog dialog(windowIcon(), this);
    dialog.exec();
}
void ControlWindow::_handleRenderFailure(const QString& message) {
    if (message.isEmpty()) return;

    _setStatusMessage(message);
    if (_lastRenderFailureMessage == message) {
        return;
    }

    _lastRenderFailureMessage = message;
    QMessageBox::warning(this, "Render", message);
}

void ControlWindow::_syncStateFromControls() {
    _state.backend = _backendCombo->currentText();
    _state.iterations = _iterationsSpin->value();
    _state.useThreads = _useThreadsCheckBox->isChecked();
    _state.julia = _juliaCheck->isChecked();
    _state.inverse = _inverseCheck->isChecked();
    _state.aaPixels = _aaSpin->value();
    _state.preserveRatio = _preserveRatioCheck->isChecked();
    _state.panRate = _panRateSlider->value();
    _state.zoomRate = _zoomRateSlider->value();
    _state.exponent = std::max(1.01, _exponentSpin->value());
    if (_sineCombo) {
        QString sineName = _sineCombo->currentData().toString();
        if (sineName.isEmpty()) {
            sineName = GUI::Util::undecoratedLabel(_sineCombo->currentText());
        }
        if (!sineName.isEmpty() && sineName != kNewEntryLabel) {
            _state.sineName = sineName;
        }
    }
    _state.sinePalette.freqR = static_cast<float>(_freqRSpin->value());
    _state.sinePalette.freqG = static_cast<float>(_freqGSpin->value());
    _state.sinePalette.freqB = static_cast<float>(_freqBSpin->value());
    _state.sinePalette.freqMult = static_cast<float>(_freqMultSpin->value());
    if (_paletteCombo) {
        QString paletteName = _paletteCombo->currentData().toString();
        if (paletteName.isEmpty()) {
            paletteName
                = GUI::Util::undecoratedLabel(_paletteCombo->currentText());
        }
        if (!paletteName.isEmpty() && paletteName != kNewEntryLabel) {
            _state.paletteName = paletteName;
        }
    }
    _state.palette.totalLength
        = static_cast<float>(_paletteLengthSpin->value());
    _state.palette.offset = static_cast<float>(_paletteOffsetSpin->value());
    _state.outputWidth = _outputWidthSpin->value();
    _state.outputHeight = _outputHeightSpin->value();

    if (_infoRealEdit) {
        const QString text = _infoRealEdit->text().trimmed();
        if (!text.isEmpty()) _pointRealText = text;
    }
    if (_infoImagEdit) {
        const QString text = _infoImagEdit->text().trimmed();
        if (!text.isEmpty()) _pointImagText = text;
    }
    _syncStatePointFromText();

    if (_infoZoomEdit) {
        const QString text = _infoZoomEdit->text().trimmed();
        if (!text.isEmpty()) _zoomText = text;
    }
    _syncStateZoomFromText();

    if (_infoSeedRealEdit) {
        const QString text = _infoSeedRealEdit->text().trimmed();
        if (!text.isEmpty()) _seedRealText = text;
    }
    if (_infoSeedImagEdit) {
        const QString text = _infoSeedImagEdit->text().trimmed();
        if (!text.isEmpty()) _seedImagText = text;
    }
    _syncStateSeedFromText();

    switch (_colorMethodCombo->currentIndex()) {
    case 0:
        _state.colorMethod = Backend::ColorMethod::iterations;
        break;
    case 1:
        _state.colorMethod = Backend::ColorMethod::smooth_iterations;
        break;
    case 2:
        _state.colorMethod = Backend::ColorMethod::palette;
        break;
    case 3:
        _state.colorMethod = Backend::ColorMethod::light;
        break;
    }

    switch (_fractalCombo->currentIndex()) {
    case 0:
        _state.fractalType = Backend::FractalType::mandelbrot;
        break;
    case 1:
        _state.fractalType = Backend::FractalType::perpendicular;
        break;
    case 2:
        _state.fractalType = Backend::FractalType::burningship;
        break;
    }
}

void ControlWindow::_syncControlsFromState() {
    const QSignalBlocker backendBlocker(_backendCombo);
    const QSignalBlocker iterationsBlocker(_iterationsSpin);
    const QSignalBlocker threadsBlocker(_useThreadsCheckBox);
    const QSignalBlocker juliaBlocker(_juliaCheck);
    const QSignalBlocker inverseBlocker(_inverseCheck);
    const QSignalBlocker aaBlocker(_aaSpin);
    const QSignalBlocker preserveRatioBlocker(_preserveRatioCheck);
    const QSignalBlocker panSliderBlocker(_panRateSlider);
    const QSignalBlocker panSpinBlocker(_panRateSpin);
    const QSignalBlocker zoomSliderBlocker(_zoomRateSlider);
    const QSignalBlocker zoomSpinBlocker(_zoomRateSpin);
    const QSignalBlocker exponentSliderBlocker(_exponentSlider);
    const QSignalBlocker exponentSpinBlocker(_exponentSpin);
    const QSignalBlocker sineComboBlocker(_sineCombo);
    const QSignalBlocker freqRBlocker(_freqRSpin);
    const QSignalBlocker freqGBlocker(_freqGSpin);
    const QSignalBlocker freqBBlocker(_freqBSpin);
    const QSignalBlocker freqMultBlocker(_freqMultSpin);
    const QSignalBlocker paletteComboBlocker(_paletteCombo);
    const QSignalBlocker paletteLengthBlocker(_paletteLengthSpin);
    const QSignalBlocker paletteOffsetBlocker(_paletteOffsetSpin);
    const QSignalBlocker widthBlocker(_outputWidthSpin);
    const QSignalBlocker heightBlocker(_outputHeightSpin);
    const QSignalBlocker colorBlocker(_colorMethodCombo);
    const QSignalBlocker fractalBlocker(_fractalCombo);
    const QSignalBlocker navModeBlocker(_navModeCombo);

    _backendCombo->setCurrentText(_state.backend);
    _iterationsSpin->setValue(_state.iterations);
    _useThreadsCheckBox->setChecked(_state.useThreads);
    _juliaCheck->setChecked(_state.julia);
    _inverseCheck->setChecked(_state.inverse);
    _aaSpin->setValue(_state.aaPixels);
    _preserveRatioCheck->setChecked(_state.preserveRatio);
    _panRateSlider->setValue(_state.panRate);
    _panRateSpin->setValue(_state.panRate);
    _zoomRateSlider->setValue(_state.zoomRate);
    _zoomRateSpin->setValue(_state.zoomRate);
    _exponentSlider->setValue(
        static_cast<int>(std::round(std::max(1.01, _state.exponent) * 100.0)));
    GUI::Util::setAdaptiveSpinValue(
        _exponentSpin, std::max(1.01, _state.exponent));
    if (_sineCombo && !_state.sineName.isEmpty()) {
        const int sineIndex = _sineCombo->findData(_state.sineName);
        if (sineIndex >= 0) {
            _sineCombo->setCurrentIndex(sineIndex);
        } else {
            _sineCombo->setCurrentText(_state.sineName);
        }
    }
    GUI::Util::setAdaptiveSpinValue(_freqRSpin, _state.sinePalette.freqR);
    GUI::Util::setAdaptiveSpinValue(_freqGSpin, _state.sinePalette.freqG);
    GUI::Util::setAdaptiveSpinValue(_freqBSpin, _state.sinePalette.freqB);
    GUI::Util::setAdaptiveSpinValue(_freqMultSpin, _state.sinePalette.freqMult);
    if (_paletteCombo && !_state.paletteName.isEmpty()) {
        const int paletteIndex = _paletteCombo->findData(_state.paletteName);
        if (paletteIndex >= 0) {
            _paletteCombo->setCurrentIndex(paletteIndex);
        } else {
            _paletteCombo->setCurrentText(_state.paletteName);
        }
    }
    GUI::Util::setAdaptiveSpinValue(
        _paletteLengthSpin, _state.palette.totalLength);
    GUI::Util::setAdaptiveSpinValue(_paletteOffsetSpin, _state.palette.offset);
    _outputWidthSpin->setValue(_state.outputWidth);
    _outputHeightSpin->setValue(_state.outputHeight);
    _colorMethodCombo->setCurrentIndex(static_cast<int>(_state.colorMethod));
    _fractalCombo->setCurrentIndex(static_cast<int>(_state.fractalType));
    _navModeCombo->setCurrentIndex(static_cast<int>(displayedNavMode()));

    _updateLightColorButton();
    _syncStateReadouts();
    _refreshDirtyComboLabels();
    _updateSinePreview();
}

void ControlWindow::_syncStateReadouts() {
    GUI::Util::setCommittedLineEditText(_infoRealEdit, _pointRealText);
    GUI::Util::setCommittedLineEditText(_infoImagEdit, _pointImagText);
    GUI::Util::setCommittedLineEditText(_infoZoomEdit, _zoomText);
    GUI::Util::setCommittedLineEditText(_infoSeedRealEdit, _seedRealText);
    GUI::Util::setCommittedLineEditText(_infoSeedImagEdit, _seedImagText);
    if (_lightRealEdit)
        _lightRealEdit->setText(_stateToString(_state.light.x(), 12));
    if (_lightImagEdit)
        _lightImagEdit->setText(_stateToString(_state.light.y(), 12));
}

void ControlWindow::_refreshSineList(const QString& preferredName) {
    if (!_sineCombo) return;

    QStringList names = GUI::SineStore::listNames();
    const QString currentName
        = preferredName.isEmpty() ? _state.sineName : preferredName;
    if (!currentName.isEmpty() && currentName != kNewEntryLabel
        && !names.contains(currentName, Qt::CaseInsensitive)) {
        names.push_front(currentName);
    }
    const QString selectedName
        = names.contains(currentName, Qt::CaseInsensitive)
        ? currentName
        : (names.isEmpty() ? QString() : names.front());

    const QSignalBlocker blocker(_sineCombo);
    _sineCombo->clear();
    _sineCombo->addItem(kNewEntryLabel);
    _sineCombo->setItemData(0, QBrush(QColor(0, 153, 0)), Qt::ForegroundRole);
    _sineCombo->insertSeparator(1);
    const bool markUnsaved = _isSineDirty();
    for (const QString& name : names) {
        const bool unsavedItem = markUnsaved
            && name.compare(currentName, Qt::CaseInsensitive) == 0;
        _sineCombo->addItem(
            GUI::Util::decorateUnsavedLabel(name, unsavedItem), name);
    }
    if (!selectedName.isEmpty()) {
        const int selectedIndex = _sineCombo->findData(selectedName);
        if (selectedIndex >= 0) {
            _sineCombo->setCurrentIndex(selectedIndex);
        } else {
            _sineCombo->setCurrentText(selectedName);
        }
    } else {
        _sineCombo->setCurrentIndex(0);
    }
}

void ControlWindow::_refreshPaletteList(const QString& preferredName) {
    if (!_paletteCombo) return;

    QStringList names = GUI::PaletteStore::listNames();
    const QString currentName
        = preferredName.isEmpty() ? _state.paletteName : preferredName;
    if (!currentName.isEmpty() && currentName != kNewEntryLabel
        && !names.contains(currentName, Qt::CaseInsensitive)) {
        names.push_front(currentName);
    }
    const QString selectedName
        = names.contains(currentName, Qt::CaseInsensitive)
        ? currentName
        : (names.isEmpty() ? QString() : names.front());

    const QSignalBlocker blocker(_paletteCombo);
    _paletteCombo->clear();
    _paletteCombo->addItem(kNewEntryLabel);
    _paletteCombo->setItemData(
        0, QBrush(QColor(0, 153, 0)), Qt::ForegroundRole);
    _paletteCombo->insertSeparator(1);
    const bool markUnsaved = _isPaletteDirty();
    for (const QString& name : names) {
        const bool unsavedItem = markUnsaved
            && name.compare(currentName, Qt::CaseInsensitive) == 0;
        _paletteCombo->addItem(
            GUI::Util::decorateUnsavedLabel(name, unsavedItem), name);
    }
    if (!selectedName.isEmpty()) {
        const int selectedIndex = _paletteCombo->findData(selectedName);
        if (selectedIndex >= 0) {
            _paletteCombo->setCurrentIndex(selectedIndex);
        } else {
            _paletteCombo->setCurrentText(selectedName);
        }
    } else {
        _paletteCombo->setCurrentIndex(0);
    }
}

void ControlWindow::_createNewSinePalette(bool requestRenderOnSuccess) {
    const QStringList existingNames = GUI::SineStore::listNames();
    const QString newName = GUI::Util::uniqueIndexedNameFromList(
        kNewSinePaletteBaseName, existingNames);
    const Backend::SinePaletteConfig config = GUI::SineStore::makeNewConfig();

    _state.sineName = newName;
    _state.sinePalette = config;
    _refreshSineList(_state.sineName);
    _syncControlsFromState();
    _updateViewport();
    if (requestRenderOnSuccess) {
        requestRender();
    }
}

void ControlWindow::_createNewColorPalette(bool requestRenderOnSuccess) {
    const QStringList existingNames = GUI::PaletteStore::listNames();
    const QString newName = GUI::Util::uniqueIndexedNameFromList(
        kNewColorPaletteBaseName, existingNames);
    const Backend::PaletteHexConfig config = GUI::PaletteStore::makeNewConfig();

    _state.paletteName = newName;
    _state.palette = config;
    _palettePreviewDirty = true;
    _refreshPaletteList(_state.paletteName);
    _syncControlsFromState();
    _updateViewport();
    if (requestRenderOnSuccess) {
        requestRender();
    }
}

void ControlWindow::_saveSine() {
    _syncStateFromControls();

    QString targetName = GUI::PaletteStore::normalizeName(_state.sineName);
    if (!GUI::PaletteStore::isValidName(targetName)
        || targetName == kNewEntryLabel) {
        const QString suggested
            = GUI::PaletteStore::sanitizeName(_state.sineName);
        bool accepted = false;
        const QString enteredName = QInputDialog::getText(this,
            "Save Sine Color", "Name", QLineEdit::Normal,
            suggested.isEmpty() ? "sine" : suggested, &accepted);
        if (!accepted) return;

        targetName = GUI::PaletteStore::normalizeName(enteredName);
        if (!GUI::PaletteStore::isValidName(targetName)) {
            QMessageBox::warning(this, "Save Sine Color",
                "Use an ASCII name with letters, numbers, spaces, ., _, or -.");
            return;
        }
    }

    QString errorMessage;
    if (!GUI::SineStore::ensureDirectory(errorMessage)) {
        QMessageBox::warning(this, "Save Sine Color", errorMessage);
        return;
    }

    std::filesystem::path destinationPath
        = GUI::SineStore::filePath(targetName);
    std::error_code existsError;
    const bool destinationExists
        = std::filesystem::exists(destinationPath, existsError) && !existsError;
    if (destinationExists) {
        const QMessageBox::StandardButton choice
            = QMessageBox::question(this, "Save Sine Color",
                QString("Sine \"%1\" already exists. Overwrite it?")
                    .arg(targetName),
                QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                QMessageBox::Yes);
        if (choice == QMessageBox::Cancel) return;
        if (choice == QMessageBox::No) {
            const QString savePath = GUI::showNativeSaveFileDialog(this,
                "Save Sine Color As", GUI::SineStore::directoryPath(),
                QFileInfo(
                    GUI::Util::uniqueIndexedPathWithExtension(
                        GUI::SineStore::directoryPath(), targetName, "txt"))
                    .fileName(),
                "Sine Files (*.txt);;All Files (*.*)");
            if (savePath.isEmpty()) return;

            const QString savePathWithExtension = QString::fromStdString(
                PathUtil::appendExtension(savePath.toStdString(), "txt"));
            const QString saveName = GUI::PaletteStore::normalizeName(
                QFileInfo(savePathWithExtension).completeBaseName());
            if (!GUI::PaletteStore::isValidName(saveName)) {
                QMessageBox::warning(this, "Save Sine Color",
                    "Use an ASCII name with letters, numbers, spaces, ., _, or "
                    "-.");
                return;
            }

            targetName = saveName;
            destinationPath = GUI::SineStore::filePath(targetName);
        }
    }

    if (!GUI::SineStore::saveToPath(
            destinationPath, _state.sinePalette, errorMessage)) {
        QMessageBox::warning(this, "Save Sine Color", errorMessage);
        return;
    }

    _state.sineName = targetName;
    _markSineSavedState(_state.sineName, _state.sinePalette);
    _setStatusSavedPath(QString::fromStdWString(destinationPath.wstring()));
    _refreshSineList(_state.sineName);
    _syncControlsFromState();
    _updateStatusLabels();
}

void ControlWindow::_savePointView() {
    _syncStateFromControls();
    _state.zoom = GUI::Util::clampGUIZoom(_state.zoom);

    QString errorMessage;
    if (!GUI::PointStore::ensureDirectory(errorMessage)) {
        QMessageBox::warning(this, "Save View", errorMessage);
        return;
    }

    const QString defaultPath = GUI::Util::uniqueIndexedPathWithExtension(
        GUI::PointStore::directoryPath(), "View", "txt");
    const QString selectedPath
        = GUI::showNativeSaveFileDialog(this, "Save View",
            GUI::PointStore::directoryPath(), QFileInfo(defaultPath).fileName(),
            "View Files (*.txt);;All Files (*.*)");
    if (selectedPath.isEmpty()) return;

    PointConfig point
        = { .fractal = GUI::Util::fractalTypeToConfigString(_state.fractalType)
                  .toStdString(),
              .inverse = _state.inverse,
              .julia = _state.julia,
              .iterations = _state.iterations,
              .real = _pointRealText.toStdString(),
              .imag = _pointImagText.toStdString(),
              .zoom = _zoomText.toStdString(),
              .seedReal = _seedRealText.toStdString(),
              .seedImag = _seedImagText.toStdString() };

    const QString outputPathWithExtension = QString::fromStdString(
        PathUtil::appendExtension(selectedPath.toStdString(), "txt"));
    if (!GUI::PointStore::saveToPath(
            outputPathWithExtension.toStdString(), point, errorMessage)) {
        QMessageBox::warning(this, "Save View", errorMessage);
        return;
    }
    _markPointViewSavedState();
    _setStatusSavedPath(outputPathWithExtension);
    _updateStatusLabels();
}

void ControlWindow::_loadPointView() {
    QString errorMessage;
    if (!GUI::PointStore::ensureDirectory(errorMessage)) {
        QMessageBox::warning(this, "Load View", errorMessage);
        return;
    }

    const QString defaultPath
        = QString::fromStdWString(GUI::PointStore::directoryPath().wstring());
    const QString selectedPath = GUI::showNativeOpenFileDialog(this,
        "Load View", GUI::PointStore::directoryPath(),
        "View Files (*.txt);;All Files (*.*)");
    if (selectedPath.isEmpty()) return;

    PointConfig point;
    if (!GUI::PointStore::loadFromPath(
            selectedPath.toStdString(), point, errorMessage)) {
        QMessageBox::warning(this, "Load View", errorMessage);
        return;
    }

    _state.fractalType = GUI::Util::fractalTypeFromConfigString(
        QString::fromStdString(point.fractal));
    _state.inverse = point.inverse;
    _state.julia = point.julia;
    _state.iterations = std::max(0, point.iterations);
    _zoomText = QString::fromStdString(point.zoom);
    _pointRealText = QString::fromStdString(point.real);
    _pointImagText = QString::fromStdString(point.imag);
    _seedRealText = QString::fromStdString(point.seedReal);
    _seedImagText = QString::fromStdString(point.seedImag);
    _syncStateZoomFromText();
    _syncStatePointFromText();
    _syncStateSeedFromText();
    _markPointViewSavedState();
    _syncControlsFromState();
    requestRender();
}

void ControlWindow::_importSine() {
    const QString sourcePath = GUI::showNativeOpenFileDialog(this,
        "Import Sine Color", GUI::SineStore::directoryPath(),
        "Sine Files (*.txt);;All Files (*.*)");
    if (sourcePath.isEmpty()) return;

    Backend::SinePaletteConfig loaded;
    QString errorMessage;
    if (!GUI::SineStore::loadFromPath(
            sourcePath.toStdString(), loaded, errorMessage)) {
        QMessageBox::warning(this, "Sine Color", errorMessage);
        return;
    }

    if (!GUI::SineStore::ensureDirectory(errorMessage)) {
        QMessageBox::warning(this, "Sine Color", errorMessage);
        return;
    }

    const QStringList existingNames = GUI::SineStore::listNames();
    const QString importedName = GUI::PaletteStore::uniqueName(
        QFileInfo(sourcePath).completeBaseName(), existingNames);
    const std::filesystem::path destinationPath
        = GUI::SineStore::filePath(importedName);
    if (!GUI::SineStore::saveToPath(destinationPath, loaded, errorMessage)) {
        QMessageBox::warning(this, "Sine Color", errorMessage);
        return;
    }

    _state.sineName = importedName;
    _state.sinePalette = loaded;
    _markSineSavedState(_state.sineName, _state.sinePalette);
    _setStatusSavedPath(QString::fromStdWString(destinationPath.wstring()));
    _refreshSineList(_state.sineName);
    _syncControlsFromState();
    _updateStatusLabels();
    requestRender();
}

void ControlWindow::_loadPalette() {
    const QString sourcePath = GUI::showNativeOpenFileDialog(this, "Load Palette",
        GUI::PaletteStore::directoryPath(),
        "Palette Files (*.txt);;All Files (*.*)");
    if (sourcePath.isEmpty()) return;

    Backend::PaletteHexConfig loaded;
    QString importedName;
    std::filesystem::path destinationPath;
    QString errorMessage;
    if (!GUI::PaletteStore::importFromPath(sourcePath.toStdString(),
            _state.palette.totalLength, _state.palette.offset, importedName,
            loaded, destinationPath, errorMessage)) {
        QMessageBox::warning(this, "Palette", errorMessage);
        return;
    }

    _state.paletteName = importedName;
    _state.palette = loaded;
    _markPaletteSavedState(_state.paletteName, _state.palette);
    _palettePreviewDirty = true;
    _setStatusSavedPath(QString::fromStdWString(destinationPath.wstring()));
    _refreshPaletteList(_state.paletteName);
    _syncControlsFromState();
    _updateStatusLabels();
    _updateViewport();
    requestRender();
}

void ControlWindow::_savePalette() {
    _syncStateFromControls();

    QString targetName = GUI::PaletteStore::normalizeName(_state.paletteName);
    if (!GUI::PaletteStore::isValidName(targetName)
        || targetName == kNewEntryLabel) {
        const QString suggested
            = GUI::PaletteStore::sanitizeName(_state.paletteName);
        bool accepted = false;
        const QString enteredName = QInputDialog::getText(this, "Save Palette",
            "Name", QLineEdit::Normal,
            suggested.isEmpty() ? "palette" : suggested, &accepted);
        if (!accepted) return;

        targetName = GUI::PaletteStore::normalizeName(enteredName);
        if (!GUI::PaletteStore::isValidName(targetName)) {
            QMessageBox::warning(this, "Save Palette",
                "Use an ASCII name with letters, numbers, spaces, ., _, or -.");
            return;
        }
    }

    QString errorMessage;
    if (!GUI::PaletteStore::ensureDirectory(errorMessage)) {
        QMessageBox::warning(this, "Save Palette", errorMessage);
        return;
    }

    std::filesystem::path destinationPath
        = GUI::PaletteStore::filePath(targetName);
    std::error_code existsError;
    const bool destinationExists
        = std::filesystem::exists(destinationPath, existsError) && !existsError;
    if (destinationExists) {
        const QMessageBox::StandardButton choice = QMessageBox::question(this,
            "Palette",
            QString("Palette \"%1\" already exists. Overwrite it?")
                .arg(targetName),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
            QMessageBox::Yes);
        if (choice == QMessageBox::Cancel) return;
        if (choice == QMessageBox::No) {
            const QString savePath = GUI::showNativeSaveFileDialog(this,
                "Save Palette As", GUI::PaletteStore::directoryPath(),
                QFileInfo(GUI::Util::uniqueIndexedPathWithExtension(
                    GUI::PaletteStore::directoryPath(), targetName, "txt"))
                    .fileName(),
                "Palette Files (*.txt);;All Files (*.*)");
            if (savePath.isEmpty()) return;

            const QString savePathWithExtension = QString::fromStdString(
                PathUtil::appendExtension(savePath.toStdString(), "txt"));
            QString saveName;
            if (!GUI::PaletteStore::saveFromDialogPath(savePathWithExtension,
                    _state.palette, saveName, destinationPath,
                    errorMessage)) {
                QMessageBox::warning(this, "Save Palette",
                    errorMessage);
                return;
            }

            targetName = saveName;
        } else if (!GUI::PaletteStore::saveNamed(
                       targetName, _state.palette, destinationPath,
                       errorMessage)) {
            QMessageBox::warning(this, "Save Palette", errorMessage);
            return;
        }
    }

    if (!destinationExists
        && !GUI::PaletteStore::saveNamed(
            targetName, _state.palette, destinationPath, errorMessage)) {
        QMessageBox::warning(this, "Save Palette", errorMessage);
        return;
    }

    _state.paletteName = targetName;
    _markPaletteSavedState(_state.paletteName, _state.palette);
    _setStatusSavedPath(QString::fromStdWString(destinationPath.wstring()));
    _refreshPaletteList(_state.paletteName);
    _syncControlsFromState();
    _updateStatusLabels();
}

bool ControlWindow::_loadSineByName(
    const QString& name, bool requestRenderOnSuccess, QString* errorMessage) {
    if (errorMessage) errorMessage->clear();

    const QString normalizedName = GUI::PaletteStore::normalizeName(name);
    if (normalizedName.isEmpty()) return false;

    Backend::SinePaletteConfig loaded;
    const std::filesystem::path sourcePath
        = GUI::SineStore::filePath(normalizedName);
    QString localError;
    if (std::filesystem::exists(sourcePath)) {
        if (!GUI::SineStore::loadFromPath(sourcePath, loaded, localError)) {
            if (errorMessage) *errorMessage = localError;
            return false;
        }
    } else {
        localError = QString("Sine file not found: %1").arg(normalizedName);
        if (errorMessage) *errorMessage = localError;
        return false;
    }

    _state.sineName = normalizedName;
    _state.sinePalette = loaded;
    _markSineSavedState(_state.sineName, _state.sinePalette);
    _syncControlsFromState();
    _updateViewport();
    if (requestRenderOnSuccess) {
        requestRender();
    }
    return true;
}

bool ControlWindow::_loadPaletteByName(
    const QString& name, bool requestRenderOnSuccess, QString* errorMessage) {
    if (errorMessage) errorMessage->clear();

    const QString normalizedName = GUI::PaletteStore::normalizeName(name);
    if (normalizedName.isEmpty()) return false;

    Backend::PaletteHexConfig loaded;
    QString localError;
    if (!GUI::PaletteStore::loadNamed(normalizedName, loaded, localError)) {
        if (errorMessage) *errorMessage = localError;
        return false;
    }

    _state.paletteName = normalizedName;
    _state.palette = loaded;
    _markPaletteSavedState(_state.paletteName, _state.palette);
    _palettePreviewDirty = true;
    _syncControlsFromState();
    _updateViewport();
    if (requestRenderOnSuccess) {
        requestRender();
    }
    return true;
}

void ControlWindow::_openPaletteEditor() {
    const Backend::PaletteHexConfig original = _state.palette;
    const QString originalName = _state.paletteName;
    PaletteDialog dialog(
        _state.palette, _state.paletteName,
        [this](const QString& path) { _setStatusSavedPath(path); }, this);

    if (dialog.exec() == QDialog::Accepted) {
        _state.palette = dialog.palette();
        _state.palette.totalLength
            = static_cast<float>(_paletteLengthSpin->value());
        _state.palette.offset = static_cast<float>(_paletteOffsetSpin->value());
        if (!dialog.savedPaletteName().isEmpty() && !dialog.savedStateDirty()) {
            _state.paletteName = dialog.savedPaletteName();
            _markPaletteSavedState(_state.paletteName, _state.palette);
            _refreshPaletteList(_state.paletteName);
        }
        _palettePreviewDirty = true;
        _updateViewport();
        requestRender();
    } else {
        _state.palette = original;
        _state.paletteName = originalName;
        _refreshPaletteList(_state.paletteName);
        _palettePreviewDirty = true;
        _updateViewport();
    }
}

bool ControlWindow::_commitImageSave(
    const QString& path, bool appendDate, const QString& type) {
    QString error;
    if (!_ensureBackendReady(error)) {
        QMessageBox::warning(this, "Save", error);
        return false;
    }

    if (_previewImage.isNull()) {
        QMessageBox::warning(this, "Save", "No image is available.");
        return false;
    }

    Backend::Status status;
    {
        status = _renderSession->saveImage(
            path.toStdString(), appendDate, type.toStdString());
    }
    if (!status) {
        QMessageBox::warning(
            this, "Save", QString::fromStdString(status.message));
        return false;
    }

    return true;
}

void ControlWindow::_saveImage() {
    auto result = GUI::runSaveImageDialog(this, "mandelbrot.png");
    if (!result) return;

    _commitImageSave(result->path, result->appendDate, result->type);
}

void ControlWindow::quickSaveImage() {
    std::error_code ec;
    std::filesystem::create_directories(GUI::Util::savesDirectoryPath(), ec);
    const QString path = QString::fromStdWString(
        (GUI::Util::savesDirectoryPath() / "mandelbrot.png").wstring());
    _commitImageSave(path, true, "png");
}

void ControlWindow::_resizeViewportImage() {
    _syncStateFromControls();
    _resizeViewportWindowToImageSize();
    requestRender(true);
}

void ControlWindow::_updateSelectionTarget(int index) {
    _selectionTarget = static_cast<SelectionTarget>(index);
}

void ControlWindow::_updateNavMode(int index) {
    _navMode = static_cast<NavMode>(index);
}

void ControlWindow::_updateAspectLinkedSizes(bool widthChanged) {
    if (!_preserveRatioCheck || !_preserveRatioCheck->isChecked()) return;
    if (!_outputWidthSpin || !_outputHeightSpin) return;
    if (_state.outputWidth <= 0 || _state.outputHeight <= 0) return;

    if (widthChanged) {
        const int newWidth = _outputWidthSpin->value();
        const int newHeight = std::max(1,
            static_cast<int>(std::lround(static_cast<double>(newWidth)
                * _state.outputHeight / std::max(1, _state.outputWidth))));
        const QSignalBlocker blocker(_outputHeightSpin);
        _outputHeightSpin->setValue(newHeight);
        return;
    }

    const int newHeight = _outputHeightSpin->value();
    const int newWidth = std::max(1,
        static_cast<int>(std::lround(static_cast<double>(newHeight)
            * _state.outputWidth / std::max(1, _state.outputHeight))));
    const QSignalBlocker blocker(_outputWidthSpin);
    _outputWidthSpin->setValue(newWidth);
}

void ControlWindow::_markSineSavedState(
    const QString& name, const Backend::SinePaletteConfig& palette) {
    _savedSineName = GUI::PaletteStore::normalizeName(name);
    _savedSinePalette = palette;
    _hasSavedSineState = true;
}

void ControlWindow::_markPaletteSavedState(
    const QString& name, const Backend::PaletteHexConfig& palette) {
    _savedPaletteName = GUI::PaletteStore::normalizeName(name);
    _savedPalette = palette;
    _hasSavedPaletteState = true;
}

SavedPointViewState ControlWindow::_capturePointViewState() const {
    return SavedPointViewState { .fractalType = _state.fractalType,
        .inverse = _state.inverse,
        .julia = _state.julia,
        .iterations = _state.iterations,
        .real = _pointRealText,
        .imag = _pointImagText,
        .zoom = _zoomText,
        .seedReal = _seedRealText,
        .seedImag = _seedImagText };
}

void ControlWindow::_markPointViewSavedState() {
    _savedPointViewState = _capturePointViewState();
    _hasSavedPointViewState = true;
}

bool ControlWindow::_isSineDirty() const {
    if (!_hasSavedSineState) return true;

    if (GUI::PaletteStore::normalizeName(_state.sineName)
            .compare(_savedSineName, Qt::CaseInsensitive)
        != 0) {
        return true;
    }

    return !GUI::SineStore::sameConfig(_state.sinePalette, _savedSinePalette);
}

bool ControlWindow::_isPaletteDirty() const {
    if (!_hasSavedPaletteState) return true;

    if (GUI::PaletteStore::normalizeName(_state.paletteName)
            .compare(_savedPaletteName, Qt::CaseInsensitive)
        != 0) {
        return true;
    }

    return !GUI::PaletteStore::sameConfig(_state.palette, _savedPalette);
}

bool ControlWindow::_isPointViewDirty() const {
    if (!_hasSavedPointViewState) return true;

    const SavedPointViewState current = _capturePointViewState();
    const SavedPointViewState& saved = _savedPointViewState;
    if (current.fractalType != saved.fractalType
        || current.inverse != saved.inverse || current.julia != saved.julia
        || current.iterations != saved.iterations
        || !NumberUtil::equalParsedDoubleText(
            current.real.toStdString(), saved.real.toStdString())
        || !NumberUtil::equalParsedDoubleText(
            current.imag.toStdString(), saved.imag.toStdString())
        || !NumberUtil::equalParsedDoubleText(
            current.zoom.toStdString(), saved.zoom.toStdString())
        || !NumberUtil::equalParsedDoubleText(
            current.seedReal.toStdString(), saved.seedReal.toStdString())
        || !NumberUtil::equalParsedDoubleText(
            current.seedImag.toStdString(), saved.seedImag.toStdString())) {
        return true;
    }

    return false;
}

void ControlWindow::_refreshDirtyComboLabels() {
    auto refreshCombo = [](QComboBox* combo, const QString& currentName,
                            bool isDirty) {
        if (!combo) return;
        if (currentName.isEmpty()) return;

        const QSignalBlocker blocker(combo);
        for (int i = 0; i < combo->count(); ++i) {
            const QString itemData = combo->itemData(i).toString();
            const QString baseName = itemData.isEmpty()
                ? GUI::Util::undecoratedLabel(combo->itemText(i))
                : itemData;
            if (baseName.isEmpty() || baseName == kNewEntryLabel) continue;

            const bool unsavedItem = isDirty
                && baseName.compare(currentName, Qt::CaseInsensitive) == 0;
            const QString desiredLabel
                = GUI::Util::decorateUnsavedLabel(baseName, unsavedItem);
            if (combo->itemText(i) != desiredLabel) {
                combo->setItemText(i, desiredLabel);
            }
        }
    };

    refreshCombo(_sineCombo, _state.sineName, _isSineDirty());
    refreshCombo(_paletteCombo, _state.paletteName, _isPaletteDirty());
}

QString ControlWindow::_stateToString(double value, int precision) const {
    return QString::number(value, 'g', precision);
}

void ControlWindow::_syncZoomTextFromState() {
    _zoomText = _stateToString(_state.zoom, 17);
}

void ControlWindow::_syncStateZoomFromText() {
    bool ok = false;
    const double zoom = _zoomText.toDouble(&ok);
    if (ok) {
        _state.zoom = GUI::Util::clampGUIZoom(zoom);
        if (!NumberUtil::almostEqual(_state.zoom, zoom)) {
            _zoomText = _stateToString(_state.zoom, 17);
        }
    }
}

void ControlWindow::_syncPointTextFromState() {
    _pointRealText = _stateToString(_state.point.x());
    _pointImagText = _stateToString(_state.point.y());
}

void ControlWindow::_syncStatePointFromText() {
    bool okReal = false, okImag = false;
    const double real = _pointRealText.toDouble(&okReal);
    const double imag = _pointImagText.toDouble(&okImag);
    if (okReal) _state.point.setX(real);
    if (okImag) _state.point.setY(imag);
}

void ControlWindow::_syncSeedTextFromState() {
    _seedRealText = _stateToString(_state.seed.x(), 17);
    _seedImagText = _stateToString(_state.seed.y(), 17);
}

void ControlWindow::_syncStateSeedFromText() {
    bool okReal = false, okImag = false;
    const double real = _seedRealText.toDouble(&okReal);
    const double imag = _seedImagText.toDouble(&okImag);
    if (okReal) _state.seed.setX(real);
    if (okImag) _state.seed.setY(imag);
}

QPoint ControlWindow::_clampPixelToOutput(const QPoint& pixel) const {
    const QSize size = outputSize();
    return { std::clamp(pixel.x(), 0, std::max(0, size.width() - 1)),
        std::clamp(pixel.y(), 0, std::max(0, size.height() - 1)) };
}

int ControlWindow::_resolveCurrentIterationCount() const {
    if (_backend && _renderSession) {
        return std::max(1, _renderSession->currentIterationCount());
    }

    return std::max(1, _state.iterations);
}

void ControlWindow::_resizeViewportWindowToImageSize() {
    auto* viewport = _viewport;
    if (viewport && viewport->isFullScreen()) {
        return;
    }

    GUI::Util::resizeViewportToImageSize(_viewport, outputSize());
}

int ControlWindow::interactionFrameIntervalMs() const {
    const int fps = std::max(1,
        _state.interactionTargetFPS > 0 ? _state.interactionTargetFPS
                                        : kDefaultInteractionTargetFPS);
    return std::max(
        1, static_cast<int>(std::lround(1000.0 / static_cast<double>(fps))));
}

bool ControlWindow::shouldUseInteractionPreviewFallback() const {
    return _presentingCopiedPreview;
}

void ControlWindow::_markPreviewMotion() {
    _lastPreviewMotionAt = std::chrono::steady_clock::now();
    if (_presentingCopiedPreview) {
        _previewStillTimer.start();
    }
}

void ControlWindow::_tryResumeDirectPreview() {
    if (!_presentingCopiedPreview) {
        return;
    }
    if (_renderInFlight) {
        _previewStillTimer.start();
        return;
    }

    bool hasQueuedRender = false;
    {
        const std::scoped_lock lock(_renderMutex);
        hasQueuedRender = _queuedRenderRequest.has_value();
    }
    if (hasQueuedRender) {
        _previewStillTimer.start();
        return;
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - _lastPreviewMotionAt);
    if (elapsed.count() < kPreviewStillMs) {
        _previewStillTimer.start();
        return;
    }

    if (_previewImage.isNull() || !_renderSession) {
        return;
    }

    const Backend::ImageView view = _renderSession->imageView();
    if (!view.outputPixels || view.outputWidth <= 0 || view.outputHeight <= 0) {
        return;
    }

    QImage releasedImage;
    _previewImage.swap(releasedImage);
    _previewImage = GUI::Util::wrapImageViewToImage(view);
    _previewUsesBackendMemory = true;
    _directFrameDetachedForRender = false;
    _presentingCopiedPreview = false;
    _interactionPreviewFallbackLatched.store(false, std::memory_order_release);
    const QWidget* dprSource = _viewport ? static_cast<QWidget*>(_viewport)
                                         : static_cast<QWidget*>(this);
    _previewImage.setDevicePixelRatio(
        GUI::Util::effectiveDevicePixelRatio(dprSource));
    _updateViewport();
}

bool ControlWindow::matchesShortcut(
    const QString& id, const QKeyEvent* event) const {
    if (!event) return false;

    const QKeySequence configured = _shortcuts.sequence(id);
    if (configured.isEmpty()) return false;

    const QKeyCombination combo(
        event->modifiers(), static_cast<Qt::Key>(event->key()));
    return configured.matches(QKeySequence(combo)) == QKeySequence::ExactMatch;
}

double ControlWindow::zoomRateFactor() const {
    return _currentZoomFactor(_state.zoomRate);
}

double ControlWindow::panRateFactor() const {
    constexpr double panRateBase = 1.125;
    const int clamped = GUI::Util::clampSliderValue(_state.panRate);
    return std::pow(panRateBase, static_cast<double>(clamped - 8));
}

double ControlWindow::_currentZoomFactor(int sliderValue) const {
    return kZoomStepTable[GUI::Util::clampSliderValue(sliderValue)];
}
