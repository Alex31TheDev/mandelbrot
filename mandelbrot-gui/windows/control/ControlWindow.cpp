#include "ControlWindow.h"
#include "ui_ControlWindow.h"

#include <algorithm>
#include <array>
#include <cmath>

#include <QApplication>
#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QKeyEvent>
#include <QLineEdit>
#include <QProgressBar>
#include <QScrollArea>
#include <QScrollBar>
#include <QScreen>
#include <QSignalBlocker>
#include <QStyleFactory>
#include <QWheelEvent>
#include <QWindow>

#include "services/PaletteStore.h"
#include "widgets/AdaptiveDoubleSpinBox.h"
#include "widgets/CollapsibleGroupBox.h"
#include "widgets/IterationSpinBox.h"
#include "widgets/MarqueeLabel.h"
#include "widgets/PalettePreviewWidget.h"
#include "widgets/SinePreviewWidget.h"

#include "app/GUISessionState.h"
using namespace GUI;

#include "BackendAPI.h"
using namespace Backend;

#include "util/GUIUtil.h"

static const int controlWindowScreenPadding = 48;

static int controlScrollBarExtent(const QScrollArea *area) {
    if (!area) return 0;
    if (const QScrollBar *scrollBar = area->verticalScrollBar()) {
        return std::max(0, scrollBar->sizeHint().width());
    }
    return 0;
}

ControlWindow::ControlWindow(QWidget *parent)
    : QMainWindow(parent)
    , _ui(std::make_unique<Ui::ControlWindow>()) {
    _buildUI();
    _populateStaticControls();
    _connectUI();
    qApp->installEventFilter(this);
}

ControlWindow::~ControlWindow() {
    qApp->removeEventFilter(this);
}

void ControlWindow::_buildUI() {
    setWindowFlag(Qt::Window, true);
    _ui->setupUi(this);
    if (_ui->outerLayout) {
        _ui->outerLayout->setSizeConstraint(QLayout::SetMinimumSize);
    }
    if (_ui->controlLayout) {
        _ui->controlLayout->setSizeConstraint(QLayout::SetMinimumSize);
    }
    _ui->controlScrollArea->setStyleSheet(QString());

    if (QScrollBar *scrollBar = _ui->controlScrollArea->verticalScrollBar()) {
        scrollBar->setStyleSheet(QString());
        if (QStyleFactory::keys().contains("windowsvista")) {
            scrollBar->setStyle(QStyleFactory::create("windowsvista"));
        }

        const int scrollBarWidth = std::max(
            scrollBar->sizeHint().width(), scrollBar->minimumSizeHint().width());
        scrollBar->setMinimumWidth(scrollBarWidth);
        scrollBar->setMaximumWidth(scrollBarWidth);
    }

    const auto onSectionToggled = [this](bool) { _updateControlWindowSize(); };
    const std::array<QGroupBox *, 8> groups = { _ui->cpuGroup, _ui->renderGroup,
        _ui->infoGroup, _ui->viewportGroup, _ui->sineGroup, _ui->paletteGroup,
        _ui->lightGroup, static_cast<QGroupBox *>(_ui->imageGroup) };
    for (QGroupBox *group : groups) {
        auto *collapsible = qobject_cast<::CollapsibleGroupBox *>(group);
        if (!collapsible) continue;
        connect(collapsible, &::CollapsibleGroupBox::expandedChanged, this,
            onSectionToggled);
        collapsible->applyExpandedState(collapsible->isChecked());
    }

    if (auto *spin = qobject_cast<::IterationSpinBox *>(_ui->iterationsSpin)) {
        spin->resolveAutoIterations = [this]() {
            return _iterationAutoResolver ? std::max(1, _iterationAutoResolver()) : 1;
            };
    }
    if (QLineEdit *iterationsEdit = _ui->iterationsSpin->findChild<QLineEdit *>()) {
        iterationsEdit->installEventFilter(this);
    }
    _ui->paletteCombo->installEventFilter(this);

    if (auto *spin = qobject_cast<::AdaptiveDoubleSpinBox *>(_ui->exponentSpin)) {
        spin->setDefaultDisplayDecimals(2);
    }

    QDoubleSpinBox compactSpinProbe;
    compactSpinProbe.setRange(0.0001, 1000.0);
    compactSpinProbe.setDecimals(4);
    compactSpinProbe.setSingleStep(0.01);
    const int compactSpinWidth = compactSpinProbe.sizeHint().width();
    for (QDoubleSpinBox *spin : { _ui->freqRSpin, _ui->freqGSpin, _ui->freqBSpin,
             _ui->freqMultSpin, _ui->paletteLengthSpin,
             _ui->paletteOffsetSpin }) {
        spin->setFixedWidth(compactSpinWidth);
        if (auto *adaptive = qobject_cast<::AdaptiveDoubleSpinBox *>(spin)) {
            adaptive->setDefaultDisplayDecimals(4);
        }
    }

    for (QPushButton *button : { _ui->savePointButton, _ui->loadPointButton,
             _ui->randomizeSineButton, _ui->importSineButton,
             _ui->saveSineButton, _ui->paletteLoadButton,
             _ui->paletteSaveButton, _ui->paletteEditorButton,
             _ui->lightColorButton, _ui->resizeButton, _ui->calculateButton,
             _ui->homeButton, _ui->zoomButton, _ui->saveButton }) {
        Util::stabilizePushButton(button);
    }

    connect(qobject_cast<::SinePreviewWidget *>(_ui->sinePreview),
        &::SinePreviewWidget::rangeChanged, this,
        [this](double, double) { emit previewRefreshRequested(); });

    _ui->statusRightLabel->setMinimumWidth(0);
    _ui->statusRightLabel->setSizePolicy(
        QSizePolicy::Ignored, QSizePolicy::Preferred);
    _ui->statusLayout->setStretch(
        _ui->statusLayout->indexOf(_ui->statusRightLabel), 1);
    connect(_ui->statusRightLabel, &MarqueeLabel::layoutWidthChanged, this,
        [this]() {
            _updateStatusRightEdgeAlignment();
            _updateStatusLabels();
        });
}

void ControlWindow::_populateStaticControls() {
    _ui->colorMethodCombo->addItems({ tr("Iterations"), tr("Smooth Iterations"),
        tr("Palette"), tr("Light") });
    _ui->fractalCombo->addItems(
        { tr("Mandelbrot"), tr("Perpendicular"), tr("Burning Ship") });
    _ui->navModeCombo->addItems({ tr("Realtime Zoom"), tr("Box Zoom"), tr("Pan") });
    _ui->pickTargetCombo->addItems(
        { tr("Zoom Point"), tr("Seed Point"), tr("Light Point") });
}

void ControlWindow::_connectUI() {
    connect(_ui->backendCombo, &QComboBox::currentTextChanged, this,
        &ControlWindow::backendSelectionRequested);

    const auto emitRender = [this]() { emit renderRequested(); };
    connect(_ui->iterationsSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
        [emitRender](int) { emitRender(); });
    connect(_ui->useThreadsCheckBox, &QCheckBox::toggled, this,
        &ControlWindow::threadUsageChanged);
    connect(_ui->colorMethodCombo,
        QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [emitRender](int) { emitRender(); });
    connect(_ui->fractalCombo,
        QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [emitRender](int) { emitRender(); });
    connect(_ui->juliaCheck, &QCheckBox::toggled, this,
        [emitRender](bool) { emitRender(); });
    connect(_ui->inverseCheck, &QCheckBox::toggled, this,
        [emitRender](bool) { emitRender(); });
    connect(_ui->aaSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
        [emitRender](int) { emitRender(); });
    connect(_ui->exponentSlider, &QSlider::valueChanged, this,
        [this](int value) {
            const double exponent = value / 100.0;
            {
                const QSignalBlocker blocker(_ui->exponentSpin);
                _ui->exponentSpin->setValue(exponent);
            }
            emit renderRequested();
        });
    connect(_ui->exponentSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, [this](double value) {
            const QSignalBlocker blocker(_ui->exponentSlider);
            _ui->exponentSlider->setValue(
                static_cast<int>(std::round(std::max(1.01, value) * 100.0)));
            emit renderRequested();
        });

    connect(_ui->navModeCombo,
        QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this](int index) { emit navModeChanged(static_cast<NavMode>(index)); });
    connect(_ui->pickTargetCombo,
        QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this](int index) {
            emit selectionTargetChanged(static_cast<SelectionTarget>(index));
        });

    connect(_ui->panRateSlider, &QSlider::valueChanged, this, [this](int value) {
        const QSignalBlocker blocker(_ui->panRateSpin);
        _ui->panRateSpin->setValue(value);
        });
    connect(_ui->panRateSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
        [this](int value) {
            const QSignalBlocker blocker(_ui->panRateSlider);
            _ui->panRateSlider->setValue(std::clamp(
                value, _ui->panRateSlider->minimum(), _ui->panRateSlider->maximum()));
        });
    connect(_ui->zoomRateSlider, &QSlider::valueChanged, this, [this](int value) {
        const QSignalBlocker blocker(_ui->zoomRateSpin);
        _ui->zoomRateSpin->setValue(value);
        });
    connect(_ui->zoomRateSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
        [this](int value) {
            const QSignalBlocker blocker(_ui->zoomRateSlider);
            _ui->zoomRateSlider->setValue(std::clamp(
                value, _ui->zoomRateSlider->minimum(), _ui->zoomRateSlider->maximum()));
        });

    connect(_ui->sineCombo, &QComboBox::currentTextChanged, this,
        [this](const QString &) {
            const QString selected = _ui->sineCombo->currentData().toString();
            if (selected == Util::translatedNewEntryLabel()
                || _ui->sineCombo->currentIndex() == 0) {
                emit newSineRequested();
                return;
            }

            emit sineSelectionRequested(
                selected.isEmpty()
                ? Util::undecoratedLabel(_ui->sineCombo->currentText())
                : selected);
        });
    for (QDoubleSpinBox *spin : { _ui->freqRSpin, _ui->freqGSpin, _ui->freqBSpin,
             _ui->freqMultSpin }) {
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this, emitRender](double) {
                emit sineStateEdited();
                emitRender();
            });
    }
    connect(_ui->randomizeSineButton, &QPushButton::clicked, this,
        &ControlWindow::randomizeSineRequested);
    connect(_ui->importSineButton, &QPushButton::clicked, this,
        &ControlWindow::importSineRequested);
    connect(_ui->saveSineButton, &QPushButton::clicked, this,
        &ControlWindow::saveSineRequested);

    connect(_ui->paletteCombo, &QComboBox::currentTextChanged, this,
        [this](const QString &) {
            const QString selected = _ui->paletteCombo->currentData().toString();
            if (selected == Util::translatedNewEntryLabel()
                || _ui->paletteCombo->currentIndex() == 0) {
                emit newPaletteRequested();
                return;
            }

            emit paletteSelectionRequested(
                selected.isEmpty()
                ? Util::undecoratedLabel(_ui->paletteCombo->currentText())
                : selected);
        });
    connect(_ui->paletteLengthSpin,
        QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
        [this, emitRender](double) {
            emit paletteStateEdited();
            emitRender();
        });
    connect(_ui->paletteOffsetSpin,
        QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
        [this, emitRender](double) {
            emit paletteStateEdited();
            emitRender();
        });
    connect(_ui->paletteLoadButton, &QPushButton::clicked, this,
        &ControlWindow::loadPaletteRequested);
    connect(_ui->paletteSaveButton, &QPushButton::clicked, this,
        &ControlWindow::savePaletteRequested);
    connect(_ui->paletteEditorButton, &QPushButton::clicked, this,
        &ControlWindow::paletteEditorRequested);

    connect(_ui->lightColorButton, &QPushButton::clicked, this,
        &ControlWindow::lightColorChangeRequested);
    connect(_ui->outputWidthSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
        [this](int) { _updateAspectLinkedSizes(true); });
    connect(_ui->outputHeightSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
        [this](int) { _updateAspectLinkedSizes(false); });
    connect(_ui->viewportScaleSpin,
        QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
        [this](double value) {
            const float scalePercent = std::max(1.0f, static_cast<float>(value));
            emit viewportScaleChanged(scalePercent);
        });
    connect(_ui->preserveRatioCheck, &QCheckBox::toggled, this,
        [this](bool checked) {
            if (!checked) return;
            _lastOutputSize = QSize(
                std::max(1, _ui->outputWidthSpin->value()),
                std::max(1, _ui->outputHeightSpin->value()));
            _aspectReferenceInitialized = true;
        });

    connect(_ui->calculateButton, &QPushButton::clicked, this,
        &ControlWindow::renderRequested);
    connect(_ui->homeButton, &QPushButton::clicked, this,
        &ControlWindow::homeRequested);
    connect(_ui->zoomButton, &QPushButton::clicked, this,
        &ControlWindow::zoomCenterRequested);
    connect(_ui->saveButton, &QPushButton::clicked, this,
        &ControlWindow::saveImageRequested);
    connect(_ui->resizeButton, &QPushButton::clicked, this,
        [this]() {
            _lastOutputSize = QSize(
                std::max(1, _ui->outputWidthSpin->value()),
                std::max(1, _ui->outputHeightSpin->value()));
            _aspectReferenceInitialized = true;
            emit viewportResizeRequested();
        });
    connect(_ui->savePointButton, &QPushButton::clicked, this,
        &ControlWindow::saveViewRequested);
    connect(_ui->loadPointButton, &QPushButton::clicked, this,
        &ControlWindow::loadViewRequested);

    const auto applyViewTextEdits = [this]() { emit renderRequested(); };
    const auto connectViewTextEdit = [this, applyViewTextEdits](QLineEdit *edit) {
        if (!edit) return;
        Util::markLineEditTextCommitted(edit);
        connect(edit, &QLineEdit::editingFinished, this,
            [edit, applyViewTextEdits]() {
                if (!Util::hasUncommittedLineEditChange(edit)) return;
                applyViewTextEdits();
            });
        };
    connectViewTextEdit(_ui->infoRealEdit);
    connectViewTextEdit(_ui->infoImagEdit);
    connectViewTextEdit(_ui->infoZoomEdit);
    connectViewTextEdit(_ui->infoSeedRealEdit);
    connectViewTextEdit(_ui->infoSeedImagEdit);

    connect(_ui->actionHome, &QAction::triggered, this,
        &ControlWindow::homeRequested);
    connect(_ui->actionOpenView, &QAction::triggered, this,
        &ControlWindow::loadViewRequested);
    connect(_ui->actionSaveView, &QAction::triggered, this,
        &ControlWindow::saveViewRequested);
    connect(_ui->actionSaveImage, &QAction::triggered, this,
        &ControlWindow::saveImageRequested);
    connect(_ui->actionQuickSave, &QAction::triggered, this,
        &ControlWindow::quickSaveRequested);
    connect(_ui->actionSettings, &QAction::triggered, this,
        &ControlWindow::settingsRequested);
    connect(_ui->actionClearSavedSettings, &QAction::triggered, this,
        &ControlWindow::clearSavedSettingsRequested);
    connect(_ui->actionExit, &QAction::triggered, this,
        &ControlWindow::exitRequested);
    connect(_ui->actionCalculate, &QAction::triggered, this,
        &ControlWindow::renderRequested);
    connect(_ui->actionCancelRender, &QAction::triggered, this,
        &ControlWindow::cancelRenderRequested);
    connect(_ui->actionCycleMode, &QAction::triggered, this,
        [this]() { emit navModeChanged(static_cast<NavMode>((_ui->navModeCombo->currentIndex() + 1) % 3)); });
    connect(_ui->actionCycleGrid, &QAction::triggered, this,
        &ControlWindow::cycleGridRequested);
    connect(_ui->actionMinimalUI, &QAction::triggered, this,
        &ControlWindow::toggleViewportOverlayRequested);
    connect(_ui->actionFullscreen, &QAction::triggered, this,
        &ControlWindow::toggleViewportFullscreenRequested);
    connect(_ui->actionHelp, &QAction::triggered, this,
        &ControlWindow::helpRequested);
    connect(_ui->actionAbout, &QAction::triggered, this,
        &ControlWindow::aboutRequested);
}

void ControlWindow::setBackendNames(
    const QStringList &names, const QString &selectedBackend
) {
    const QSignalBlocker blocker(_ui->backendCombo);
    _ui->backendCombo->clear();
    _ui->backendCombo->addItems(names);
    const int index = _ui->backendCombo->findText(selectedBackend);
    _ui->backendCombo->setCurrentIndex(index >= 0 ? index : 0);
}

void ControlWindow::setSineNames(
    const QStringList &names, const QString &currentName, bool dirty
) {
    _sineNames = names;
    _currentSineName = currentName;
    _sineDirty = dirty;
    _refreshNamedCombo(_ui->sineCombo, names, currentName, dirty);
}

void ControlWindow::setPaletteNames(
    const QStringList &names, const QString &currentName, bool dirty
) {
    _paletteNames = names;
    _currentPaletteName = currentName;
    _paletteDirty = dirty;
    _refreshNamedCombo(_ui->paletteCombo, names, currentName, dirty);
}

void ControlWindow::setCpuInfo(const QString &name, int cores, int threads) {
    _ui->cpuNameEdit->setText(name);
    _ui->cpuCoresEdit->setText(cores > 0 ? QString::number(cores) : QString());
    _ui->cpuThreadsEdit->setText(
        threads > 0 ? QString::number(threads) : QString());
}

void ControlWindow::setSessionState(
    const GUISessionState &sessionState,
    NavMode displayedNavMode, SelectionTarget selectionTarget
) {
    const GUIState &state = sessionState.state();
    const QSignalBlocker backendBlocker(_ui->backendCombo);
    const QSignalBlocker iterationsBlocker(_ui->iterationsSpin);
    const QSignalBlocker threadsBlocker(_ui->useThreadsCheckBox);
    const QSignalBlocker juliaBlocker(_ui->juliaCheck);
    const QSignalBlocker inverseBlocker(_ui->inverseCheck);
    const QSignalBlocker aaBlocker(_ui->aaSpin);
    const QSignalBlocker preserveRatioBlocker(_ui->preserveRatioCheck);
    const QSignalBlocker panSliderBlocker(_ui->panRateSlider);
    const QSignalBlocker panSpinBlocker(_ui->panRateSpin);
    const QSignalBlocker zoomSliderBlocker(_ui->zoomRateSlider);
    const QSignalBlocker zoomSpinBlocker(_ui->zoomRateSpin);
    const QSignalBlocker exponentSliderBlocker(_ui->exponentSlider);
    const QSignalBlocker exponentSpinBlocker(_ui->exponentSpin);
    const QSignalBlocker sineComboBlocker(_ui->sineCombo);
    const QSignalBlocker freqRBlocker(_ui->freqRSpin);
    const QSignalBlocker freqGBlocker(_ui->freqGSpin);
    const QSignalBlocker freqBBlocker(_ui->freqBSpin);
    const QSignalBlocker freqMultBlocker(_ui->freqMultSpin);
    const QSignalBlocker paletteComboBlocker(_ui->paletteCombo);
    const QSignalBlocker paletteLengthBlocker(_ui->paletteLengthSpin);
    const QSignalBlocker paletteOffsetBlocker(_ui->paletteOffsetSpin);
    const QSignalBlocker colorBlocker(_ui->colorMethodCombo);
    const QSignalBlocker fractalBlocker(_ui->fractalCombo);
    const QSignalBlocker navModeBlocker(_ui->navModeCombo);
    const QSignalBlocker pickTargetBlocker(_ui->pickTargetCombo);
    const QSignalBlocker widthBlocker(_ui->outputWidthSpin);
    const QSignalBlocker heightBlocker(_ui->outputHeightSpin);
    const QSignalBlocker viewportScaleBlocker(_ui->viewportScaleSpin);

    _ui->backendCombo->setCurrentText(sessionState.state().backend);
    _ui->iterationsSpin->setValue(sessionState.state().iterations);
    _ui->useThreadsCheckBox->setChecked(sessionState.state().useThreads);
    _ui->juliaCheck->setChecked(sessionState.state().julia);
    _ui->inverseCheck->setChecked(sessionState.state().inverse);
    _ui->aaSpin->setValue(sessionState.state().aaPixels);
    _ui->preserveRatioCheck->setChecked(sessionState.state().preserveRatio);
    _ui->panRateSlider->setValue(sessionState.state().panRate);
    _ui->panRateSpin->setValue(sessionState.state().panRate);
    _ui->zoomRateSlider->setValue(sessionState.state().zoomRate);
    _ui->zoomRateSpin->setValue(sessionState.state().zoomRate);
    _ui->exponentSlider->setValue(
        static_cast<int>(std::round(
            std::max(1.01, sessionState.state().exponent) * 100.0))
    );
    Util::setAdaptiveSpinValue(
        _ui->exponentSpin, std::max(1.01, sessionState.state().exponent));
    if (!sessionState.state().sineName.isEmpty()) {
        const int sineIndex
            = _ui->sineCombo->findData(sessionState.state().sineName);
        if (sineIndex >= 0) {
            _ui->sineCombo->setCurrentIndex(sineIndex);
        } else {
            _ui->sineCombo->setCurrentText(sessionState.state().sineName);
        }
    }
    Util::setAdaptiveSpinValue(
        _ui->freqRSpin, sessionState.state().sinePalette.freqR);
    Util::setAdaptiveSpinValue(
        _ui->freqGSpin, sessionState.state().sinePalette.freqG);
    Util::setAdaptiveSpinValue(
        _ui->freqBSpin, sessionState.state().sinePalette.freqB);
    Util::setAdaptiveSpinValue(
        _ui->freqMultSpin, sessionState.state().sinePalette.freqMult);
    if (!sessionState.state().paletteName.isEmpty()) {
        const int paletteIndex
            = _ui->paletteCombo->findData(sessionState.state().paletteName);
        if (paletteIndex >= 0) {
            _ui->paletteCombo->setCurrentIndex(paletteIndex);
        } else {
            _ui->paletteCombo->setCurrentText(sessionState.state().paletteName);
        }
    }
    Util::setAdaptiveSpinValue(
        _ui->paletteLengthSpin, sessionState.state().palette.totalLength);
    Util::setAdaptiveSpinValue(
        _ui->paletteOffsetSpin, sessionState.state().palette.offset);
    _ui->outputWidthSpin->setValue(sessionState.state().outputWidth);
    _ui->outputHeightSpin->setValue(sessionState.state().outputHeight);
    _ui->viewportScaleSpin->setValue(static_cast<double>(std::max(
        1.0f, sessionState.state().viewportScalePercent)));
    if (!_aspectReferenceInitialized || !state.preserveRatio) {
        _lastOutputSize = QSize(state.outputWidth, state.outputHeight);
        _aspectReferenceInitialized = true;
    }
    _ui->colorMethodCombo->setCurrentIndex(
        static_cast<int>(sessionState.state().colorMethod));
    _ui->fractalCombo->setCurrentIndex(
        static_cast<int>(sessionState.state().fractalType));
    _ui->navModeCombo->setCurrentIndex(static_cast<int>(displayedNavMode));
    _ui->pickTargetCombo->setCurrentIndex(static_cast<int>(selectionTarget));

    Util::setCommittedLineEditText(_ui->infoRealEdit, sessionState.pointRealText());
    Util::setCommittedLineEditText(_ui->infoImagEdit, sessionState.pointImagText());
    Util::setCommittedLineEditText(_ui->infoZoomEdit, sessionState.zoomText());
    Util::setCommittedLineEditText(_ui->infoSeedRealEdit, sessionState.seedRealText());
    Util::setCommittedLineEditText(_ui->infoSeedImagEdit, sessionState.seedImagText());
    _ui->lightRealEdit->setText(sessionState.stateToString(state.light.x(), 12));
    _ui->lightImagEdit->setText(sessionState.stateToString(state.light.y(), 12));

    _pointViewDirty = sessionState.isPointViewDirty() && !sessionState.isHomeView();
    _updateWindowTitle();
    _updateModeEnablement(state.colorMethod);
}

void ControlWindow::syncToSessionState(GUISessionState &sessionState) const {
    GUIState &state = sessionState.mutableState();
    state.backend = _ui->backendCombo->currentText();
    state.iterations = _ui->iterationsSpin->value();
    state.useThreads = _ui->useThreadsCheckBox->isChecked();
    state.julia = _ui->juliaCheck->isChecked();
    state.inverse = _ui->inverseCheck->isChecked();
    state.aaPixels = _ui->aaSpin->value();
    state.preserveRatio = _ui->preserveRatioCheck->isChecked();
    state.panRate = _ui->panRateSlider->value();
    state.zoomRate = _ui->zoomRateSlider->value();
    state.exponent = std::max(1.01, _ui->exponentSpin->value());
    state.sinePalette.freqR = static_cast<float>(_ui->freqRSpin->value());
    state.sinePalette.freqG = static_cast<float>(_ui->freqGSpin->value());
    state.sinePalette.freqB = static_cast<float>(_ui->freqBSpin->value());
    state.sinePalette.freqMult = static_cast<float>(_ui->freqMultSpin->value());
    const float paletteTotalLength
        = static_cast<float>(_ui->paletteLengthSpin->value());
    const float paletteOffset
        = static_cast<float>(_ui->paletteOffsetSpin->value());
    const auto paletteStops = PaletteStore::configToStops(state.palette);
    state.palette = PaletteStore::stopsToConfig(
        paletteStops, paletteTotalLength, paletteOffset, state.palette.blendEnds);
    state.outputWidth = _ui->outputWidthSpin->value();
    state.outputHeight = _ui->outputHeightSpin->value();
    state.viewportScalePercent = std::max(
        1.0f, static_cast<float>(_ui->viewportScaleSpin->value()));

    QString sineName = _ui->sineCombo->currentData().toString();
    if (sineName.isEmpty()) {
        sineName = Util::undecoratedLabel(_ui->sineCombo->currentText());
    }
    if (!sineName.isEmpty() && sineName != Util::translatedNewEntryLabel()) {
        state.sineName = sineName;
    }

    QString paletteName = _ui->paletteCombo->currentData().toString();
    if (paletteName.isEmpty()) {
        paletteName = Util::undecoratedLabel(_ui->paletteCombo->currentText());
    }
    if (!paletteName.isEmpty()
        && paletteName != Util::translatedNewEntryLabel()) {
        state.paletteName = paletteName;
    }

    const QString realText = _ui->infoRealEdit->text().trimmed();
    const QString imagText = _ui->infoImagEdit->text().trimmed();
    const QString zoomText = _ui->infoZoomEdit->text().trimmed();
    const QString seedRealText = _ui->infoSeedRealEdit->text().trimmed();
    const QString seedImagText = _ui->infoSeedImagEdit->text().trimmed();

    if (!realText.isEmpty()) sessionState.setPointRealText(realText);
    if (!imagText.isEmpty()) sessionState.setPointImagText(imagText);
    if (!zoomText.isEmpty()) sessionState.setZoomText(zoomText);
    if (!seedRealText.isEmpty()) sessionState.setSeedRealText(seedRealText);
    if (!seedImagText.isEmpty()) sessionState.setSeedImagText(seedImagText);

    sessionState.syncStatePointFromText();
    sessionState.syncStateZoomFromText();
    sessionState.syncStateSeedFromText();

    state.colorMethod
        = static_cast<ColorMethod>(_ui->colorMethodCombo->currentIndex());
    state.fractalType
        = static_cast<FractalType>(_ui->fractalCombo->currentIndex());
}

void ControlWindow::applyShortcuts(const Shortcuts &shortcuts) {
    const auto setShortcut = [&shortcuts](QAction *action, const QString &id) {
        if (action) action->setShortcut(shortcuts.sequence(id));
        };

    setShortcut(_ui->actionCancelRender, "cancel");
    setShortcut(_ui->actionHome, "home");
    setShortcut(_ui->actionCycleMode, "cycleMode");
    setShortcut(_ui->actionCycleGrid, "cycleGrid");
    setShortcut(_ui->actionMinimalUI, "overlay");
    setShortcut(_ui->actionQuickSave, "quickSave");
    setShortcut(_ui->actionFullscreen, "fullscreen");
    setShortcut(_ui->actionCalculate, "calculate");
    setShortcut(_ui->actionSaveImage, "saveImage");
    setShortcut(_ui->actionOpenView, "openView");
    setShortcut(_ui->actionSaveView, "saveView");
    setShortcut(_ui->actionSettings, "settings");
    setShortcut(_ui->actionExit, "exit");
}

void ControlWindow::setIterationAutoResolver(std::function<int()> resolver) {
    _iterationAutoResolver = std::move(resolver);
}

void ControlWindow::restoreWindowSettings(const AppSettings &settings) {
    const QByteArray geometry = settings.controlWindowGeometry();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }
    const QByteArray state = settings.controlWindowState();
    if (!state.isEmpty()) {
        restoreState(state);
    }
    if (!geometry.isEmpty() || !state.isEmpty()) {
        _controlWindowSized = true;
        _windowSettingsRestored = true;
    }
}

void ControlWindow::saveWindowSettings(AppSettings &settings) const {
    settings.setControlWindowGeometry(saveGeometry());
    settings.setControlWindowState(saveState());
}

void ControlWindow::setStatusState(
    const QString &statusText,
    const QString &statusLinkPath, const QString &pixelsPerSecondText,
    const QString &imageMemoryText, int progressValue, bool progressActive,
    bool progressCancelled
) {
    _statusText = statusText;
    _statusLinkPath = statusLinkPath;
    _pixelsPerSecondText = pixelsPerSecondText;
    _imageMemoryText = imageMemoryText;
    _progressValue = progressValue;
    _progressActive = progressActive;
    _progressCancelled = progressCancelled;
    _updateStatusLabels();
}

void ControlWindow::setSinePreviewImage(const QImage &image) {
    auto *preview = qobject_cast<::SinePreviewWidget *>(_ui->sinePreview);
    if (preview) preview->setPreviewImage(image);
}

void ControlWindow::setPalettePreviewPixmap(const QPixmap &pixmap) {
    auto *preview = qobject_cast<::PalettePreviewWidget *>(_ui->palettePreviewLabel);
    if (preview) preview->setPreviewPixmap(pixmap);
}

void ControlWindow::clearPalettePreview() {
    auto *preview = qobject_cast<::PalettePreviewWidget *>(_ui->palettePreviewLabel);
    if (preview) preview->clearPreview();
}

void ControlWindow::setLightColorButton(
    const QColor &color, const QString &text
) {
    _ui->lightColorButton->setText(text);
    _ui->lightColorButton->setStyleSheet(Util::lightColorButtonStyle(color));
}

std::pair<double, double> ControlWindow::sinePreviewRange() const {
    if (auto *preview = qobject_cast<::SinePreviewWidget *>(_ui->sinePreview)) {
        return preview->range();
    }
    return { 0.0, 1.0 };
}

bool ControlWindow::eventFilter(QObject *watched, QEvent *event) {
    if (watched == _ui->paletteCombo && event->type() == QEvent::Wheel) {
        auto *wheelEvent = static_cast<QWheelEvent *>(event);
        if (wheelEvent && wheelEvent->angleDelta().y() > 0
            && _ui->paletteCombo->currentIndex() <= 2) {
            event->accept();
            return true;
        }
    }

    QLineEdit *iterationsEdit = _ui->iterationsSpin->findChild<QLineEdit *>();
    if (iterationsEdit && watched == iterationsEdit && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);

        const QString autoText = _ui->iterationsSpin->specialValueText().trimmed();
        const bool showingAuto = !autoText.isEmpty()
            && iterationsEdit->text().trimmed().compare(autoText, Qt::CaseInsensitive) == 0;
        if (!showingAuto) {
            return QMainWindow::eventFilter(watched, event);
        }

        const bool isBackspaceOrDelete = keyEvent->key() == Qt::Key_Backspace
            || keyEvent->key() == Qt::Key_Delete;
        if (isBackspaceOrDelete) {
            iterationsEdit->clear();
            event->accept();
            return true;
        }

        const bool hasEditBlockingModifier = (keyEvent->modifiers()
            & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))
            != Qt::NoModifier;
        const QString keyText = keyEvent->text();

        const bool isPrintableInsert = !hasEditBlockingModifier
            && keyText.size() == 1 && keyText.at(0).isPrint()
            && !keyText.at(0).isSpace();
        if (isPrintableInsert) iterationsEdit->clear();
    }

    return QMainWindow::eventFilter(watched, event);
}

void ControlWindow::changeEvent(QEvent *event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) {
        _ui->retranslateUi(this);
        _retranslateMenus();
        _retranslateDynamicControls();
        _updateWindowTitle();
        _refreshNamedCombo(
            _ui->sineCombo, _sineNames, _currentSineName, _sineDirty);
        _refreshNamedCombo(
            _ui->paletteCombo, _paletteNames, _currentPaletteName, _paletteDirty);
        _updateStatusLabels();
        _updateControlWindowSize();
    }
}

void ControlWindow::closeEvent(QCloseEvent *event) {
    if (!_closeAllowed) {
        event->ignore();
        const bool skipDirtyViewPrompt
            = (QApplication::keyboardModifiers() & Qt::ShiftModifier) != 0;
        emit closeRequested(skipDirtyViewPrompt);
        return;
    }

    event->accept();
}

void ControlWindow::showEvent(QShowEvent *event) {
    QMainWindow::showEvent(event);

    if (!_controlWindowSized) {
        _updateControlWindowSize();
        _controlWindowSized = true;
    }
}

void ControlWindow::_retranslateMenus() {
    _ui->menuFile->setTitle(tr("File"));
    _ui->menuView->setTitle(tr("View"));
    _ui->menuRender->setTitle(tr("Render"));
    _ui->menuHelp->setTitle(tr("Help"));
    _ui->actionHome->setText(tr("New / Home"));
    _ui->actionOpenView->setText(tr("Open View"));
    _ui->actionSaveView->setText(tr("Save View"));
    _ui->actionSaveImage->setText(tr("Save Image"));
    _ui->actionQuickSave->setText(tr("Quick Save"));
    _ui->actionSettings->setText(tr("Settings"));
    _ui->actionClearSavedSettings->setText(tr("Clear Saved Settings"));
    _ui->actionExit->setText(tr("Exit"));
    _ui->actionCycleMode->setText(tr("Cycle Navigation Mode"));
    _ui->actionCycleGrid->setText(tr("Cycle Grid"));
    _ui->actionMinimalUI->setText(tr("Toggle Viewport Overlay"));
    _ui->actionFullscreen->setText(tr("Toggle Fullscreen"));
    _ui->actionCalculate->setText(tr("Calculate"));
    _ui->actionCancelRender->setText(tr("Cancel Render"));
    _ui->actionHelp->setText(tr("Help"));
    _ui->actionAbout->setText(tr("About"));
}

void ControlWindow::_retranslateDynamicControls() {
    if (_ui->colorMethodCombo->count() >= 4) {
        _ui->colorMethodCombo->setItemText(0, tr("Iterations"));
        _ui->colorMethodCombo->setItemText(1, tr("Smooth Iterations"));
        _ui->colorMethodCombo->setItemText(2, tr("Palette"));
        _ui->colorMethodCombo->setItemText(3, tr("Light"));
    }
    if (_ui->fractalCombo->count() >= 3) {
        _ui->fractalCombo->setItemText(0, tr("Mandelbrot"));
        _ui->fractalCombo->setItemText(1, tr("Perpendicular"));
        _ui->fractalCombo->setItemText(2, tr("Burning Ship"));
    }
    if (_ui->navModeCombo->count() >= 3) {
        _ui->navModeCombo->setItemText(0, tr("Realtime Zoom"));
        _ui->navModeCombo->setItemText(1, tr("Box Zoom"));
        _ui->navModeCombo->setItemText(2, tr("Pan"));
    }
    if (_ui->pickTargetCombo->count() >= 3) {
        _ui->pickTargetCombo->setItemText(0, tr("Zoom Point"));
        _ui->pickTargetCombo->setItemText(1, tr("Seed Point"));
        _ui->pickTargetCombo->setItemText(2, tr("Light Point"));
    }
}

void ControlWindow::_updateModeEnablement(ColorMethod colorMethod) {
    const bool paletteMode = colorMethod == ColorMethod::palette;
    const bool sineMode = colorMethod == ColorMethod::iterations
        || colorMethod == ColorMethod::smooth_iterations;
    const bool lightMode = colorMethod == ColorMethod::light;
    static_cast<::CollapsibleGroupBox *>(_ui->sineGroup)->setContentEnabled(sineMode);
    static_cast<::CollapsibleGroupBox *>(_ui->paletteGroup)
        ->setContentEnabled(paletteMode);
    static_cast<::CollapsibleGroupBox *>(_ui->lightGroup)->setContentEnabled(lightMode);
}

void ControlWindow::_updateControlWindowSize() {
    QLayout *outerLayout = _ui->centralWidget ? _ui->centralWidget->layout() : nullptr;
    if (outerLayout) outerLayout->activate();
    if (_ui->controlScrollContent && _ui->controlScrollContent->layout()) {
        _ui->controlScrollContent->layout()->activate();
        _ui->controlScrollContent->updateGeometry();
    }

    const QSize minimumHint = minimumSizeHint().expandedTo(QSize(1, 1));
    const QSize baseTarget = sizeHint().expandedTo(minimumHint);
    const QMargins outerMargins = outerLayout ? outerLayout->contentsMargins() : QMargins();
    const int outerHorizontalMargins = outerMargins.left() + outerMargins.right();
    const int scrollAreaNonContentWidth = controlScrollBarExtent(_ui->controlScrollArea)
        + (_ui->controlScrollArea ? (_ui->controlScrollArea->frameWidth() * 2) : 0);

    const QSize contentSize = _ui->controlScrollContent->sizeHint().expandedTo(
        _ui->controlScrollContent->minimumSizeHint());
    const int controlContentMinWidth = std::max(
        { contentSize.width(), _ui->controlScrollContent->minimumSizeHint().width(),
            _ui->controlLayout ? _ui->controlLayout->minimumSize().width() : 0,
            _ui->controlScrollContent->width() });
    const int controlContentHeight = contentSize.height();
    const int defaultVisibleContentHeight = std::max(0,
        _ui->viewportGroup->geometry().bottom() + 1
        + _ui->controlScrollContent->contentsMargins().bottom());

    _ui->controlScrollArea->setMinimumWidth(1);

    int fixedPanelsMinWidth = 0;
    int fixedPanelsHeight = 0;
    int visibleWidgets = 0;
    if (outerLayout) {
        const QMargins margins = outerLayout->contentsMargins();
        fixedPanelsHeight = margins.top() + margins.bottom();
        for (int i = 0; i < outerLayout->count(); i++) {
            QLayoutItem *item = outerLayout->itemAt(i);
            QWidget *widget = item ? item->widget() : nullptr;
            if (!widget) continue;

            visibleWidgets++;
            if (widget == _ui->controlScrollArea) continue;
            fixedPanelsMinWidth = std::max(fixedPanelsMinWidth, item->minimumSize().width());
            fixedPanelsMinWidth
                = std::max(fixedPanelsMinWidth, widget->minimumSizeHint().width());
            fixedPanelsHeight += item->sizeHint().height();
        }
        fixedPanelsHeight += std::max(0, visibleWidgets - 1) * outerLayout->spacing();
    }
    const int minimumWidthForFixedPanels
        = std::max(1, fixedPanelsMinWidth + outerHorizontalMargins);
    const int desiredWidth = std::max(
        { baseTarget.width(), minimumHint.width(),
            controlContentMinWidth + outerHorizontalMargins + scrollAreaNonContentWidth,
            minimumWidthForFixedPanels });

    const int minHeight = minimumHint.height();
    setMinimumWidth(minimumWidthForFixedPanels);
    setMaximumWidth(QWIDGETSIZE_MAX);
    setMinimumHeight(minHeight);
    setMaximumHeight(QWIDGETSIZE_MAX);

    if (!_controlWindowSized) {
        const int desiredContentHeight = defaultVisibleContentHeight > 0
            ? defaultVisibleContentHeight
            : controlContentHeight;
        int desiredHeight
            = std::max(minHeight, fixedPanelsHeight + desiredContentHeight);
        if (const QScreen *screen = (windowHandle() && windowHandle()->screen()) ? windowHandle()->screen()
            : (this->screen() ? this->screen() : QApplication::primaryScreen())) {
            const int maxOnScreenHeight = std::max(minHeight,
                screen->availableGeometry().height() - controlWindowScreenPadding
            );
            desiredHeight = std::min(desiredHeight, maxOnScreenHeight);
        }

        resize(desiredWidth, desiredHeight);
    } else if (width() < minimumWidthForFixedPanels) {
        resize(minimumWidthForFixedPanels, height());
    }

    _updateStatusRightEdgeAlignment();
}

void ControlWindow::_updateWindowTitle() {
    setWindowTitle(Util::decorateUnsavedLabel(tr("Control"), _pointViewDirty));
}

void ControlWindow::_updateAspectLinkedSizes(bool widthChanged) {
    if (!_ui->preserveRatioCheck->isChecked()) return;
    const int currentWidth = std::max(1, _ui->outputWidthSpin->value());
    const int currentHeight = std::max(1, _ui->outputHeightSpin->value());
    const int referenceWidth = std::max(1, _lastOutputSize.width());
    const int referenceHeight = std::max(1, _lastOutputSize.height());

    if (widthChanged) {
        const int newHeight = std::max(1,
            static_cast<int>(std::lround(static_cast<double>(currentWidth)
                * referenceHeight / referenceWidth)));
        const QSignalBlocker blocker(_ui->outputHeightSpin);
        _ui->outputHeightSpin->setValue(newHeight);
        return;
    }

    const int newWidth = std::max(1,
        static_cast<int>(std::lround(static_cast<double>(currentHeight)
            * referenceWidth / referenceHeight)));
    const QSignalBlocker blocker(_ui->outputWidthSpin);
    _ui->outputWidthSpin->setValue(newWidth);
}

void ControlWindow::_updateStatusRightEdgeAlignment() {
    constexpr int statusRowLeftShift = 3;
    QWidget *statusPanel = _ui->statusRightLabel->parentWidget();
    if (!statusPanel) return;

    auto *statusRow = qobject_cast<QHBoxLayout *>(statusPanel->layout());
    if (!statusRow) return;

    const QPoint saveTopLeft = _ui->saveButton->mapTo(this, QPoint(0, 0));
    const int targetRight = saveTopLeft.x() + _ui->saveButton->width() - statusRowLeftShift;
    const QPoint panelTopLeft = statusPanel->mapTo(this, QPoint(0, 0));
    const int panelRight = panelTopLeft.x() + statusPanel->width();
    const int rightInset = std::max(0, panelRight - targetRight);
    const int leftInset = -statusRowLeftShift;

    const QMargins margins = statusRow->contentsMargins();
    if (margins.left() == leftInset && margins.right() == rightInset) return;

    statusRow->setContentsMargins(leftInset, margins.top(), rightInset, margins.bottom());
}

void ControlWindow::_updateStatusLabels() {
    const int shownProgressValue
        = (_progressActive || _progressCancelled) ? _progressValue : 0;
    _ui->progressLabel->setText(QStringLiteral("%1%").arg(shownProgressValue));
    _ui->progressLabel->setStyleSheet(
        _progressCancelled ? QStringLiteral("color: rgb(215, 80, 80);") : QString());
    _ui->progressBar->setValue(shownProgressValue);
    _ui->progressBar->setStyleSheet(_progressCancelled
        ? QStringLiteral("QProgressBar::chunk { background-color: rgb(215, 80, 80); }")
        : QString());
    _ui->statusRightLabel->setEmphasisEnabled(_progressCancelled);
    _ui->pixelsPerSecondLabel->setText(
        ((_progressActive
            || _pixelsPerSecondText != Util::defaultPixelsPerSecondText())
            && !_pixelsPerSecondText.isEmpty())
        ? _pixelsPerSecondText
        : QString());
    _ui->imageMemoryLabel->setText(_imageMemoryText);
    _ui->statusRightLabel->setSource(_statusText, _statusLinkPath);
}

void ControlWindow::_refreshNamedCombo(
    QComboBox *combo, const QStringList &names,
    const QString &currentName, bool dirty
) const {
    const QSignalBlocker blocker(combo);
    combo->clear();
    combo->addItem(Util::translatedNewEntryLabel());
    combo->setItemData(0, QBrush(QColor(0, 153, 0)), Qt::ForegroundRole);
    combo->insertSeparator(1);
    QStringList effectiveNames = names;
    if (!currentName.isEmpty()
        && currentName != Util::translatedNewEntryLabel()
        && !effectiveNames.contains(currentName, Qt::CaseInsensitive)) {
        effectiveNames.push_front(currentName);
    }
    for (const QString &name : effectiveNames) {
        const bool currentDirty
            = dirty && name.compare(currentName, Qt::CaseInsensitive) == 0;
        combo->addItem(Util::decorateUnsavedLabel(name, currentDirty), name);
    }

    const int selectedIndex = combo->findData(currentName);
    if (selectedIndex >= 0) {
        combo->setCurrentIndex(selectedIndex);
    } else {
        combo->setCurrentIndex(0);
    }
}
