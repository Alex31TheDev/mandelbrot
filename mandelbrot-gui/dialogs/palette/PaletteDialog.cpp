#include "dialogs/palette/PaletteDialog.h"

#include <filesystem>
#include <system_error>
#include <utility>

#include <QCheckBox>
#include <QColorDialog>
#include <QFileInfo>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSignalBlocker>

#include "services/NativeFileDialog.h"
#include "services/PaletteStore.h"
#include "ui_PaletteDialog.h"
#include "util/FormatUtil.h"
#include "util/PathUtil.h"

static void stabilizePushButton(QPushButton* button) {
    if (!button) return;
    button->setAutoDefault(false);
    button->setDefault(false);
}

static QString uniqueIndexedPathWithExtension(
    const std::filesystem::path& directory, const QString& baseName,
    const QString& extension) {
    const std::string uniqueName
        = FormatUtil::uniqueIndexedName(baseName.toStdString(),
            [&directory, &extension](const std::string_view candidate) {
                std::error_code ec;
                const std::filesystem::path path = directory
                    / std::filesystem::path(
                        std::string(candidate) + "." + extension.toStdString());
                return std::filesystem::exists(path, ec) && !ec;
            });

    return QString::fromStdWString((directory
        / std::filesystem::path(uniqueName + "." + extension.toStdString()))
            .wstring());
}

PaletteDialog::PaletteDialog(const Backend::PaletteHexConfig& palette,
    const QString& paletteName,
    std::function<void(const QString&)> savedPathCallback, QWidget* parent)
    : QDialog(parent)
    , _ui(std::make_unique<Ui::PaletteDialog>())
    , _palette(palette)
    , _savedPaletteName(GUI::PaletteStore::normalizeName(paletteName))
    , _savedPathCallback(std::move(savedPathCallback)) {
    _ui->setupUi(this);
    resize(420, 200);

    _ui->nameEdit->setText(_savedPaletteName.isEmpty()
            ? QString::fromLatin1(GUI::PaletteStore::kDefaultName)
            : _savedPaletteName);
    _ui->nameEdit->setMaxLength(64);
    _ui->nameEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression("[A-Za-z0-9 _.\\-]*"), _ui->nameEdit));

    stabilizePushButton(_ui->addButton);
    stabilizePushButton(_ui->removeButton);
    stabilizePushButton(_ui->evenButton);
    stabilizePushButton(_ui->colorButton);
    stabilizePushButton(_ui->importButton);
    stabilizePushButton(_ui->saveButton);

    _applyPalette(_palette);

    connect(
        _ui->paletteTimeline, &PaletteTimelineWidget::changed, this, [this]() {
            _palette = _currentPalette();
            _savedPaletteDirty = true;
        });
    connect(_ui->paletteTimeline, &PaletteTimelineWidget::selectionChanged,
        this, [this](int) { _refreshButtonState(); });
    connect(_ui->paletteTimeline, &PaletteTimelineWidget::editColorRequested,
        this, [this](int) { _editColor(); });
    connect(_ui->addButton, &QPushButton::clicked, _ui->paletteTimeline,
        &PaletteTimelineWidget::addStop);
    connect(_ui->removeButton, &QPushButton::clicked, _ui->paletteTimeline,
        &PaletteTimelineWidget::removeSelectedStops);
    connect(_ui->evenButton, &QPushButton::clicked, _ui->paletteTimeline,
        &PaletteTimelineWidget::evenSpacing);
    connect(_ui->colorButton, &QPushButton::clicked, this,
        &PaletteDialog::_editColor);
    connect(
        _ui->blendEndsCheck, &QCheckBox::toggled, this, [this](bool checked) {
            _palette.blendEnds = checked;
            _ui->paletteTimeline->setBlendEnds(checked);
            _palette = _currentPalette();
            _savedPaletteDirty = true;
        });
    connect(_ui->nameEdit, &QLineEdit::textEdited, this,
        [this](const QString&) { _savedPaletteDirty = true; });
    connect(_ui->importButton, &QPushButton::clicked, this,
        &PaletteDialog::_importPalette);
    connect(_ui->saveButton, &QPushButton::clicked, this,
        &PaletteDialog::_savePalette);

    _refreshButtonState();
}

PaletteDialog::~PaletteDialog() = default;

Backend::PaletteHexConfig PaletteDialog::palette() const {
    return _currentPalette();
}

QString PaletteDialog::savedPaletteName() const {
    return _savedPaletteName;
}

bool PaletteDialog::savedStateDirty() const {
    return _savedPaletteDirty;
}

Backend::PaletteHexConfig PaletteDialog::_currentPalette() const {
    return GUI::PaletteStore::stopsToConfig(_ui->paletteTimeline->stops(),
        _palette.totalLength, _palette.offset,
        _ui->blendEndsCheck->isChecked());
}

void PaletteDialog::_applyPalette(const Backend::PaletteHexConfig& palette) {
    _palette = palette;
    const QSignalBlocker blendBlocker(_ui->blendEndsCheck);
    _ui->blendEndsCheck->setChecked(_palette.blendEnds);
    _ui->paletteTimeline->setBlendEnds(_palette.blendEnds);
    _ui->paletteTimeline->setStops(GUI::PaletteStore::configToStops(_palette));
}

QString PaletteDialog::_validatedPaletteName() const {
    const QString name
        = GUI::PaletteStore::normalizeName(_ui->nameEdit->text());
    return GUI::PaletteStore::isValidName(name) ? name : QString();
}

void PaletteDialog::_editColor() {
    const int index = _ui->paletteTimeline->selectedIndex();
    if (index < 0) return;

    const QColor initial = _ui->paletteTimeline->stops()[index].color;
    const QColor color = QColorDialog::getColor(initial, this, tr("Color"));
    if (!color.isValid()) return;

    _ui->paletteTimeline->setSelectedColor(color);
}

void PaletteDialog::_importPalette() {
    const QString sourcePath = GUI::showNativeOpenFileDialog(this,
        tr("Import Palette"), GUI::PaletteStore::directoryPath(),
        tr("Palette Files (*.txt);;All Files (*.*)"));
    if (sourcePath.isEmpty()) return;

    Backend::PaletteHexConfig loaded;
    QString errorMessage;
    if (!GUI::PaletteStore::loadFromPath(
            sourcePath.toStdString(), loaded, errorMessage)) {
        QMessageBox::warning(this, tr("Palette"), errorMessage);
        return;
    }

    loaded.totalLength = _palette.totalLength;
    loaded.offset = _palette.offset;

    if (!GUI::PaletteStore::ensureDirectory(errorMessage)) {
        QMessageBox::warning(this, tr("Palette"), errorMessage);
        return;
    }

    const QStringList existingNames = GUI::PaletteStore::listNames();
    const QString importedName = GUI::PaletteStore::uniqueName(
        QFileInfo(sourcePath).completeBaseName(), existingNames);
    const std::filesystem::path destinationPath
        = GUI::PaletteStore::filePath(importedName);
    if (!GUI::PaletteStore::saveToPath(destinationPath, loaded, errorMessage)) {
        QMessageBox::warning(this, tr("Palette"), errorMessage);
        return;
    }

    _savedPaletteName = importedName;
    _ui->nameEdit->setText(importedName);
    _applyPalette(loaded);
    _savedPaletteDirty = false;
    if (_savedPathCallback) {
        _savedPathCallback(QString::fromStdWString(destinationPath.wstring()));
    }
}

void PaletteDialog::_savePalette() {
    const QString paletteName = _validatedPaletteName();
    if (paletteName.isEmpty()) {
        QMessageBox::warning(this, tr("Palette"),
            tr("Palette names must be non-empty ASCII and use only letters, "
               "numbers, spaces, '.', '-', or '_'."));
        return;
    }

    QString errorMessage;
    if (!GUI::PaletteStore::ensureDirectory(errorMessage)) {
        QMessageBox::warning(this, tr("Palette"), errorMessage);
        return;
    }

    _palette = _currentPalette();
    QString targetName = paletteName;
    std::filesystem::path destinationPath
        = GUI::PaletteStore::filePath(targetName);
    std::error_code existsError;
    const bool destinationExists
        = std::filesystem::exists(destinationPath, existsError) && !existsError;
    if (destinationExists) {
        const QMessageBox::StandardButton choice = QMessageBox::question(this,
            tr("Palette"),
            tr("Palette \"%1\" already exists. Overwrite it?").arg(targetName),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
            QMessageBox::Yes);
        if (choice == QMessageBox::Cancel) return;
        if (choice == QMessageBox::No) {
            const QString savePath = GUI::showNativeSaveFileDialog(this,
                tr("Save Palette As"), GUI::PaletteStore::directoryPath(),
                QFileInfo(
                    uniqueIndexedPathWithExtension(
                        GUI::PaletteStore::directoryPath(), targetName, "txt"))
                    .fileName(),
                tr("Palette Files (*.txt);;All Files (*.*)"));
            if (savePath.isEmpty()) return;

            const QString savePathWithExtension = QString::fromStdString(
                PathUtil::appendExtension(savePath.toStdString(), "txt"));
            const QString saveName = GUI::PaletteStore::normalizeName(
                QFileInfo(savePathWithExtension).completeBaseName());
            if (!GUI::PaletteStore::isValidName(saveName)) {
                QMessageBox::warning(this, tr("Palette"),
                    tr("Use an ASCII name with letters, numbers, spaces, ., _, "
                       "or -."));
                return;
            }

            targetName = saveName;
            destinationPath = GUI::PaletteStore::filePath(targetName);
        }
    }

    if (!GUI::PaletteStore::saveToPath(
            destinationPath, _palette, errorMessage)) {
        QMessageBox::warning(this, tr("Palette"), errorMessage);
        return;
    }

    _savedPaletteName = targetName;
    _ui->nameEdit->setText(_savedPaletteName);
    _savedPaletteDirty = false;
    if (_savedPathCallback) {
        _savedPathCallback(QString::fromStdWString(destinationPath.wstring()));
    }
}

void PaletteDialog::_refreshButtonState() {
    _ui->colorButton->setEnabled(_ui->paletteTimeline->selectedIndex() >= 0);
    _ui->removeButton->setEnabled(_ui->paletteTimeline->selectedCount() > 0
        && static_cast<int>(_ui->paletteTimeline->stops().size())
                - _ui->paletteTimeline->selectedCount()
            >= 2);
}
