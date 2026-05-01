#include "app/GUIAppController.h"

#include <filesystem>

#include <QApplication>
#include <QColorDialog>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QInputDialog>
#include <QMessageBox>
#include <QRandomGenerator>
#include <QScreen>
#include <QWindow>

#include "CPUInfo.h"
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
#include "util/FormatUtil.h"
#include "util/PathUtil.h"

GUIAppController::GUIAppController(
    GUILocale& locale, AppSettings& settings, QObject* parent)
    : QObject(parent)
    , _locale(locale)
    , _settings(settings)
    , _shortcuts(settings)
    , _sessionState(this)
    , _renderController(this)
    , _viewportController(
          _sessionState, _renderController, _shortcuts, this)
    , _controlWindow(std::make_unique<ControlWindow>())
    , _viewportWindow(std::make_unique<ViewportWindow>(&_viewportController)) {
    connect(qApp, &QCoreApplication::aboutToQuit, this,
        &GUIAppController::_savePersistentState);
    _viewportController.attachViewport(_viewportWindow.get());
    _renderController.setPreviewDevicePixelRatio(
        _viewportWindow ? _viewportWindow->devicePixelRatioF() : 1.0);
}

GUIAppController::~GUIAppController() = default;

bool GUIAppController::initialize() {
    QString backendError;
    _backendNames = GUI::BackendCatalog::listNames(backendError);
    if (_backendNames.isEmpty()) {
        QMessageBox::critical(nullptr, tr("Backend"), backendError);
        return false;
    }
    _renderController.setAvailableBackends(_backendNames);

    _connectUI();
    _controlWindow->setIterationAutoResolver([this]() {
        const int currentIterations = _renderController.currentIterationCount();
        return currentIterations > 0 ? currentIterations
                                     : std::max(1, _sessionState.state().iterations);
    });
    _initializeSessionState();
    _controlWindow->restoreWindowSettings(_settings);
    _refreshAllViews();

    if (!_loadSelectedBackend()) {
        return false;
    }
    _requestRenderFromModel();
    return true;
}

void GUIAppController::show() {
    _controlWindow->show();
    _viewportWindow->show();
    GUI::Util::resizeViewportToImageSize(
        _viewportWindow.get(), _sessionState.outputSize());
    if (!_controlWindow->windowSettingsRestored()) {
        _positionWindowsForInitialShow();
    }
    QTimer::singleShot(0, this, [this]() {
        if (!_viewportWindow || !_viewportWindow->isVisible()) return;
        _renderController.setPreviewDevicePixelRatio(
            _viewportWindow->devicePixelRatioF());
        _viewportWindow->raise();
        _viewportWindow->activateWindow();
        _viewportWindow->setFocus(Qt::ActiveWindowFocusReason);
    });
}

void GUIAppController::_connectUI() {
    connect(_controlWindow.get(), &ControlWindow::backendSelectionRequested, this,
        [this](const QString& backendName) {
            _sessionState.mutableState().backend = backendName;
            _sessionState.mutableState().manualBackendSelection = true;
            _loadSelectedBackend();
            _refreshControlState();
            _requestRenderFromModel();
        });
    connect(_controlWindow.get(), &ControlWindow::renderRequested, this,
        &GUIAppController::_requestRenderFromControls);
    connect(_controlWindow.get(), &ControlWindow::threadUsageChanged, this,
        [this](bool checked) {
            _sessionState.mutableState().useThreads = checked;
            _renderController.freezePreview();
            _refreshControlState();
            _refreshStatus();
        });
    connect(_controlWindow.get(), &ControlWindow::previewRefreshRequested, this,
        [this]() { _refreshPreviews(); });
    connect(_controlWindow.get(), &ControlWindow::homeRequested, this,
        [this]() { _viewportController.applyHomeView(); });
    connect(_controlWindow.get(), &ControlWindow::zoomCenterRequested, this,
        [this]() {
            const QSize size = _sessionState.outputSize();
            _viewportController.zoomAtPixel(
                QPoint(size.width() / 2, size.height() / 2), true);
        });
    connect(_controlWindow.get(), &ControlWindow::saveImageRequested, this,
        &GUIAppController::_saveImage);
    connect(_controlWindow.get(), &ControlWindow::quickSaveRequested, this,
        &GUIAppController::_quickSaveImage);
    connect(_controlWindow.get(), &ControlWindow::viewportResizeRequested, this,
        [this]() {
            _controlWindow->syncToSessionState(_sessionState);
            _refreshControlState();
            GUI::Util::resizeViewportToImageSize(
                _viewportWindow.get(), _sessionState.outputSize());
            _requestRenderFromModel();
        });
    connect(_controlWindow.get(), &ControlWindow::saveViewRequested, this,
        &GUIAppController::_savePointView);
    connect(_controlWindow.get(), &ControlWindow::loadViewRequested, this,
        &GUIAppController::_loadPointView);
    connect(_controlWindow.get(), &ControlWindow::settingsRequested, this,
        &GUIAppController::_showSettingsDialog);
    connect(_controlWindow.get(), &ControlWindow::clearSavedSettingsRequested, this,
        &GUIAppController::_clearSavedSettings);
    connect(_controlWindow.get(), &ControlWindow::helpRequested, this,
        &GUIAppController::_showHelpDialog);
    connect(_controlWindow.get(), &ControlWindow::aboutRequested, this,
        &GUIAppController::_showAboutDialog);
    connect(_controlWindow.get(), &ControlWindow::exitRequested, this,
        &GUIAppController::_closeAllWindows);
    connect(_controlWindow.get(), &ControlWindow::cancelRenderRequested, this,
        [this]() { _viewportController.cancelQueuedRenders(); _refreshStatus(); });
    connect(_controlWindow.get(), &ControlWindow::cycleGridRequested,
        &_viewportController, &ViewportController::cycleGridMode);
    connect(_controlWindow.get(), &ControlWindow::toggleViewportOverlayRequested,
        &_viewportController, &ViewportController::toggleMinimalUI);
    connect(_controlWindow.get(), &ControlWindow::toggleViewportFullscreenRequested,
        &_viewportController, &ViewportController::toggleFullscreen);
    connect(_controlWindow.get(), &ControlWindow::navModeChanged, this,
        [this](NavMode mode) {
            _viewportController.setNavMode(mode);
            _refreshControlState();
        });
    connect(_controlWindow.get(), &ControlWindow::selectionTargetChanged, this,
        [this](SelectionTarget target) {
            _viewportController.setSelectionTarget(target);
            _refreshControlState();
        });
    connect(_controlWindow.get(), &ControlWindow::sineSelectionRequested, this,
        [this](const QString& name) {
            if (_confirmDiscardDirtySine()) {
                _loadSineByName(name, true);
            } else {
                _refreshControlState();
            }
        });
    connect(_controlWindow.get(), &ControlWindow::newSineRequested, this,
        [this]() { _createNewSinePalette(true); });
    connect(_controlWindow.get(), &ControlWindow::randomizeSineRequested, this,
        &GUIAppController::_randomizeSinePalette);
    connect(_controlWindow.get(), &ControlWindow::saveSineRequested, this,
        &GUIAppController::_saveSine);
    connect(_controlWindow.get(), &ControlWindow::importSineRequested, this,
        &GUIAppController::_importSine);
    connect(_controlWindow.get(), &ControlWindow::paletteSelectionRequested, this,
        [this](const QString& name) {
            if (_confirmDiscardDirtyPalette()) {
                _loadPaletteByName(name, true);
            } else {
                _refreshControlState();
            }
        });
    connect(_controlWindow.get(), &ControlWindow::newPaletteRequested, this,
        [this]() { _createNewColorPalette(true); });
    connect(_controlWindow.get(), &ControlWindow::loadPaletteRequested, this,
        &GUIAppController::_loadPalette);
    connect(_controlWindow.get(), &ControlWindow::savePaletteRequested, this,
        &GUIAppController::_savePalette);
    connect(_controlWindow.get(), &ControlWindow::paletteEditorRequested, this,
        &GUIAppController::_openPaletteEditor);
    connect(_controlWindow.get(), &ControlWindow::lightColorChangeRequested, this,
        &GUIAppController::_changeLightColor);

    connect(&_viewportController, &ViewportController::sessionStateChanged, this,
        &GUIAppController::_refreshControlState);
    connect(&_viewportController, &ViewportController::renderRequested, this,
        &GUIAppController::_requestRenderFromModel);
    connect(&_viewportController, &ViewportController::renderRequestedWithPickAction,
        this, [this](int pickTarget, const QPoint& pixel) {
            PendingPickAction pickAction;
            pickAction.target = static_cast<SelectionTarget>(pickTarget);
            pickAction.pixel = pixel;
            _requestRender(pickAction);
        });
    connect(&_viewportController, &ViewportController::quickSaveRequested, this,
        &GUIAppController::_quickSaveImage);
    connect(&_viewportController, &ViewportController::statusMessageChanged, this,
        &GUIAppController::_setStatusMessage);

    connect(&_renderController, &RenderController::previewImageChanged, this,
        [this]() { _viewportWindow->update(); });
    connect(&_renderController, &RenderController::renderStateChanged, this,
        [this]() {
            _refreshStatus();
            _viewportWindow->update();
        });
    connect(&_renderController, &RenderController::renderFailed, this,
        [this](const QString& message) {
            QMessageBox::warning(_controlWindow.get(), tr("Render"), message);
        });
    connect(&_renderController, &RenderController::automaticBackendSwitchRequested,
        this, [this](const QString& backendName, bool hasPickAction, int pickTarget,
                  const QPoint& pickPixel) {
            if (backendName.isEmpty()
                || backendName == _sessionState.state().backend) {
                return;
            }

            _sessionState.mutableState().backend = backendName;
            _refreshControlState();
            if (!_loadSelectedBackend()) {
                return;
            }

            _requestRender(
                _pendingPickAction(hasPickAction, pickTarget, pickPixel));
        });
}

void GUIAppController::_requestRenderFromControls() {
    _controlWindow->syncToSessionState(_sessionState);
    _refreshControlState();
    _refreshPreviews();
    _requestRenderFromModel();
}

void GUIAppController::_requestRenderFromModel() {
    _requestRender();
}

void GUIAppController::_requestRender(
    const std::optional<PendingPickAction>& pickAction) {
    _sessionState.mutableState().zoom = GUI::Util::clampGUIZoom(
        _sessionState.state().zoom);
    _statusText.clear();
    _statusLinkPath.clear();
    if (_viewportWindow) {
        _renderController.setPreviewDevicePixelRatio(
            _viewportWindow->devicePixelRatioF());
    }
    _renderController.requestRender(_sessionState.snapshot(), pickAction);
    _refreshStatus();
}

void GUIAppController::_clearSavedSettings() {
    const QMessageBox::StandardButton choice = QMessageBox::question(
        _controlWindow.get(), tr("Clear Saved Settings"),
        tr("This will remove the persisted GUI settings and close the application. Continue?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (choice != QMessageBox::Yes) {
        return;
    }

    _skipSaveSettings = true;
    _settings.clearPersistedSettings();
    _closeAllWindows();
}

void GUIAppController::_refreshAllViews() {
    _refreshCollections();
    _refreshControlState();
    _refreshStatus();
    _refreshPreviews();
}

void GUIAppController::_refreshControlState() {
    _controlWindow->setBackendNames(_backendNames, _sessionState.state().backend);
    _controlWindow->setSessionState(_sessionState,
        _viewportController.displayedNavMode(),
        _viewportController.selectionTarget());
    _controlWindow->applyShortcuts(_shortcuts);
    const QColor lightColor
        = GUI::Util::lightColorToQColor(_sessionState.state().lightColor);
    _controlWindow->setLightColorButton(lightColor,
        QString::fromStdString(FormatUtil::formatHexColor(
            _sessionState.state().lightColor.R, _sessionState.state().lightColor.G,
            _sessionState.state().lightColor.B)));
}

void GUIAppController::_refreshStatus() {
    const QString statusText = !_statusText.isEmpty()
        ? _statusText
        : _renderController.statusMessage();
    const QString pixelsText
        = _renderController.pixelsPerSecondText().isEmpty()
        ? GUI::Util::defaultPixelsPerSecondText()
        : _renderController.pixelsPerSecondText();
    _controlWindow->setStatusState(statusText, _statusLinkPath, pixelsText,
        _renderController.imageMemoryText(), _renderController.progressValue(),
        _renderController.progressActive(), _renderController.progressCancelled());
}

void GUIAppController::_refreshPreviews() {
    _controlWindow->setSinePreviewImage(_renderController.renderSinePreview(
        _sessionState.state().sinePalette,
        _controlWindow->findChild<QWidget*>("sinePreview")->size(),
        _controlWindow->sinePreviewRange().first,
        _controlWindow->sinePreviewRange().second));

    const QImage paletteImage
        = _renderController.renderPalettePreview(_sessionState.state().palette);
    if (paletteImage.isNull()) {
        _controlWindow->clearPalettePreview();
    } else {
        _controlWindow->setPalettePreviewPixmap(QPixmap::fromImage(paletteImage));
    }
}

void GUIAppController::_refreshCollections() {
    _controlWindow->setSineNames(
        GUI::SineStore::listNames(), _sessionState.state().sineName,
        _sessionState.isSineDirty());
    _controlWindow->setPaletteNames(
        GUI::PaletteStore::listNames(), _sessionState.state().paletteName,
        _sessionState.isPaletteDirty());
}

void GUIAppController::_setStatusMessage(const QString& message) {
    _statusText = message;
    _statusLinkPath.clear();
    _refreshStatus();
}

void GUIAppController::_setStatusSavedPath(const QString& path) {
    const QString absolutePath = QFileInfo(path).absoluteFilePath();
    _statusText = tr("Saved: %1").arg(QDir::toNativeSeparators(absolutePath));
    _statusLinkPath = absolutePath;
    _refreshStatus();
}

bool GUIAppController::_loadSelectedBackend() {
    QString errorMessage;
    if (_renderController.loadBackend(_sessionState.state().backend, errorMessage)) {
        _setStatusMessage(tr("Ready"));
        _refreshPreviews();
        return true;
    }

    QMessageBox::warning(_controlWindow.get(), tr("Backend"), errorMessage);
    return false;
}

std::optional<PendingPickAction> GUIAppController::_pendingPickAction(
    bool hasPickAction, int pickTarget, const QPoint& pickPixel) const {
    if (!hasPickAction) {
        return std::nullopt;
    }

    PendingPickAction action;
    action.target = static_cast<SelectionTarget>(pickTarget);
    action.pixel = pickPixel;
    return action;
}

void GUIAppController::_initializeSessionState() {
    GUIState& state = _sessionState.mutableState();
    state.backend = _defaultBackend();
    state.iterations = 0;
    state.interactionTargetFPS = _settings.previewFallbackFPS();
    state.outputWidth = _settings.selectedOutputWidth();
    state.outputHeight = _settings.selectedOutputHeight();
    state.sineName = GUI::SineStore::defaultName;
    state.paletteName = GUI::PaletteStore::defaultName;

    QString sineError;
    if (!_loadSineByName(state.sineName, false, &sineError)) {
        _createNewSinePalette(false);
    }
    QString paletteError;
    if (!_loadPaletteByName(state.paletteName, false, &paletteError)) {
        _createNewColorPalette(false);
    }

    const CPUInfo cpu = queryCPUInfo();
    _controlWindow->setCpuInfo(
        QString::fromStdString(cpu.name), cpu.cores, cpu.threads);
    _sessionState.markPointViewSavedState();
}

void GUIAppController::_positionWindowsForInitialShow() {
    const QScreen* screen
        = (_viewportWindow->windowHandle() && _viewportWindow->windowHandle()->screen())
        ? _viewportWindow->windowHandle()->screen()
        : ((_controlWindow->windowHandle() && _controlWindow->windowHandle()->screen())
                  ? _controlWindow->windowHandle()->screen()
                  : QApplication::primaryScreen());
    if (!screen) return;

    const QRect available = screen->availableGeometry();
    const QSize controlFrameSize = _controlWindow->frameGeometry().size();
    const QSize viewportFrameSize = _viewportWindow->frameGeometry().size();
    const int gap = 8;

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
    }

    _controlWindow->move(controlX, controlY);
    _viewportWindow->move(viewportX, viewportY);
}

QString GUIAppController::_defaultBackend() const {
    QString backend = _settings.preferredBackend();
    if (!backend.isEmpty() && _backendNames.contains(backend)) {
        return backend;
    }

    const QString preferredType
        = QString::fromUtf8(GUI::Constants::defaultBackendType);
    for (const QString& name : _backendNames) {
        if (name.compare(preferredType, Qt::CaseInsensitive) == 0
            || name.endsWith(QStringLiteral(" - %1").arg(preferredType),
                Qt::CaseInsensitive)) {
            return name;
        }
    }
    return _backendNames.isEmpty() ? QString() : _backendNames.front();
}

void GUIAppController::_showSettingsDialog() {
    SettingsDialog dialog(_settings.language(),
        _sessionState.state().interactionTargetFPS, _shortcuts, _controlWindow.get());
    if (dialog.exec() != QDialog::Accepted) return;

    Shortcuts edited = dialog.shortcuts(_settings);
    const QString duplicateError = edited.duplicateError();
    if (!duplicateError.isEmpty()) {
        QMessageBox::warning(_controlWindow.get(), tr("Settings"), duplicateError);
        return;
    }

    _settings.setLanguage(dialog.language());
    _sessionState.mutableState().interactionTargetFPS = dialog.previewFallbackFPS();
    _settings.setPreviewFallbackFPS(_sessionState.state().interactionTargetFPS);
    if (!_locale.setLanguage(dialog.language())) {
        QMessageBox::warning(_controlWindow.get(), tr("Settings"),
            tr("The selected language could not be loaded."));
    }
    edited.save();
    _shortcuts.reload();
    _refreshAllViews();
}

void GUIAppController::_showHelpDialog() {
    HelpDialog dialog(_controlWindow.get());
    dialog.exec();
}

void GUIAppController::_showAboutDialog() {
    AboutDialog dialog(_controlWindow->windowIcon(), _controlWindow.get());
    dialog.exec();
}

void GUIAppController::_saveImage() {
    auto result = GUI::runSaveImageDialog(_controlWindow.get(), "mandelbrot.png");
    if (!result) return;

    QString errorMessage;
    if (!_renderController.saveImage(
            result->path, result->appendDate, result->type, errorMessage)) {
        QMessageBox::warning(_controlWindow.get(), tr("Save"), errorMessage);
        return;
    }

    _setStatusSavedPath(result->path);
}

void GUIAppController::_quickSaveImage() {
    std::error_code ec;
    std::filesystem::create_directories(GUI::Util::savesDirectoryPath(), ec);
    const QString path = QString::fromStdWString(
        (GUI::Util::savesDirectoryPath() / "mandelbrot.png").wstring());
    QString errorMessage;
    if (!_renderController.saveImage(path, true, "png", errorMessage)) {
        QMessageBox::warning(_controlWindow.get(), tr("Save"), errorMessage);
        return;
    }

    _setStatusSavedPath(path);
}

void GUIAppController::_changeLightColor() {
    const QColor initial
        = GUI::Util::lightColorToQColor(_sessionState.state().lightColor);
    const QColor color = QColorDialog::getColor(initial, _controlWindow.get(), tr("Color"));
    if (!color.isValid()) return;

    _sessionState.mutableState().lightColor = GUI::Util::lightColorFromQColor(color);
    _refreshControlState();
    _requestRenderFromModel();
}

void GUIAppController::_randomizeSinePalette() {
    auto randomFrequency = []() {
        const double minValue = 0.5;
        const double maxValue = 1.5;
        const double value = minValue
            + (maxValue - minValue)
                * QRandomGenerator::global()->generateDouble();
        return static_cast<float>(std::round(value * 10000.0) / 10000.0);
    };

    GUIState& state = _sessionState.mutableState();
    state.sinePalette.freqR = randomFrequency();
    state.sinePalette.freqG = randomFrequency();
    state.sinePalette.freqB = randomFrequency();
    _refreshControlState();
    _refreshPreviews();
    _requestRenderFromModel();
}

void GUIAppController::_savePointView() {
    _controlWindow->syncToSessionState(_sessionState);
    _sessionState.mutableState().zoom = GUI::Util::clampGUIZoom(_sessionState.state().zoom);

    QString errorMessage;
    if (!GUI::PointStore::ensureDirectory(errorMessage)) {
        QMessageBox::warning(_controlWindow.get(), tr("Save View"), errorMessage);
        return;
    }

    const QString defaultPath = GUI::Util::uniqueIndexedPathWithExtension(
        GUI::PointStore::directoryPath(), tr("View"), "txt");
    const QString selectedPath = GUI::showNativeSaveFileDialog(_controlWindow.get(),
        tr("Save View"), GUI::PointStore::directoryPath(),
        QFileInfo(defaultPath).fileName(), tr("View Files (*.txt);;All Files (*.*)"));
    if (selectedPath.isEmpty()) return;

    PointConfig point { .fractal = GUI::Util::fractalTypeToConfigString(
                            _sessionState.state().fractalType)
                            .toStdString(),
        .inverse = _sessionState.state().inverse,
        .julia = _sessionState.state().julia,
        .iterations = _sessionState.state().iterations,
        .real = _sessionState.pointRealText().toStdString(),
        .imag = _sessionState.pointImagText().toStdString(),
        .zoom = _sessionState.zoomText().toStdString(),
        .seedReal = _sessionState.seedRealText().toStdString(),
        .seedImag = _sessionState.seedImagText().toStdString() };

    const QString outputPathWithExtension = QString::fromStdString(
        PathUtil::appendExtension(selectedPath.toStdString(), "txt"));
    if (!GUI::PointStore::saveToPath(
            outputPathWithExtension.toStdString(), point, errorMessage)) {
        QMessageBox::warning(_controlWindow.get(), tr("Save View"), errorMessage);
        return;
    }

    _sessionState.markPointViewSavedState();
    _setStatusSavedPath(outputPathWithExtension);
}

void GUIAppController::_loadPointView() {
    QString errorMessage;
    if (!GUI::PointStore::ensureDirectory(errorMessage)) {
        QMessageBox::warning(_controlWindow.get(), tr("Load View"), errorMessage);
        return;
    }

    const QString selectedPath = GUI::showNativeOpenFileDialog(_controlWindow.get(),
        tr("Load View"), GUI::PointStore::directoryPath(),
        tr("View Files (*.txt);;All Files (*.*)"));
    if (selectedPath.isEmpty()) return;

    PointConfig point;
    if (!GUI::PointStore::loadFromPath(
            selectedPath.toStdString(), point, errorMessage)) {
        QMessageBox::warning(_controlWindow.get(), tr("Load View"), errorMessage);
        return;
    }

    GUIState& state = _sessionState.mutableState();
    state.fractalType = GUI::Util::fractalTypeFromConfigString(
        QString::fromStdString(point.fractal));
    state.inverse = point.inverse;
    state.julia = point.julia;
    state.iterations = std::max(0, point.iterations);
    _sessionState.setZoomText(QString::fromStdString(point.zoom));
    _sessionState.setPointTexts(
        QString::fromStdString(point.real), QString::fromStdString(point.imag));
    _sessionState.setSeedTexts(
        QString::fromStdString(point.seedReal), QString::fromStdString(point.seedImag));
    _sessionState.syncStateZoomFromText();
    _sessionState.syncStatePointFromText();
    _sessionState.syncStateSeedFromText();
    _sessionState.markPointViewSavedState();
    _refreshControlState();
    _requestRenderFromModel();
}

void GUIAppController::_importSine() {
    const QString sourcePath = GUI::showNativeOpenFileDialog(_controlWindow.get(),
        tr("Import Sine Color"), GUI::SineStore::directoryPath(),
        tr("Sine Files (*.txt);;All Files (*.*)"));
    if (sourcePath.isEmpty()) return;

    Backend::SinePaletteConfig loaded;
    QString errorMessage;
    if (!GUI::SineStore::loadFromPath(
            sourcePath.toStdString(), loaded, errorMessage)) {
        QMessageBox::warning(_controlWindow.get(), tr("Sine Color"), errorMessage);
        return;
    }

    if (!GUI::SineStore::ensureDirectory(errorMessage)) {
        QMessageBox::warning(_controlWindow.get(), tr("Sine Color"), errorMessage);
        return;
    }

    const QStringList existingNames = GUI::SineStore::listNames();
    const QString importedName = GUI::PaletteStore::uniqueName(
        QFileInfo(sourcePath).completeBaseName(), existingNames);
    const std::filesystem::path destinationPath = GUI::SineStore::filePath(importedName);
    if (!GUI::SineStore::saveToPath(destinationPath, loaded, errorMessage)) {
        QMessageBox::warning(_controlWindow.get(), tr("Sine Color"), errorMessage);
        return;
    }

    _sessionState.mutableState().sineName = importedName;
    _sessionState.mutableState().sinePalette = loaded;
    _sessionState.markSineSavedState();
    _setStatusSavedPath(QString::fromStdWString(destinationPath.wstring()));
    _refreshAllViews();
    _requestRenderFromModel();
}

void GUIAppController::_saveSine() {
    _controlWindow->syncToSessionState(_sessionState);

    QString targetName = GUI::PaletteStore::normalizeName(_sessionState.state().sineName);
    if (!GUI::PaletteStore::isValidName(targetName)
        || targetName == GUI::Util::translatedNewEntryLabel()) {
        const QString suggested = GUI::PaletteStore::sanitizeName(
            _sessionState.state().sineName);
        bool accepted = false;
        const QString enteredName = QInputDialog::getText(_controlWindow.get(),
            tr("Save Sine Color"), tr("Name"), QLineEdit::Normal,
            suggested.isEmpty() ? tr("sine") : suggested, &accepted);
        if (!accepted) return;

        targetName = GUI::PaletteStore::normalizeName(enteredName);
        if (!GUI::PaletteStore::isValidName(targetName)) {
            QMessageBox::warning(_controlWindow.get(), tr("Save Sine Color"),
                tr("Use an ASCII name with letters, numbers, spaces, ., _, or -."));
            return;
        }
    }

    QString errorMessage;
    if (!GUI::SineStore::ensureDirectory(errorMessage)) {
        QMessageBox::warning(_controlWindow.get(), tr("Save Sine Color"), errorMessage);
        return;
    }

    const std::filesystem::path destinationPath = GUI::SineStore::filePath(targetName);
    if (!GUI::SineStore::saveToPath(
            destinationPath, _sessionState.state().sinePalette, errorMessage)) {
        QMessageBox::warning(_controlWindow.get(), tr("Save Sine Color"), errorMessage);
        return;
    }

    _sessionState.mutableState().sineName = targetName;
    _sessionState.markSineSavedState();
    _setStatusSavedPath(QString::fromStdWString(destinationPath.wstring()));
    _refreshAllViews();
}

void GUIAppController::_loadPalette() {
    const QString sourcePath = GUI::showNativeOpenFileDialog(_controlWindow.get(),
        tr("Load Palette"), GUI::PaletteStore::directoryPath(),
        tr("Palette Files (*.txt);;All Files (*.*)"));
    if (sourcePath.isEmpty()) return;

    Backend::PaletteHexConfig loaded;
    QString importedName;
    std::filesystem::path destinationPath;
    QString errorMessage;
    if (!GUI::PaletteStore::importFromPath(sourcePath.toStdString(),
            _sessionState.state().palette.totalLength,
            _sessionState.state().palette.offset, importedName, loaded,
            destinationPath, errorMessage)) {
        QMessageBox::warning(_controlWindow.get(), tr("Palette"), errorMessage);
        return;
    }

    _sessionState.mutableState().paletteName = importedName;
    _sessionState.mutableState().palette = loaded;
    _sessionState.markPaletteSavedState();
    _setStatusSavedPath(QString::fromStdWString(destinationPath.wstring()));
    _refreshAllViews();
    _requestRenderFromModel();
}

void GUIAppController::_savePalette() {
    _controlWindow->syncToSessionState(_sessionState);

    QString targetName = GUI::PaletteStore::normalizeName(
        _sessionState.state().paletteName);
    if (!GUI::PaletteStore::isValidName(targetName)
        || targetName == GUI::Util::translatedNewEntryLabel()) {
        const QString suggested = GUI::PaletteStore::sanitizeName(
            _sessionState.state().paletteName);
        bool accepted = false;
        const QString enteredName = QInputDialog::getText(_controlWindow.get(),
            tr("Save Palette"), tr("Name"), QLineEdit::Normal,
            suggested.isEmpty() ? tr("palette") : suggested, &accepted);
        if (!accepted) return;

        targetName = GUI::PaletteStore::normalizeName(enteredName);
        if (!GUI::PaletteStore::isValidName(targetName)) {
            QMessageBox::warning(_controlWindow.get(), tr("Save Palette"),
                tr("Use an ASCII name with letters, numbers, spaces, ., _, or -."));
            return;
        }
    }

    QString errorMessage;
    std::filesystem::path destinationPath;
    if (!GUI::PaletteStore::saveNamed(
            targetName, _sessionState.state().palette, destinationPath, errorMessage)) {
        QMessageBox::warning(_controlWindow.get(), tr("Save Palette"), errorMessage);
        return;
    }

    _sessionState.mutableState().paletteName = targetName;
    _sessionState.markPaletteSavedState();
    _setStatusSavedPath(QString::fromStdWString(destinationPath.wstring()));
    _refreshAllViews();
}

void GUIAppController::_openPaletteEditor() {
    const Backend::PaletteHexConfig original = _sessionState.state().palette;
    const QString originalName = _sessionState.state().paletteName;
    PaletteDialog dialog(_sessionState.state().palette, originalName,
        [this](const QString& path) { _setStatusSavedPath(path); }, _controlWindow.get());

    if (dialog.exec() == QDialog::Accepted) {
        _sessionState.mutableState().palette = dialog.palette();
        _sessionState.mutableState().palette.totalLength
            = static_cast<float>(_sessionState.state().palette.totalLength);
        _sessionState.mutableState().palette.offset
            = static_cast<float>(_sessionState.state().palette.offset);
        if (!dialog.savedPaletteName().isEmpty() && !dialog.savedStateDirty()) {
            _sessionState.mutableState().paletteName = dialog.savedPaletteName();
            _sessionState.markPaletteSavedState();
        }
        _refreshAllViews();
        _requestRenderFromModel();
    } else {
        _sessionState.mutableState().palette = original;
        _sessionState.mutableState().paletteName = originalName;
        _refreshAllViews();
    }
}

void GUIAppController::_createNewSinePalette(bool requestRenderOnSuccess) {
    const QStringList existingNames = GUI::SineStore::listNames();
    _sessionState.mutableState().sineName = GUI::Util::uniqueIndexedNameFromList(
        tr("New Sine"), existingNames);
    _sessionState.mutableState().sinePalette = GUI::SineStore::makeNewConfig();
    _refreshAllViews();
    if (requestRenderOnSuccess) {
        _requestRenderFromModel();
    }
}

void GUIAppController::_createNewColorPalette(bool requestRenderOnSuccess) {
    const QStringList existingNames = GUI::PaletteStore::listNames();
    _sessionState.mutableState().paletteName = GUI::Util::uniqueIndexedNameFromList(
        tr("New Palette"), existingNames);
    _sessionState.mutableState().palette = GUI::PaletteStore::makeNewConfig();
    _refreshAllViews();
    if (requestRenderOnSuccess) {
        _requestRenderFromModel();
    }
}

bool GUIAppController::_loadSineByName(
    const QString& name, bool requestRenderOnSuccess, QString* errorMessage) {
    if (errorMessage) errorMessage->clear();
    const QString normalizedName = GUI::PaletteStore::normalizeName(name);
    if (normalizedName.isEmpty()) return false;

    Backend::SinePaletteConfig loaded;
    const std::filesystem::path sourcePath = GUI::SineStore::filePath(normalizedName);
    QString localError;
    if (std::filesystem::exists(sourcePath)) {
        if (!GUI::SineStore::loadFromPath(sourcePath, loaded, localError)) {
            if (errorMessage) *errorMessage = localError;
            return false;
        }
    } else {
        localError = tr("Sine file not found: %1").arg(normalizedName);
        if (errorMessage) *errorMessage = localError;
        return false;
    }

    _sessionState.mutableState().sineName = normalizedName;
    _sessionState.mutableState().sinePalette = loaded;
    _sessionState.markSineSavedState();
    _refreshAllViews();
    if (requestRenderOnSuccess) {
        _requestRenderFromModel();
    }
    return true;
}

bool GUIAppController::_loadPaletteByName(
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

    _sessionState.mutableState().paletteName = normalizedName;
    _sessionState.mutableState().palette = loaded;
    _sessionState.markPaletteSavedState();
    _refreshAllViews();
    if (requestRenderOnSuccess) {
        _requestRenderFromModel();
    }
    return true;
}

bool GUIAppController::_confirmDiscardDirtySine() {
    if (!_sessionState.isSineDirty()) return true;

    const QMessageBox::StandardButton choice = QMessageBox::question(
        _controlWindow.get(), tr("Sine Color"),
        tr("Current sine palette has unsaved changes. Discard them?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    return choice == QMessageBox::Yes;
}

bool GUIAppController::_confirmDiscardDirtyPalette() {
    if (!_sessionState.isPaletteDirty()) return true;

    const QMessageBox::StandardButton choice = QMessageBox::question(
        _controlWindow.get(), tr("Palette"),
        tr("Current palette has unsaved changes. Discard them?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    return choice == QMessageBox::Yes;
}

void GUIAppController::_savePersistentState() {
    if (_quitPrepared) {
        return;
    }
    _quitPrepared = true;

    if (!_skipSaveSettings && _controlWindow) {
        _controlWindow->syncToSessionState(_sessionState);
        _controlWindow->saveWindowSettings(_settings);
        _settings.setPreferredBackend(_sessionState.state().backend);
        _settings.setPreviewFallbackFPS(
            _sessionState.state().interactionTargetFPS);
        _settings.setSelectedOutputWidth(_sessionState.state().outputWidth);
        _settings.setSelectedOutputHeight(_sessionState.state().outputHeight);
        _settings.sync();
    }

    _renderController.cancelQueuedRenders();
    _renderController.shutdown(true, 0);
}

void GUIAppController::_closeAllWindows() {
    if (_closingWindows) return;
    _closingWindows = true;

    if (_viewportWindow && _viewportWindow->isVisible()) {
        _viewportWindow->close();
    }
    if (_controlWindow && _controlWindow->isVisible()) {
        _controlWindow->close();
    }

    QMetaObject::invokeMethod(
        qApp, &QCoreApplication::quit, Qt::QueuedConnection);
}
