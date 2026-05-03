#include "SettingsDialog.h"
#include "ui_SettingsDialog.h"

#include <algorithm>
#include <filesystem>
#include <vector>

#include <QCoreApplication>
#include <QKeySequenceEdit>
#include <QLocale>

#include "app/GUIConstants.h"

#include "settings/AppSettings.h"

#include "util/FileUtil.h"

using namespace GUI;

static QString _displayLanguageName(const QString &code) {
    if (code.trimmed().compare("en", Qt::CaseInsensitive) == 0) {
        return QCoreApplication::translate("SettingsDialog", "English (US)");
    }

    const QLocale locale(code);
    QString name = locale.nativeLanguageName();
    if (name.isEmpty()) {
        name = QLocale::languageToString(locale.language());
    }
    if (name.isEmpty()) return code;

    name[0] = name.at(0).toUpper();
    return name;
}

static std::vector<QString> _availableLanguageCodes() {
    std::vector<QString> codes{ "en" };
    const std::filesystem::path translationsDir
        = FileUtil::executableDir() / "translations";

    std::error_code ec;
    if (!std::filesystem::exists(translationsDir, ec)
        || !std::filesystem::is_directory(translationsDir, ec)) {
        return codes;
    }

    for (const auto &entry :
        std::filesystem::directory_iterator(translationsDir, ec)) {
        if (ec || !entry.is_regular_file()) continue;

        const QString fileName
            = QString::fromStdWString(entry.path().filename().wstring());
        if (!fileName.startsWith("mandelbrot_") || !fileName.endsWith(".qm")) {
            continue;
        }

        const QString code
            = fileName.mid(QString("mandelbrot_").size(),
                fileName.size() - QString("mandelbrot_").size() - 3)
            .trimmed()
            .toLower();
        if (!code.isEmpty()) codes.push_back(code);
    }

    std::sort(codes.begin(), codes.end());
    codes.erase(std::unique(codes.begin(), codes.end()), codes.end());
    const auto english = std::find(codes.begin(), codes.end(), QString("en"));
    if (english != codes.end() && english != codes.begin()) {
        std::rotate(codes.begin(), english, english + 1);
    }
    return codes;
}

SettingsDialog::SettingsDialog(const QString &language, int previewFallbackFPS,
    const Shortcuts &shortcuts, QWidget *parent)
    : QDialog(parent)
    , _ui(std::make_unique<Ui::SettingsDialog>()) {
    _ui->setupUi(this);
    const std::vector<QString> languages = _availableLanguageCodes();
    for (const QString &code : languages) {
        _ui->languageCombo->addItem(_displayLanguageName(code), code);
    }

    const int langIndex = _ui->languageCombo->findData(language);
    _ui->languageCombo->setCurrentIndex(langIndex >= 0 ? langIndex : 0);
    _ui->previewFallbackSpin->setValue(std::max(0, previewFallbackFPS));

    for (const ShortcutDef &def : Shortcuts::defs()) {
        auto *edit = new QKeySequenceEdit(this);
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
        : Constants::defaultInteractionTargetFPS;
}

Shortcuts SettingsDialog::shortcuts(AppSettings &settings) const {
    Shortcuts result(settings);
    for (const auto &[id, edit] : _shortcutEdits) {
        result.setSequence(id, edit ? edit->keySequence() : QKeySequence());
    }
    return result;
}