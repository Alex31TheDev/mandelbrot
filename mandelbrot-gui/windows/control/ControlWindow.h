#pragma once

#include <functional>
#include <memory>

#include <QColor>
#include <QComboBox>
#include <QCloseEvent>
#include <QEvent>
#include <QImage>
#include <QMainWindow>
#include <QPixmap>
#include <QShowEvent>

#include "settings/AppSettings.h"
#include "settings/Shortcuts.h"

#include "BackendAPI.h"
#include "options/ColorMethods.h"

#include "app/GUITypes.h"
#include "app/GUISessionState.h"
#include "util/GUIUtil.h"

namespace Ui {
    class ControlWindow;
}

class ControlWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit ControlWindow(QWidget *parent = nullptr);
    ~ControlWindow() override;

    void setBackendNames(const QStringList &names,
        const QString &selectedBackend);
    void setSineNames(
        const QStringList &names, const QString &currentName, bool dirty
    );
    void setPaletteNames(
        const QStringList &names, const QString &currentName, bool dirty
    );
    void setCpuInfo(const QString &name, int cores, int threads);
    void setSessionState(
        const GUISessionState &sessionState, GUI::NavMode displayedNavMode,
        GUI::SelectionTarget selectionTarget
    );
    void syncToSessionState(GUISessionState &sessionState) const;
    void applyShortcuts(const Shortcuts &shortcuts);
    void setIterationAutoResolver(std::function<int()> resolver);
    void restoreWindowSettings(const AppSettings &settings);
    void saveWindowSettings(AppSettings &settings) const;
    void setStatusState(
        const QString &statusText, const QString &statusLinkPath,
        const QString &pixelsPerSecondText, const QString &imageMemoryText,
        int progressValue, bool progressActive, bool progressCancelled
    );
    void setSinePreviewImage(const QImage &image);
    void setPalettePreviewPixmap(const QPixmap &pixmap);
    void clearPalettePreview();
    void setLightColorButton(const QColor &color, const QString &text);

    [[nodiscard]] std::pair<double, double> sinePreviewRange() const;
    [[nodiscard]] bool windowSettingsRestored() const {
        return _windowSettingsRestored;
    }
    void setCloseAllowed(bool allowed) { _closeAllowed = allowed; }

signals:
    void backendSelectionRequested(const QString &backendName);
    void renderRequested();
    void threadUsageChanged(bool checked);
    void previewRefreshRequested();
    void homeRequested();
    void zoomCenterRequested();
    void saveImageRequested();
    void quickSaveRequested();
    void viewportResizeRequested();
    void viewportScaleChanged(float scalePercent);
    void saveViewRequested();
    void loadViewRequested();
    void settingsRequested();
    void clearSavedSettingsRequested();
    void helpRequested();
    void aboutRequested();
    void exitRequested();
    void cancelRenderRequested();
    void cycleGridRequested();
    void toggleViewportOverlayRequested();
    void toggleViewportFullscreenRequested();
    void navModeChanged(GUI::NavMode mode);
    void selectionTargetChanged(GUI::SelectionTarget target);
    void sineStateEdited();
    void sineSelectionRequested(const QString &name);
    void newSineRequested();
    void randomizeSineRequested();
    void saveSineRequested();
    void importSineRequested();
    void paletteStateEdited();
    void paletteSelectionRequested(const QString &name);
    void newPaletteRequested();
    void loadPaletteRequested();
    void savePaletteRequested();
    void paletteEditorRequested();
    void lightColorChangeRequested();
    void closeRequested(bool skipDirtyViewPrompt);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void changeEvent(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    std::unique_ptr<Ui::ControlWindow> _ui;
    bool _controlWindowSized = false;
    QString _statusText;
    QString _statusLinkPath;
    QString _pixelsPerSecondText;
    QString _imageMemoryText = GUI::Util::defaultImageMemoryText();
    QStringList _sineNames;
    QString _currentSineName;
    bool _sineDirty = false;
    QStringList _paletteNames;
    QString _currentPaletteName;
    bool _paletteDirty = false;
    int _progressValue = 0;
    bool _progressActive = false;
    bool _progressCancelled = false;
    QSize _lastOutputSize{
        GUI::Constants::defaultOutputWidth, GUI::Constants::defaultOutputHeight
    };
    bool _aspectReferenceInitialized = false;
    bool _windowSettingsRestored = false;
    bool _closeAllowed = false;
    bool _pointViewDirty = false;
    std::function<int()> _iterationAutoResolver;

    void _buildUI();
    void _populateStaticControls();
    void _connectUI();
    void _retranslateMenus();
    void _retranslateDynamicControls();
    void _updateModeEnablement(ColorMethod colorMethod);
    void _updateControlWindowSize();
    void _updateWindowTitle();
    void _updateAspectLinkedSizes(bool widthChanged);
    void _updateStatusRightEdgeAlignment();
    void _updateStatusLabels();
    void _refreshNamedCombo(QComboBox *combo, const QStringList &names,
        const QString &currentName, bool dirty) const;
};
