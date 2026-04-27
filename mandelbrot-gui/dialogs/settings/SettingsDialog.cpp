#include "SettingsDialog.h"

#include <algorithm>

#include <QKeySequenceEdit>

#include "AppSettings.h"
#include "GUITypes.h"
#include "ui_SettingsDialog.h"

SettingsDialog::SettingsDialog(const QString& language, int previewFallbackFPS,
    const Shortcuts& shortcuts, QWidget* parent)
    : QDialog(parent)
    , _ui(std::make_unique<Ui::SettingsDialog>()) {
    _ui->setupUi(this);
    _ui->languageCombo->addItem(tr("English"), "en");
    _ui->languageCombo->addItem(tr("Romanian"), "ro");
    const int langIndex = _ui->languageCombo->findData(language);
    _ui->languageCombo->setCurrentIndex(langIndex >= 0 ? langIndex : 0);
    _ui->previewFallbackSpin->setValue(std::max(0, previewFallbackFPS));

    for (const ShortcutDef& def : Shortcuts::defs()) {
        auto* edit = new QKeySequenceEdit(this);
        edit->setKeySequence(shortcuts.sequence(def.id));
        _ui->shortcutLayout->addRow(Shortcuts::label(def), edit);
        _shortcutEdits.push_back({ def.id, edit });
    }
}

SettingsDialog::~SettingsDialog() = default;

QString SettingsDialog::language() const {
    return _ui ? _ui->languageCombo->currentData().toString() : "en";
}

int SettingsDialog::previewFallbackFPS() const {
    return _ui ? _ui->previewFallbackSpin->value()
               : kDefaultInteractionTargetFPS;
}

Shortcuts SettingsDialog::shortcuts(AppSettings& settings) const {
    Shortcuts result(settings);
    for (const auto& [id, edit] : _shortcutEdits) {
        result.setSequence(id, edit ? edit->keySequence() : QKeySequence());
    }
    return result;
}
