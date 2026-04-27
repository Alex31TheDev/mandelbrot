#pragma once

#include <memory>
#include <vector>

#include <QDialog>
#include <QKeySequenceEdit>

#include "AppSettings.h"
#include "Shortcuts.h"

namespace Ui {
class SettingsDialog;
}

class SettingsDialog final : public QDialog {
    Q_OBJECT

public:
    SettingsDialog(const QString& language, int previewFallbackFPS,
        const Shortcuts& shortcuts, QWidget* parent = nullptr);
    ~SettingsDialog() override;

    [[nodiscard]] QString language() const;
    [[nodiscard]] int previewFallbackFPS() const;
    [[nodiscard]] Shortcuts shortcuts(AppSettings& settings) const;

private:
    std::unique_ptr<Ui::SettingsDialog> _ui;
    std::vector<std::pair<QString, QKeySequenceEdit*>> _shortcutEdits;
};
