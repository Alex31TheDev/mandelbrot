#include "PaletteDialog.h"
#include "ui_PaletteDialog.h"

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

#include "BackendAPI.h"
using namespace Backend;

#include "services/PaletteStore.h"
#include "services/NativeFileDialog.h"

#include "util/FileUtil.h"
#include "util/GUIUtil.h"

using namespace GUI;

PaletteDialog::PaletteDialog(const PaletteRGBConfig &palette,
    const QString &paletteName,
    std::function<void(const QString &)> savedPathCallback, QWidget *parent)
    : QDialog(parent)
    , _ui(std::make_unique<Ui::PaletteDialog>())
    , _palette(palette)
    , _savedPaletteName(PaletteStore::normalizeName(paletteName))
    , _savedPathCallback(std::move(savedPathCallback)) {
    _ui->setupUi(this);

    _ui->nameEdit->setText(_savedPaletteName.isEmpty()
        ? QString::fromLatin1(PaletteStore::defaultName)
        : _savedPaletteName);
    _ui->nameEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression("[A-Za-z0-9 _.\\-]*"), _ui->nameEdit
    ));

    Util::stabilizePushButton(_ui->addButton);
    Util::stabilizePushButton(_ui->removeButton);
    Util::stabilizePushButton(_ui->evenButton);
    Util::stabilizePushButton(_ui->colorButton);
    Util::stabilizePushButton(_ui->importButton);
    Util::stabilizePushButton(_ui->saveButton);

    _applyPalette(_palette);

    connect(
        _ui->paletteTimeline, &PaletteTimelineWidget::changed, this, [this]() {
            _savedPaletteDirty = true;
            _refreshButtonState();
        }
    );
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
            _ui->paletteTimeline->setBlendEnds(checked);
            _savedPaletteDirty = true;
            _refreshButtonState();
        }
    );
    connect(_ui->nameEdit, &QLineEdit::textEdited, this,
        [this](const QString &) { _savedPaletteDirty = true; });
    connect(_ui->importButton, &QPushButton::clicked, this,
        &PaletteDialog::_importPalette);
    connect(_ui->saveButton, &QPushButton::clicked, this,
        &PaletteDialog::_savePalette);

    _refreshButtonState();
}

PaletteDialog::~PaletteDialog() = default;

PaletteRGBConfig PaletteDialog::palette() const {
    return _currentPalette();
}

QString PaletteDialog::savedPaletteName() const {
    return _savedPaletteName;
}

bool PaletteDialog::savedStateDirty() const {
    return _savedPaletteDirty;
}

void PaletteDialog::accept() {
    _palette = _currentPalette();
    QDialog::accept();
}

PaletteRGBConfig PaletteDialog::_currentPalette() const {
    return PaletteStore::stopsToConfig(_ui->paletteTimeline->stops(),
        _palette.totalLength, _palette.offset,
        _ui->blendEndsCheck->isChecked());
}

void PaletteDialog::_applyPalette(const PaletteRGBConfig &palette) {
    _palette = palette;
    const QSignalBlocker blendBlocker(_ui->blendEndsCheck);
    _ui->blendEndsCheck->setChecked(_palette.blendEnds);
    _ui->paletteTimeline->setBlendEnds(_palette.blendEnds);
    _ui->paletteTimeline->setStops(PaletteStore::configToStops(_palette));
}

QString PaletteDialog::_validatedPaletteName() const {
    const QString name
        = PaletteStore::normalizeName(_ui->nameEdit->text());
    return PaletteStore::isValidName(name) ? name : QString();
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
    const QString sourcePath = showNativeOpenFileDialog(this,
        tr("Import Palette"), PaletteStore::directoryPath(),
        tr("Palette Files (*.txt);;All Files (*.*)"));
    if (sourcePath.isEmpty()) return;

    PaletteRGBConfig loaded;
    QString importedName;
    std::filesystem::path destinationPath;
    QString errorMessage;
    if (!PaletteStore::importFromPath(sourcePath.toStdString(),
        _palette.totalLength, _palette.offset, importedName, loaded,
        destinationPath, errorMessage)) {
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
    if (!PaletteStore::ensureDirectory(errorMessage)) {
        QMessageBox::warning(this, tr("Palette"), errorMessage);
        return;
    }

    _palette = _currentPalette();
    QString targetName = paletteName;
    std::filesystem::path destinationPath
        = PaletteStore::filePath(targetName);
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
            const QString savePath = showNativeSaveDialog(this,
                tr("Save Palette As"), PaletteStore::directoryPath(),
                QFileInfo(Util::uniqueIndexedPathWithExtension(
                    PaletteStore::directoryPath(), targetName, "txt"
                ))
                .fileName(),
                tr("Palette Files (*.txt);;All Files (*.*)"));
            if (savePath.isEmpty()) return;

            const QString savePathWithExtension = QString::fromStdString(
                FileUtil::appendExtension(savePath.toStdString(), "txt")
            );
            QString saveName;
            if (!PaletteStore::saveFromDialogPath(savePathWithExtension,
                _palette, saveName, destinationPath, errorMessage)) {
                QMessageBox::warning(this, tr("Palette"),
                    errorMessage);
                return;
            }

            targetName = saveName;
        } else if (!PaletteStore::saveNamed(targetName, _palette,
            destinationPath, errorMessage)) {
            QMessageBox::warning(this, tr("Palette"), errorMessage);
            return;
        }
    }

    if (!destinationExists
        && !PaletteStore::saveNamed(targetName, _palette, destinationPath,
            errorMessage)) {
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