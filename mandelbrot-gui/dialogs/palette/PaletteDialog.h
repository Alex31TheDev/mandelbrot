#pragma once

#include <functional>
#include <memory>

#include <QDialog>
#include <QString>

#include "BackendAPI.h"

namespace Ui {
class PaletteDialog;
}

class PaletteDialog final : public QDialog {
    Q_OBJECT

public:
    PaletteDialog(const Backend::PaletteHexConfig& palette,
        const QString& paletteName,
        std::function<void(const QString&)> savedPathCallback,
        QWidget* parent = nullptr);
    ~PaletteDialog() override;

    [[nodiscard]] Backend::PaletteHexConfig palette() const;
    [[nodiscard]] QString savedPaletteName() const;
    [[nodiscard]] bool savedStateDirty() const;

private:
    std::unique_ptr<Ui::PaletteDialog> _ui;
    Backend::PaletteHexConfig _palette;
    QString _savedPaletteName;
    bool _savedPaletteDirty = false;
    std::function<void(const QString&)> _savedPathCallback;

    [[nodiscard]] Backend::PaletteHexConfig _currentPalette() const;
    void _applyPalette(const Backend::PaletteHexConfig& palette);
    [[nodiscard]] QString _validatedPaletteName() const;
    void _editColor();
    void _importPalette();
    void _savePalette();
    void _refreshButtonState();
};
