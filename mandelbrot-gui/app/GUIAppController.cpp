#include "GUIAppController.h"

#include <filesystem>
#include <system_error>

#include <QApplication>
#include <QColorDialog>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QInputDialog>
#include <QMessageBox>
#include <QPushButton>
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
#include "util/FileUtil.h"

#include "BackendAPI.h"
using namespace Backend;

using namespace GUI;

static QString recoveryNameFromPath(
    const std::filesystem::path &path
) {
    const QString stem = QString::fromStdWString(path.stem().wstring());
    if (!stem.startsWith('.') || !stem.endsWith("_recovery")) {
        return QString();
    }

    return PaletteStore::normalizeName(
        stem.mid(1, stem.size() - QStringLiteral("_recovery").size() - 1));
}

static std::filesystem::path findRecoveryPath(
    const std::filesystem::path &directory
) {
    std::error_code ec;
    if (!std::filesystem::exists(directory, ec) || ec) {
        return {};
    }

    for (const auto &entry : std::filesystem::directory_iterator(directory, ec)) {
        if (ec || !entry.is_regular_file()) continue;

        const std::filesystem::path path = entry.path();
        if (path.extension() != ".txt") continue;
        if (!recoveryNameFromPath(path).isEmpty()) {
            return path;
        }
    }

    return {};
}

static void discardRecoveryFiles(const std::filesystem::path &directory) {
    std::error_code ec;
    if (!std::filesystem::exists(directory, ec) || ec) {
        return;
    }

    for (const auto &entry : std::filesystem::directory_iterator(directory, ec)) {
        if (ec || !entry.is_regular_file()) continue;

        const std::filesystem::path path = entry.path();
        if (path.extension() != ".txt") continue;
        if (recoveryNameFromPath(path).isEmpty()) continue;

        std::filesystem::remove(path, ec);
        ec.clear();
    }
}

struct NamedAssetSaveTarget {
    QString name;
    std::filesystem::path path;
    bool cancelled = false;
};

template <typename NormalizeName, typename IsValidName, typename MakeSuggestedName,
    typename PathForName>
static std::optional<NamedAssetSaveTarget> resolveNamedAssetSaveTarget(
    QWidget *parent, const QString &currentName, const QString &newEntryLabel,
    const QString &saveTitle, const QString &saveAsTitle,
    const QString &overwritePrompt, const QString &invalidNameMessage,
    const QString &defaultName, const std::filesystem::path &directory,
    const QString &dialogFilter, NormalizeName &&normalizeName,
    IsValidName &&isValidName, MakeSuggestedName &&makeSuggestedName,
    PathForName &&pathForName
) {
    QString targetName = normalizeName(currentName);
    if (!isValidName(targetName) || targetName == newEntryLabel) {
        bool accepted = false;
        const QString enteredName = QInputDialog::getText(parent, saveTitle, QObject::tr("Name"),
            QLineEdit::Normal, makeSuggestedName(currentName, defaultName), &accepted);
        if (!accepted) {
            return NamedAssetSaveTarget{ .cancelled = true };
        }

        targetName = normalizeName(enteredName);
        if (!isValidName(targetName)) {
            QMessageBox::warning(parent, saveTitle, invalidNameMessage);
            return std::nullopt;
        }
    }

    std::filesystem::path destinationPath = pathForName(targetName);
    std::error_code existsError;
    const bool destinationExists
        = std::filesystem::exists(destinationPath, existsError) && !existsError;
    const bool instantOverwrite
        = (QApplication::keyboardModifiers() & Qt::ShiftModifier) != 0;
    if (destinationExists && !instantOverwrite) {
        const QMessageBox::StandardButton choice = QMessageBox::question(parent,
            saveTitle, overwritePrompt.arg(targetName),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
            QMessageBox::Yes);
        if (choice == QMessageBox::Cancel) {
            return NamedAssetSaveTarget{ .cancelled = true };
        }
        if (choice == QMessageBox::No) {
            const QString savePath = showNativeSaveFileDialog(parent, saveAsTitle, directory,
                QFileInfo(Util::uniqueIndexedPathWithExtension(
                    directory, targetName, "txt")).fileName(),
                dialogFilter);
            if (savePath.isEmpty()) {
                return NamedAssetSaveTarget{ .cancelled = true };
            }

            const QString savePathWithExtension = QString::fromStdString(
                FileUtil::appendExtension(savePath.toStdString(), "txt"));
            const QString saveName = normalizeName(
                QFileInfo(savePathWithExtension).completeBaseName());
            if (!isValidName(saveName)) {
                QMessageBox::warning(parent, saveTitle, invalidNameMessage);
                return std::nullopt;
            }

            targetName = saveName;
            destinationPath = pathForName(targetName);
        }
    }

    return NamedAssetSaveTarget{
        .name = targetName,
        .path = destinationPath,
        .cancelled = false
    };
}

template <typename NameNormalizer, typename Config, typename ConfigEqual,
    typename CacheRecovery>
static bool syncNamedAssetState(
    const QString &previousName, const QString &currentName,
    const Config &previousConfig, const Config &currentConfig,
    bool wasDirty, bool isDirty, NameNormalizer &&normalizeName,
    ConfigEqual &&configEqual, CacheRecovery &&cacheRecovery
) {
    const bool changed = normalizeName(previousName)
            .compare(normalizeName(currentName), Qt::CaseInsensitive)
            != 0
        || !configEqual(previousConfig, currentConfig);
    const bool dirtyChanged = isDirty != wasDirty;
    if (changed || dirtyChanged) {
        cacheRecovery();
        return true;
    }

    return false;
}

GUIAppController::GUIAppController(
    GUILocale &locale, AppSettings &settings, QObject *parent)
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
    _backendNames = BackendCatalog::listNames(backendError);
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
    _refreshCollections();
    _refreshControlState();
    _refreshStatus();

    if (!_loadSelectedBackend()) {
        return false;
    }
    _requestRenderFromModel();
    return true;
}

void GUIAppController::show() {
    _controlWindow->show();
    _viewportWindow->show();
    Util::resizeViewportToImageSize(
        _viewportWindow.get(), _sessionState.outputSize());
    if (!_controlWindow->windowSettingsRestored()) {
        _positionWindowsForInitialShow();
    }
    QTimer::singleShot(0, this, [this]() {
        if (_controlWindow && _controlWindow->isVisible()) {
            _refreshPreviews();
        }
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
        [this](const QString &backendName) {
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
            Util::resizeViewportToImageSize(
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
    connect(_controlWindow.get(), &ControlWindow::closeRequested, this,
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
    connect(_controlWindow.get(), &ControlWindow::sineStateEdited, this,
        &GUIAppController::_syncSineDirtyStateFromControls);
    connect(_controlWindow.get(), &ControlWindow::sineSelectionRequested, this,
        [this](const QString &name) {
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
    connect(_controlWindow.get(), &ControlWindow::paletteStateEdited, this,
        &GUIAppController::_syncPaletteDirtyStateFromControls);
    connect(_controlWindow.get(), &ControlWindow::paletteSelectionRequested, this,
        [this](const QString &name) {
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
        this, [this](int pickTarget, const QPoint &pixel) {
            PendingPickAction pickAction;
            pickAction.target = static_cast<SelectionTarget>(pickTarget);
            pickAction.pixel = pixel;
            _requestRender(pickAction);
        });
    connect(&_viewportController, &ViewportController::quickSaveRequested, this,
        &GUIAppController::_quickSaveImage);
    connect(&_viewportController, &ViewportController::statusMessageChanged, this,
        &GUIAppController::_setStatusMessage);
    connect(_viewportWindow.get(), &ViewportWindow::closeRequested, this,
        &GUIAppController::_closeAllWindows);

    connect(&_renderController, &RenderController::previewImageChanged, this,
        [this]() { _viewportWindow->update(); });
    connect(&_renderController, &RenderController::renderStateChanged, this,
        [this]() {
            _refreshStatus();
            _viewportWindow->update();
        });
    connect(&_renderController, &RenderController::renderFailed, this,
        [this](const QString &message) {
            QMessageBox::warning(_controlWindow.get(), tr("Render"), message);
        });
    connect(&_renderController, &RenderController::automaticBackendSwitchRequested,
        this, [this](const QString &backendName, bool hasPickAction, int pickTarget,
            const QPoint &pickPixel) {
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
    const std::optional<PendingPickAction> &pickAction
) {
    _sessionState.mutableState().zoom = Util::clampGUIZoom(
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
        = Util::lightColorToQColor(_sessionState.state().lightColor);
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
        ? Util::defaultPixelsPerSecondText()
        : _renderController.pixelsPerSecondText();
    _controlWindow->setStatusState(statusText, _statusLinkPath, pixelsText,
        _renderController.imageMemoryText(), _renderController.progressValue(),
        _renderController.progressActive(), _renderController.progressCancelled());
}

void GUIAppController::_refreshPreviews() {
    _controlWindow->setSinePreviewImage(_renderController.renderSinePreview(
        _sessionState.state().sinePalette,
        _controlWindow->findChild<QWidget *>("sinePreview")->size(),
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
        SineStore::listNames(), _sessionState.state().sineName,
        _sessionState.isSineDirty());
    _controlWindow->setPaletteNames(
        PaletteStore::listNames(), _sessionState.state().paletteName,
        _sessionState.isPaletteDirty());
}

void GUIAppController::_syncSineDirtyStateFromControls() {
    const GUIState previousState = _sessionState.state();
    const bool wasDirty = _sessionState.isSineDirty();
    _controlWindow->syncToSessionState(_sessionState);
    if (syncNamedAssetState(previousState.sineName,
        _sessionState.state().sineName, previousState.sinePalette,
        _sessionState.state().sinePalette, wasDirty, _sessionState.isSineDirty(),
        [](const QString &name) { return PaletteStore::normalizeName(name); },
        [](const auto &previousConfig, const auto &currentConfig) {
            return SineStore::sameConfig(previousConfig, currentConfig);
        },
        [this]() { _cacheSineRecovery(); })) {
        _controlWindow->setSineNames(
            SineStore::listNames(), _sessionState.state().sineName,
            _sessionState.isSineDirty());
    }
}

void GUIAppController::_syncPaletteDirtyStateFromControls() {
    const GUIState previousState = _sessionState.state();
    const bool wasDirty = _sessionState.isPaletteDirty();
    _controlWindow->syncToSessionState(_sessionState);
    if (syncNamedAssetState(previousState.paletteName,
        _sessionState.state().paletteName, previousState.palette,
        _sessionState.state().palette, wasDirty, _sessionState.isPaletteDirty(),
        [](const QString &name) { return PaletteStore::normalizeName(name); },
        [](const auto &previousConfig, const auto &currentConfig) {
            return PaletteStore::sameConfig(previousConfig, currentConfig);
        },
        [this]() { _cachePaletteRecovery(); })) {
        _controlWindow->setPaletteNames(
            PaletteStore::listNames(), _sessionState.state().paletteName,
            _sessionState.isPaletteDirty());
    }
}

void GUIAppController::_setStatusMessage(const QString &message) {
    _statusText = message;
    _statusLinkPath.clear();
    _refreshStatus();
}

void GUIAppController::_setStatusSavedPath(const QString &path) {
    const QString absolutePath = QFileInfo(path).absoluteFilePath();
    _statusText = tr("Saved: %1").arg(QDir::toNativeSeparators(absolutePath));
    _statusLinkPath = absolutePath;
    _refreshStatus();
}

bool GUIAppController::_loadSelectedBackend() {
    QString errorMessage;
    if (_renderController.loadBackend(_sessionState.state().backend, errorMessage)) {
        _setStatusMessage(tr("Ready"));
        if (_controlWindow && _controlWindow->isVisible()) {
            _refreshPreviews();
        }
        return true;
    }

    QMessageBox::warning(_controlWindow.get(), tr("Backend"), errorMessage);
    return false;
}

std::optional<PendingPickAction> GUIAppController::_pendingPickAction(
    bool hasPickAction, int pickTarget, const QPoint &pickPixel
) const {
    if (!hasPickAction) {
        return std::nullopt;
    }

    PendingPickAction action;
    action.target = static_cast<SelectionTarget>(pickTarget);
    action.pixel = pickPixel;
    return action;
}

void GUIAppController::_initializeSessionState() {
    GUIState &state = _sessionState.mutableState();
    state.backend = _defaultBackend();
    state.iterations = 0;
    state.interactionTargetFPS = _settings.previewFallbackFPS();
    state.outputWidth = _settings.selectedOutputWidth();
    state.outputHeight = _settings.selectedOutputHeight();
    const QString recoveredSineName
        = recoveryNameFromPath(findRecoveryPath(SineStore::directoryPath()));
    const QString recoveredPaletteName
        = recoveryNameFromPath(findRecoveryPath(PaletteStore::directoryPath()));
    state.sineName = recoveredSineName.isEmpty()
        ? QString::fromUtf8(SineStore::defaultName)
        : recoveredSineName;
    state.paletteName = recoveredPaletteName.isEmpty()
        ? QString::fromUtf8(PaletteStore::defaultName)
        : recoveredPaletteName;

    QString sineError;
    if (!_loadSineByName(state.sineName, false, &sineError, false)) {
        const QString fallbackSineName = QString::fromUtf8(SineStore::defaultName);
        if (!_loadSineByName(fallbackSineName, false, nullptr, false)) {
            state.sineName = recoveredSineName.isEmpty()
                ? Util::uniqueIndexedNameFromList(
                    tr("New Sine"), SineStore::listNames())
                : recoveredSineName;
            state.sinePalette = SineStore::makeNewConfig();
            _sessionState.markSineSavedState();
            _refreshAllViews();
        }
    }
    QString paletteError;
    if (!_loadPaletteByName(state.paletteName, false, &paletteError, false)) {
        const QString fallbackPaletteName = QString::fromUtf8(PaletteStore::defaultName);
        if (!_loadPaletteByName(fallbackPaletteName, false, nullptr, false)) {
            state.paletteName = recoveredPaletteName.isEmpty()
                ? Util::uniqueIndexedNameFromList(
                    tr("New Palette"), PaletteStore::listNames())
                : recoveredPaletteName;
            state.palette = PaletteStore::makeNewConfig();
            _sessionState.markPaletteSavedState();
            _refreshAllViews();
        }
    }

    const CPUInfo cpu = queryCPUInfo();
    _controlWindow->setCpuInfo(
        QString::fromStdString(cpu.name), cpu.cores, cpu.threads);
    _sessionState.markPointViewSavedState();
    _restoreSineRecovery();
    _restorePaletteRecovery();
}

void GUIAppController::_positionWindowsForInitialShow() {
    const QScreen *screen
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
        = QString::fromUtf8(Constants::defaultBackendType);
    for (const QString &name : _backendNames) {
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
    auto result = runSaveImageDialog(_controlWindow.get(), "mandelbrot.png");
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
    std::filesystem::create_directories(Util::savesDirectoryPath(), ec);
    const QString path = QString::fromStdWString(
        (Util::savesDirectoryPath() / "mandelbrot.png").wstring());
    QString errorMessage;
    if (!_renderController.saveImage(path, true, "png", errorMessage)) {
        QMessageBox::warning(_controlWindow.get(), tr("Save"), errorMessage);
        return;
    }

    _setStatusSavedPath(path);
}

void GUIAppController::_changeLightColor() {
    const QColor initial
        = Util::lightColorToQColor(_sessionState.state().lightColor);
    const QColor color = QColorDialog::getColor(initial, _controlWindow.get(), tr("Color"));
    if (!color.isValid()) return;

    _sessionState.mutableState().lightColor = Util::lightColorFromQColor(color);
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

    GUIState &state = _sessionState.mutableState();
    state.sinePalette.freqR = randomFrequency();
    state.sinePalette.freqG = randomFrequency();
    state.sinePalette.freqB = randomFrequency();
    _refreshCollections();
    _cacheSineRecovery();
    _refreshControlState();
    _refreshPreviews();
    _requestRenderFromModel();
}

void GUIAppController::_savePointView() {
    (void)_savePointViewInteractive();
}

bool GUIAppController::_savePointViewInteractive() {
    _controlWindow->syncToSessionState(_sessionState);
    _sessionState.mutableState().zoom = Util::clampGUIZoom(_sessionState.state().zoom);

    QString errorMessage;
    if (!PointStore::ensureDirectory(errorMessage)) {
        QMessageBox::warning(_controlWindow.get(), tr("Save View"), errorMessage);
        return false;
    }

    const QString defaultPath = Util::uniqueIndexedPathWithExtension(
        PointStore::directoryPath(), tr("View"), "txt");
    const QString selectedPath = showNativeSaveFileDialog(_controlWindow.get(),
        tr("Save View"), PointStore::directoryPath(),
        QFileInfo(defaultPath).fileName(), tr("View Files (*.txt);;All Files (*.*)"));
    if (selectedPath.isEmpty()) return false;

    PointConfig point{ .fractal = Util::fractalTypeToConfigString(
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
        FileUtil::appendExtension(selectedPath.toStdString(), "txt"));
    if (!PointStore::saveToPath(
        outputPathWithExtension.toStdString(), point, errorMessage)) {
        QMessageBox::warning(_controlWindow.get(), tr("Save View"), errorMessage);
        return false;
    }

    _sessionState.markPointViewSavedState();
    _setStatusSavedPath(outputPathWithExtension);
    return true;
}

void GUIAppController::_loadPointView() {
    QString errorMessage;
    if (!PointStore::ensureDirectory(errorMessage)) {
        QMessageBox::warning(_controlWindow.get(), tr("Load View"), errorMessage);
        return;
    }

    const QString selectedPath = showNativeOpenFileDialog(_controlWindow.get(),
        tr("Load View"), PointStore::directoryPath(),
        tr("View Files (*.txt);;All Files (*.*)"));
    if (selectedPath.isEmpty()) return;

    PointConfig point;
    if (!PointStore::loadFromPath(
        selectedPath.toStdString(), point, errorMessage)) {
        QMessageBox::warning(_controlWindow.get(), tr("Load View"), errorMessage);
        return;
    }

    GUIState &state = _sessionState.mutableState();
    state.fractalType = Util::fractalTypeFromConfigString(
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
    const QString sourcePath = showNativeOpenFileDialog(_controlWindow.get(),
        tr("Import Sine Color"), SineStore::directoryPath(),
        tr("Sine Files (*.txt);;All Files (*.*)"));
    if (sourcePath.isEmpty()) return;

    Backend::SinePaletteConfig loaded;
    QString errorMessage;
    if (!SineStore::loadFromPath(
        sourcePath.toStdString(), loaded, errorMessage)) {
        QMessageBox::warning(_controlWindow.get(), tr("Sine Color"), errorMessage);
        return;
    }

    if (!SineStore::ensureDirectory(errorMessage)) {
        QMessageBox::warning(_controlWindow.get(), tr("Sine Color"), errorMessage);
        return;
    }

    const QStringList existingNames = SineStore::listNames();
    const QString importedName = PaletteStore::uniqueName(
        QFileInfo(sourcePath).completeBaseName(), existingNames);
    const std::filesystem::path destinationPath = SineStore::filePath(importedName);
    if (!SineStore::saveToPath(destinationPath, loaded, errorMessage)) {
        QMessageBox::warning(_controlWindow.get(), tr("Sine Color"), errorMessage);
        return;
    }

    _sessionState.mutableState().sineName = importedName;
    _sessionState.mutableState().sinePalette = loaded;
    _sessionState.markSineSavedState();
    _discardSineRecovery();
    _setStatusSavedPath(QString::fromStdWString(destinationPath.wstring()));
    _refreshAllViews();
    _requestRenderFromModel();
}

void GUIAppController::_saveSine() {
    _controlWindow->syncToSessionState(_sessionState);
    QString errorMessage;
    if (!SineStore::ensureDirectory(errorMessage)) {
        QMessageBox::warning(_controlWindow.get(), tr("Save Sine Color"), errorMessage);
        return;
    }
    const auto target = resolveNamedAssetSaveTarget(_controlWindow.get(),
        _sessionState.state().sineName, Util::translatedNewEntryLabel(),
        tr("Save Sine Color"), tr("Save Sine Color As"),
        tr("Sine \"%1\" already exists. Overwrite it?"),
        tr("Use an ASCII name with letters, numbers, spaces, ., _, or -."),
        tr("sine"), SineStore::directoryPath(),
        tr("Sine Files (*.txt);;All Files (*.*)"),
        [](const QString &name) { return PaletteStore::normalizeName(name); },
        [](const QString &name) { return PaletteStore::isValidName(name); },
        [](const QString &currentName, const QString &fallbackName) {
            const QString suggested = PaletteStore::sanitizeName(currentName);
            return suggested.isEmpty() ? fallbackName : suggested;
        },
        [](const QString &name) { return SineStore::filePath(name); });
    if (!target) return;
    if (target->cancelled) return;

    if (!SineStore::saveToPath(
        target->path, _sessionState.state().sinePalette, errorMessage)) {
        QMessageBox::warning(_controlWindow.get(), tr("Save Sine Color"), errorMessage);
        return;
    }

    _sessionState.mutableState().sineName = target->name;
    _sessionState.markSineSavedState();
    _discardSineRecovery();
    _setStatusSavedPath(QString::fromStdWString(target->path.wstring()));
    _refreshAllViews();
}

void GUIAppController::_loadPalette() {
    const QString sourcePath = showNativeOpenFileDialog(_controlWindow.get(),
        tr("Load Palette"), PaletteStore::directoryPath(),
        tr("Palette Files (*.txt);;All Files (*.*)"));
    if (sourcePath.isEmpty()) return;

    Backend::PaletteHexConfig loaded;
    QString importedName;
    std::filesystem::path destinationPath;
    QString errorMessage;
    if (!PaletteStore::importFromPath(sourcePath.toStdString(),
        _sessionState.state().palette.totalLength,
        _sessionState.state().palette.offset, importedName, loaded,
        destinationPath, errorMessage)) {
        QMessageBox::warning(_controlWindow.get(), tr("Palette"), errorMessage);
        return;
    }

    _sessionState.mutableState().paletteName = importedName;
    _sessionState.mutableState().palette = loaded;
    _sessionState.markPaletteSavedState();
    _discardPaletteRecovery();
    _setStatusSavedPath(QString::fromStdWString(destinationPath.wstring()));
    _refreshAllViews();
    _requestRenderFromModel();
}

void GUIAppController::_savePalette() {
    _controlWindow->syncToSessionState(_sessionState);
    QString errorMessage;
    const auto target = resolveNamedAssetSaveTarget(_controlWindow.get(),
        _sessionState.state().paletteName, Util::translatedNewEntryLabel(),
        tr("Save Palette"), tr("Save Palette As"),
        tr("Palette \"%1\" already exists. Overwrite it?"),
        tr("Use an ASCII name with letters, numbers, spaces, ., _, or -."),
        tr("palette"), PaletteStore::directoryPath(),
        tr("Palette Files (*.txt);;All Files (*.*)"),
        [](const QString &name) { return PaletteStore::normalizeName(name); },
        [](const QString &name) { return PaletteStore::isValidName(name); },
        [](const QString &currentName, const QString &fallbackName) {
            const QString suggested = PaletteStore::sanitizeName(currentName);
            return suggested.isEmpty() ? fallbackName : suggested;
        },
        [](const QString &name) { return PaletteStore::filePath(name); });
    if (!target) return;
    if (target->cancelled) return;
    std::filesystem::path destinationPath = target->path;

    if (!PaletteStore::saveNamed(
        target->name, _sessionState.state().palette, destinationPath, errorMessage)) {
        QMessageBox::warning(_controlWindow.get(), tr("Save Palette"), errorMessage);
        return;
    }

    _sessionState.mutableState().paletteName = target->name;
    _sessionState.markPaletteSavedState();
    _discardPaletteRecovery();
    _setStatusSavedPath(QString::fromStdWString(destinationPath.wstring()));
    _refreshAllViews();
}

void GUIAppController::_openPaletteEditor() {
    const Backend::PaletteHexConfig original = _sessionState.state().palette;
    const QString originalName = _sessionState.state().paletteName;
    PaletteDialog dialog(_sessionState.state().palette, originalName,
        [this](const QString &path) { _setStatusSavedPath(path); }, _controlWindow.get());

    if (dialog.exec() == QDialog::Accepted) {
        _sessionState.mutableState().palette = dialog.palette();
        _sessionState.mutableState().palette.totalLength
            = static_cast<float>(_sessionState.state().palette.totalLength);
        _sessionState.mutableState().palette.offset
            = static_cast<float>(_sessionState.state().palette.offset);
        if (!dialog.savedPaletteName().isEmpty() && !dialog.savedStateDirty()) {
            _sessionState.mutableState().paletteName = dialog.savedPaletteName();
            _sessionState.markPaletteSavedState();
            _discardPaletteRecovery();
        }
        _refreshAllViews();
        _cachePaletteRecovery();
        _requestRenderFromModel();
    } else {
        _sessionState.mutableState().palette = original;
        _sessionState.mutableState().paletteName = originalName;
        _refreshAllViews();
    }
}

void GUIAppController::_createNewSinePalette(bool requestRenderOnSuccess) {
    _discardSineRecovery();
    const QStringList existingNames = SineStore::listNames();
    _sessionState.mutableState().sineName = Util::uniqueIndexedNameFromList(
        tr("New Sine"), existingNames);
    _sessionState.mutableState().sinePalette = SineStore::makeNewConfig();
    _refreshAllViews();
    _cacheSineRecovery();
    if (requestRenderOnSuccess) {
        _requestRenderFromModel();
    }
}

void GUIAppController::_createNewColorPalette(bool requestRenderOnSuccess) {
    _discardPaletteRecovery();
    const QStringList existingNames = PaletteStore::listNames();
    _sessionState.mutableState().paletteName = Util::uniqueIndexedNameFromList(
        tr("New Palette"), existingNames);
    _sessionState.mutableState().palette = PaletteStore::makeNewConfig();
    _refreshAllViews();
    _cachePaletteRecovery();
    if (requestRenderOnSuccess) {
        _requestRenderFromModel();
    }
}

bool GUIAppController::_loadSineByName(
    const QString &name, bool requestRenderOnSuccess, QString *errorMessage,
    bool discardRecovery
) {
    if (errorMessage) errorMessage->clear();
    const QString normalizedName = PaletteStore::normalizeName(name);
    if (normalizedName.isEmpty()) return false;

    Backend::SinePaletteConfig loaded;
    const std::filesystem::path sourcePath = SineStore::filePath(normalizedName);
    QString localError;
    if (std::filesystem::exists(sourcePath)) {
        if (!SineStore::loadFromPath(sourcePath, loaded, localError)) {
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
    if (discardRecovery) {
        _discardSineRecovery();
    }
    _refreshAllViews();
    if (requestRenderOnSuccess) {
        _requestRenderFromModel();
    }
    return true;
}

bool GUIAppController::_loadPaletteByName(
    const QString &name, bool requestRenderOnSuccess, QString *errorMessage,
    bool discardRecovery
) {
    if (errorMessage) errorMessage->clear();
    const QString normalizedName = PaletteStore::normalizeName(name);
    if (normalizedName.isEmpty()) return false;

    Backend::PaletteHexConfig loaded;
    QString localError;
    if (!PaletteStore::loadNamed(normalizedName, loaded, localError)) {
        if (errorMessage) *errorMessage = localError;
        return false;
    }

    _sessionState.mutableState().paletteName = normalizedName;
    _sessionState.mutableState().palette = loaded;
    _sessionState.markPaletteSavedState();
    if (discardRecovery) {
        _discardPaletteRecovery();
    }
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

bool GUIAppController::_promptForDirtyViewOnClose() {
    _controlWindow->syncToSessionState(_sessionState);
    _sessionState.mutableState().zoom = Util::clampGUIZoom(
        _sessionState.state().zoom);

    if (!_sessionState.isPointViewDirty() || _sessionState.isHomeView()) {
        return true;
    }

    QMessageBox dialog(_controlWindow.get());
    dialog.setIcon(QMessageBox::Question);
    dialog.setWindowTitle(tr("Save View"));
    dialog.setText(tr("Current view has unsaved changes. Save it before closing?"));
    QPushButton *saveButton = dialog.addButton(QMessageBox::Save);
    QPushButton *discardButton = dialog.addButton(QMessageBox::Discard);
    QPushButton *cancelButton = dialog.addButton(QMessageBox::Cancel);
    dialog.setDefaultButton(saveButton);
    dialog.exec();

    if (dialog.clickedButton() == cancelButton) {
        return false;
    }
    if (dialog.clickedButton() == saveButton) {
        return _savePointViewInteractive();
    }
    return dialog.clickedButton() == discardButton;
}

std::filesystem::path GUIAppController::_paletteRecoveryPath(
    const QString &name
) const {
    const QString normalizedName = PaletteStore::normalizeName(name);
    return PaletteStore::directoryPath()
        / std::filesystem::path(
            QString(".%1_recovery.txt").arg(normalizedName).toStdString());
}

std::filesystem::path GUIAppController::_sineRecoveryPath(const QString &name) const {
    const QString normalizedName = PaletteStore::normalizeName(name);
    return SineStore::directoryPath()
        / std::filesystem::path(
            QString(".%1_recovery.txt").arg(normalizedName).toStdString());
}

void GUIAppController::_restorePaletteRecovery() {
    const std::filesystem::path path = findRecoveryPath(PaletteStore::directoryPath());
    if (path.empty()) {
        return;
    }

    Backend::PaletteHexConfig palette;
    QString errorMessage;
    if (!PaletteStore::loadFromPath(path, palette, errorMessage)) {
        _discardPaletteRecovery();
        return;
    }

    const QString recoveredName = recoveryNameFromPath(path);
    _sessionState.mutableState().paletteName = recoveredName.isEmpty()
        ? _sessionState.state().paletteName
        : recoveredName;
    _sessionState.mutableState().palette = palette;
}

void GUIAppController::_restoreSineRecovery() {
    const std::filesystem::path path = findRecoveryPath(SineStore::directoryPath());
    if (path.empty()) {
        return;
    }

    Backend::SinePaletteConfig palette;
    QString errorMessage;
    if (!SineStore::loadFromPath(path, palette, errorMessage)) {
        _discardSineRecovery();
        return;
    }

    const QString recoveredName = recoveryNameFromPath(path);
    _sessionState.mutableState().sineName = recoveredName.isEmpty()
        ? _sessionState.state().sineName
        : recoveredName;
    _sessionState.mutableState().sinePalette = palette;
}

void GUIAppController::_cachePaletteRecovery() {
    if (!_sessionState.isPaletteDirty()) {
        _discardPaletteRecovery();
        return;
    }

    QString errorMessage;
    if (!PaletteStore::ensureDirectory(errorMessage)) {
        return;
    }
    discardRecoveryFiles(PaletteStore::directoryPath());
    const std::filesystem::path recoveryPath
        = _paletteRecoveryPath(_sessionState.state().paletteName);
    if (!PaletteStore::saveToPath(recoveryPath,
        _sessionState.state().palette, errorMessage)) {
        return;
    }

    FileUtil::hideFile(recoveryPath);
}

void GUIAppController::_cacheSineRecovery() {
    if (!_sessionState.isSineDirty()) {
        _discardSineRecovery();
        return;
    }

    QString errorMessage;
    if (!SineStore::ensureDirectory(errorMessage)) {
        return;
    }
    discardRecoveryFiles(SineStore::directoryPath());
    const std::filesystem::path recoveryPath
        = _sineRecoveryPath(_sessionState.state().sineName);
    if (!SineStore::saveToPath(recoveryPath,
        _sessionState.state().sinePalette, errorMessage)) {
        return;
    }

    FileUtil::hideFile(recoveryPath);
}

void GUIAppController::_discardPaletteRecovery() {
    discardRecoveryFiles(PaletteStore::directoryPath());
}

void GUIAppController::_discardSineRecovery() {
    discardRecoveryFiles(SineStore::directoryPath());
}

void GUIAppController::_savePersistentState() {
    if (_quitPrepared) {
        return;
    }
    _quitPrepared = true;

    if (!_skipSaveSettings && _controlWindow) {
        _controlWindow->syncToSessionState(_sessionState);
        _cacheSineRecovery();
        _cachePaletteRecovery();
        _controlWindow->saveWindowSettings(_settings);
        _settings.setPreferredBackend(_sessionState.state().backend);
        _settings.setPreviewFallbackFPS(
            _sessionState.state().interactionTargetFPS);
        _settings.setSelectedOutputWidth(_sessionState.state().outputWidth);
        _settings.setSelectedOutputHeight(_sessionState.state().outputHeight);
        _settings.sync();
    }
}

void GUIAppController::_closeAllWindows() {
    if (_closingWindows) return;
    if (!_promptForDirtyViewOnClose()) {
        return;
    }

    _closingWindows = true;
    _savePersistentState();
    QTimer::singleShot(0, this, [this]() {
        if (_controlWindow) {
            _controlWindow->setCloseAllowed(true);
        }
        if (_viewportWindow) {
            _viewportWindow->setCloseAllowed(true);
        }

        if (_viewportWindow && _viewportWindow->isVisible()) {
            _viewportWindow->close();
        }
        if (_controlWindow && _controlWindow->isVisible()) {
            _controlWindow->close();
        }

        QCoreApplication::exit(0);
        });
}
