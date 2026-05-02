#pragma once

#include <filesystem>
#include <memory>
#include <optional>

#include <QObject>
#include <QStringList>

#include "locale/GUILocale.h"
#include "settings/AppSettings.h"
#include "settings/Shortcuts.h"
#include "controllers/ViewportController.h"
#include "GUISessionState.h"
#include "runtime/RenderController.h"
#include "windows/control/ControlWindow.h"
#include "windows/viewport/ViewportWindow.h"

class GUIAppController final : public QObject {
    Q_OBJECT

public:
    GUIAppController(GUILocale &locale, AppSettings &settings,
        QObject *parent = nullptr);
    ~GUIAppController() override;

    bool initialize();
    void show();

private slots:
    void _requestRenderFromControls();
    void _clearSavedSettings();

private:
    GUILocale &_locale;
    AppSettings &_settings;
    Shortcuts _shortcuts;
    GUISessionState _sessionState;
    RenderController _renderController;
    ViewportController _viewportController;
    std::unique_ptr<ControlWindow> _controlWindow;
    std::unique_ptr<ViewportWindow> _viewportWindow;
    QStringList _backendNames;
    QString _statusText;
    QString _statusLinkPath;
    bool _closingWindows = false;
    bool _skipSaveSettings = false;
    bool _quitPrepared = false;

    void _connectUI();
    void _refreshAllViews();
    void _refreshControlState();
    void _refreshStatus();
    void _refreshPreviews();
    void _refreshCollections();
    void _syncSineDirtyStateFromControls();
    void _syncPaletteDirtyStateFromControls();
    void _setStatusMessage(const QString &message);
    void _setStatusSavedPath(const QString &path);
    bool _loadSelectedBackend();
    void _requestRender(
        const std::optional<GUI::PendingPickAction> &pickAction = std::nullopt
    );
    [[nodiscard]] std::optional<GUI::PendingPickAction> _pendingPickAction(
        bool hasPickAction, int pickTarget, const QPoint &pickPixel
    ) const;
    void _applyViewportScalePercent(float scalePercent);
    void _initializeSessionState();
    void _positionWindowsForInitialShow();
    QString _defaultBackend() const;
    void _showSettingsDialog();
    void _showHelpDialog();
    void _showAboutDialog();
    void _saveImage();
    void _quickSaveImage();
    void _changeLightColor();
    void _randomizeSinePalette();
    void _savePointView();
    void _loadPointView();
    void _importSine();
    void _saveSine();
    void _loadPalette();
    void _savePalette();
    void _openPaletteEditor();
    void _createNewSinePalette(bool requestRenderOnSuccess);
    void _createNewColorPalette(bool requestRenderOnSuccess);
    bool _loadSineByName(const QString &name, bool requestRenderOnSuccess,
        QString *errorMessage = nullptr, bool discardRecovery = true);
    bool _loadPaletteByName(const QString &name, bool requestRenderOnSuccess,
        QString *errorMessage = nullptr, bool discardRecovery = true);
    bool _confirmDiscardDirtySine();
    bool _confirmDiscardDirtyPalette();
    [[nodiscard]] bool _promptForDirtyViewOnClose();
    [[nodiscard]] bool _savePointViewInteractive();
    void _restorePaletteRecovery();
    void _restoreSineRecovery();
    void _cachePaletteRecovery();
    void _cacheSineRecovery();
    void _discardPaletteRecovery();
    void _discardSineRecovery();
    [[nodiscard]] std::filesystem::path _paletteRecoveryPath(
        const QString &name
    ) const;
    [[nodiscard]] std::filesystem::path _sineRecoveryPath(const QString &name) const;
    void _savePersistentState();
    void _closeAllWindows(bool skipDirtyViewPrompt = false);
};
