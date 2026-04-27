#include "mandelbrotgui.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <climits>
#include <filesystem>
#include <cmath>
#include <optional>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "util/IncludeWin32.h"

#ifdef _WIN32
#include <shobjidl.h>
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Shell32.lib")
#endif

#include <QApplication>
#include <QBrush>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QInputDialog>
#include <QIntValidator>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QPushButton>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QScreen>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStringList>
#include <QUrl>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWindow>

#include "CPUInfo.h"
#include "parsers/palette/PaletteParser.h"
#include "parsers/palette/PaletteWriter.h"
#include "parsers/point/PointParser.h"
#include "parsers/point/PointWriter.h"
#include "parsers/sine/SineParser.h"
#include "parsers/sine/SineWriter.h"
#include "util/FormatUtil.h"
#include "util/NumberUtil.h"
#include "util/PathUtil.h"
#include "util/StringUtil.h"

namespace {
    constexpr auto kDefaultPaletteName = "default";
    constexpr auto kDefaultSineName = "default";
    constexpr auto kNewEntryLabel = "+ New";
    constexpr auto kUnsavedLabelSuffix = " (unsaved)";
    constexpr auto kNewColorPaletteBaseName = "New Palette";
    constexpr auto kNewSinePaletteBaseName = "New Sine";
    constexpr auto kCommittedLineEditTextProperty = "_mandelbrotCommittedText";
    constexpr double kMinGUIZoom = -3.2499;
    constexpr int kControlScrollBarWidth = 10;
    constexpr int kControlWindowScreenPadding = 48;

    constexpr std::array<double, 21> kZoomStepTable = {
        1.000625, 1.00125, 1.0025, 1.005, 1.01, 1.015, 1.02, 1.025, 1.03, 1.04,
        1.05, 1.06, 1.07, 1.08, 1.09, 1.10, 1.11, 1.12, 1.14, 1.17, 1.20
    };
    constexpr int kGridModes[] = { 0, 2, 3, 5, 9 };
    constexpr int kGridModeCount = sizeof(kGridModes) / sizeof(kGridModes[0]);

    struct PaletteStop {
        int id = 0;
        double pos = 0.0;
        QColor color;
    };

    struct SaveDialogResult {
        QString path;
        QString type;
        bool appendDate = false;
    };

    int clampSliderValue(int value) {
        return std::clamp(value, 0, 20);
    }

    QString committedLineEditText(const QLineEdit *edit) {
        return edit ? edit->text().trimmed() : QString();
    }

    void markLineEditTextCommitted(QLineEdit *edit) {
        if (!edit) return;
        edit->setProperty(kCommittedLineEditTextProperty, committedLineEditText(edit));
    }

    void setCommittedLineEditText(QLineEdit *edit, const QString &text) {
        if (!edit) return;
        edit->setText(text);
        markLineEditTextCommitted(edit);
    }

    bool hasUncommittedLineEditChange(const QLineEdit *edit) {
        if (!edit) return false;
        return committedLineEditText(edit) !=
            edit->property(kCommittedLineEditTextProperty).toString();
    }

    QString decorateUnsavedLabel(const QString &name, bool unsaved) {
        if (!unsaved) return name;
        return QString::fromStdString(StringUtil::appendSuffix(
            name.toStdString(),
            kUnsavedLabelSuffix));
    }

    QString undecoratedLabel(const QString &name) {
        return QString::fromStdString(StringUtil::stripSuffix(
            name.toStdString(),
            kUnsavedLabelSuffix));
    }

    QString fractalTypeToConfigString(Backend::FractalType fractalType) {
        switch (fractalType) {
            case Backend::FractalType::mandelbrot: return "mandelbrot";
            case Backend::FractalType::perpendicular: return "perpendicular";
            case Backend::FractalType::burningship: return "burningship";
        }
        return "mandelbrot";
    }

    Backend::FractalType fractalTypeFromConfigString(const QString &value) {
        const QString normalized = value.trimmed().toLower();
        if (normalized == "perpendicular") return Backend::FractalType::perpendicular;
        if (normalized == "burningship" || normalized == "burning_ship" ||
            normalized == "burning ship") {
            return Backend::FractalType::burningship;
        }
        return Backend::FractalType::mandelbrot;
    }

    double clampGUIZoom(double zoom) {
        return std::max(zoom, kMinGUIZoom);
    }

    QString formatImageMemoryText(const Backend::ImageEvent &event) {
        const QString renderBytes = QString::fromStdString(
            FormatUtil::formatBufferSize(event.primaryBytes));
        if (!event.downscaling) {
            return QString("Render: %1").arg(renderBytes);
        }

        const QString outputBytesText = QString::fromStdString(
            FormatUtil::formatBufferSize(event.secondaryBytes));
        return QString("Render: %1  Output: %2")
            .arg(renderBytes, outputBytesText);
    }

    QColor lightColorToQColor(const Backend::LightColor &color) {
        return QColor::fromRgbF(
            std::clamp(static_cast<double>(color.R), 0.0, 1.0),
            std::clamp(static_cast<double>(color.G), 0.0, 1.0),
            std::clamp(static_cast<double>(color.B), 0.0, 1.0));
    }

    Backend::LightColor lightColorFromQColor(const QColor &color) {
        return {
            .R = static_cast<float>(color.redF()),
            .G = static_cast<float>(color.greenF()),
            .B = static_cast<float>(color.blueF())
        };
    }

    QString lightColorButtonStyle(const QColor &color) {
        const int luminance =
            (color.red() * 299 + color.green() * 587 + color.blue() * 114) / 1000;
        const QColor fg = luminance >= 160 ? QColor(Qt::black) : QColor(Qt::white);

        return QString(
            "QPushButton {"
            " background-color: %1;"
            " color: %2;"
            "}").arg(color.name(QColor::HexRgb), fg.name(QColor::HexRgb));
    }

    Backend::PaletteHexConfig makeNewColorPaletteConfig() {
        Backend::PaletteHexConfig palette;
        palette.totalLength = 1.0f;
        palette.offset = 0.0f;
        palette.blendEnds = false;
        palette.entries = {
            { "#000000", 1.0f },
            { "#FFFFFF", 0.0f }
        };
        return palette;
    }

    Backend::SinePaletteConfig makeNewSinePaletteConfig() {
        return {
            .freqR = 1.0f,
            .freqG = 1.0f,
            .freqB = 1.0f,
            .freqMult = 1.0f
        };
    }

    bool sameSinePalette(const Backend::SinePaletteConfig &a,
        const Backend::SinePaletteConfig &b) {
        return NumberUtil::almostEqual(a.freqR, b.freqR) &&
            NumberUtil::almostEqual(a.freqG, b.freqG) &&
            NumberUtil::almostEqual(a.freqB, b.freqB) &&
            NumberUtil::almostEqual(a.freqMult, b.freqMult);
    }

    bool samePalette(const Backend::PaletteHexConfig &a,
        const Backend::PaletteHexConfig &b) {
        if (!NumberUtil::almostEqual(a.totalLength, b.totalLength) ||
            !NumberUtil::almostEqual(a.offset, b.offset) ||
            a.blendEnds != b.blendEnds ||
            a.entries.size() != b.entries.size()) {
            return false;
        }

        for (size_t i = 0; i < a.entries.size(); ++i) {
            if (!NumberUtil::almostEqual(a.entries[i].length, b.entries[i].length) ||
                QString::fromStdString(a.entries[i].color).compare(
                    QString::fromStdString(b.entries[i].color),
                    Qt::CaseInsensitive) != 0) {
                return false;
            }
        }

        return true;
    }

    std::filesystem::path sineDirectoryPath() {
        return PathUtil::executableDir() / "sines";
    }

    std::filesystem::path sineFilePath(const QString &name) {
        return sineDirectoryPath() /
            std::filesystem::path((name + ".txt").toStdString());
    }

    std::filesystem::path paletteDirectoryPath() {
        return PathUtil::executableDir() / "palettes";
    }

    std::filesystem::path paletteFilePath(const QString &name) {
        return paletteDirectoryPath() /
            std::filesystem::path((name + ".txt").toStdString());
    }

    std::filesystem::path viewDirectoryPath() {
        return PathUtil::executableDir() / "views";
    }

    std::filesystem::path savesDirectoryPath() {
        return PathUtil::executableDir() / "saves";
    }

    struct NativeDialogFilter {
        QString displayText;
        QString patternText;
    };

    std::vector<NativeDialogFilter> parseNativeDialogFilters(const QString &filters) {
        std::vector<NativeDialogFilter> parsed;
        const QStringList entries = filters.split(";;", Qt::SkipEmptyParts);
        for (const QString &entry : entries) {
            const int openParen = entry.indexOf('(');
            const int closeParen = entry.lastIndexOf(')');
            if (openParen < 0 || closeParen <= openParen) {
                parsed.push_back({ entry.trimmed(), "*" });
                continue;
            }

            parsed.push_back({
                entry.left(openParen).trimmed(),
                entry.mid(openParen + 1, closeParen - openParen - 1).trimmed()
                });
        }
        return parsed;
    }

    QString defaultExtensionFromPattern(const QString &patternText) {
        const QStringList patterns = patternText.split(' ', Qt::SkipEmptyParts);
        for (QString pattern : patterns) {
            pattern = pattern.trimmed();
            if (!pattern.startsWith("*.")) continue;
            pattern.remove(0, 2);
            if (!pattern.isEmpty()) return pattern;
        }
        return QString();
    }

    QString showNativeSaveFileDialog(QWidget *parent,
        const QString &title,
        const std::filesystem::path &directory,
        const QString &suggestedFileName,
        const QString &filters,
        QString *selectedFilter = nullptr) {
#ifdef _WIN32
        const std::vector<NativeDialogFilter> parsedFilters =
            parseNativeDialogFilters(filters);

        HRESULT initHr = CoInitializeEx(nullptr,
            COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        const bool shouldUninitialize = SUCCEEDED(initHr);

        IFileSaveDialog *dialog = nullptr;
        const HRESULT createHr = CoCreateInstance(CLSID_FileSaveDialog,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&dialog));
        if (FAILED(createHr) || !dialog) {
            if (shouldUninitialize) CoUninitialize();
            return QString();
        }

        dialog->SetTitle(reinterpret_cast<LPCWSTR>(title.utf16()));
        if (!suggestedFileName.isEmpty()) {
            dialog->SetFileName(reinterpret_cast<LPCWSTR>(suggestedFileName.utf16()));
        }

        std::vector<std::wstring> filterNames;
        std::vector<std::wstring> filterPatterns;
        std::vector<COMDLG_FILTERSPEC> filterSpecs;
        filterNames.reserve(parsedFilters.size());
        filterPatterns.reserve(parsedFilters.size());
        filterSpecs.reserve(parsedFilters.size());
        for (const NativeDialogFilter &filter : parsedFilters) {
            filterNames.push_back(filter.displayText.toStdWString());
            filterPatterns.push_back(filter.patternText.toStdWString());
            filterSpecs.push_back({
                filterNames.back().c_str(),
                filterPatterns.back().c_str()
                });
        }
        if (!filterSpecs.empty()) {
            dialog->SetFileTypes(static_cast<UINT>(filterSpecs.size()),
                filterSpecs.data());
            dialog->SetFileTypeIndex(1);

            const QString defaultExtension = defaultExtensionFromPattern(
                parsedFilters.front().patternText);
            if (!defaultExtension.isEmpty()) {
                dialog->SetDefaultExtension(
                    reinterpret_cast<LPCWSTR>(defaultExtension.utf16()));
            }
        }

        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options |
            FOS_FORCEFILESYSTEM |
            FOS_PATHMUSTEXIST |
            FOS_OVERWRITEPROMPT);

        IShellItem *folder = nullptr;
        const std::wstring dir = directory.wstring();
        if (SUCCEEDED(SHCreateItemFromParsingName(
            dir.c_str(),
            nullptr,
            IID_PPV_ARGS(&folder)))) {
            dialog->SetFolder(folder);
            folder->Release();
        }

        const HWND owner = parent
            ? reinterpret_cast<HWND>(parent->window()->winId())
            : nullptr;
        const HRESULT showHr = dialog->Show(owner);
        if (showHr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
            dialog->Release();
            if (shouldUninitialize) CoUninitialize();
            return QString();
        }
        if (FAILED(showHr)) {
            dialog->Release();
            if (shouldUninitialize) CoUninitialize();
            return QString();
        }

        if (selectedFilter && !parsedFilters.empty()) {
            UINT filterIndex = 1;
            dialog->GetFileTypeIndex(&filterIndex);
            const size_t index = filterIndex > 0 ? static_cast<size_t>(filterIndex - 1) : 0;
            *selectedFilter = index < parsedFilters.size()
                ? parsedFilters[index].displayText + " (" + parsedFilters[index].patternText + ")"
                : QString();
        }

        IShellItem *resultItem = nullptr;
        if (FAILED(dialog->GetResult(&resultItem)) || !resultItem) {
            dialog->Release();
            if (shouldUninitialize) CoUninitialize();
            return QString();
        }

        PWSTR selectedPath = nullptr;
        if (FAILED(resultItem->GetDisplayName(SIGDN_FILESYSPATH, &selectedPath)) ||
            !selectedPath) {
            resultItem->Release();
            dialog->Release();
            if (shouldUninitialize) CoUninitialize();
            return QString();
        }

        const QString result = QString::fromWCharArray(selectedPath);
        CoTaskMemFree(selectedPath);
        resultItem->Release();
        dialog->Release();
        if (shouldUninitialize) CoUninitialize();
        return result;
#else
        QFileDialog dialog(parent,
            title,
            QString::fromStdWString(directory.wstring()),
            filters);
        dialog.setAcceptMode(QFileDialog::AcceptSave);
        dialog.setFileMode(QFileDialog::AnyFile);
        dialog.setOption(QFileDialog::DontUseNativeDialog, false);
        if (!suggestedFileName.isEmpty()) {
            dialog.selectFile(suggestedFileName);
        }

        if (dialog.exec() != QDialog::Accepted) return QString();

        if (selectedFilter) {
            *selectedFilter = dialog.selectedNameFilter();
        }
        return dialog.selectedFiles().value(0);
#endif
    }

    QString showNativeOpenFileDialog(QWidget *parent,
        const QString &title,
        const std::filesystem::path &directory,
        const QString &filters) {
#ifdef _WIN32
        const std::vector<NativeDialogFilter> parsedFilters =
            parseNativeDialogFilters(filters);

        HRESULT initHr = CoInitializeEx(nullptr,
            COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        const bool shouldUninitialize = SUCCEEDED(initHr);

        IFileOpenDialog *dialog = nullptr;
        const HRESULT createHr = CoCreateInstance(CLSID_FileOpenDialog,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&dialog));
        if (FAILED(createHr) || !dialog) {
            if (shouldUninitialize) CoUninitialize();
            return QString();
        }

        dialog->SetTitle(reinterpret_cast<LPCWSTR>(title.utf16()));

        std::vector<std::wstring> filterNames;
        std::vector<std::wstring> filterPatterns;
        std::vector<COMDLG_FILTERSPEC> filterSpecs;
        filterNames.reserve(parsedFilters.size());
        filterPatterns.reserve(parsedFilters.size());
        filterSpecs.reserve(parsedFilters.size());
        for (const NativeDialogFilter &filter : parsedFilters) {
            filterNames.push_back(filter.displayText.toStdWString());
            filterPatterns.push_back(filter.patternText.toStdWString());
            filterSpecs.push_back({
                filterNames.back().c_str(),
                filterPatterns.back().c_str()
                });
        }
        if (!filterSpecs.empty()) {
            dialog->SetFileTypes(static_cast<UINT>(filterSpecs.size()),
                filterSpecs.data());
            dialog->SetFileTypeIndex(1);
        }

        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options |
            FOS_FORCEFILESYSTEM |
            FOS_FILEMUSTEXIST |
            FOS_PATHMUSTEXIST);

        IShellItem *folder = nullptr;
        const std::wstring dir = directory.wstring();
        if (SUCCEEDED(SHCreateItemFromParsingName(
            dir.c_str(),
            nullptr,
            IID_PPV_ARGS(&folder)))) {
            dialog->SetFolder(folder);
            folder->Release();
        }

        const HWND owner = parent
            ? reinterpret_cast<HWND>(parent->window()->winId())
            : nullptr;
        const HRESULT showHr = dialog->Show(owner);
        if (showHr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
            dialog->Release();
            if (shouldUninitialize) CoUninitialize();
            return QString();
        }
        if (FAILED(showHr)) {
            dialog->Release();
            if (shouldUninitialize) CoUninitialize();
            return QString();
        }

        IShellItem *resultItem = nullptr;
        if (FAILED(dialog->GetResult(&resultItem)) || !resultItem) {
            dialog->Release();
            if (shouldUninitialize) CoUninitialize();
            return QString();
        }

        PWSTR selectedPath = nullptr;
        if (FAILED(resultItem->GetDisplayName(SIGDN_FILESYSPATH, &selectedPath)) ||
            !selectedPath) {
            resultItem->Release();
            dialog->Release();
            if (shouldUninitialize) CoUninitialize();
            return QString();
        }

        const QString result = QString::fromWCharArray(selectedPath);
        CoTaskMemFree(selectedPath);
        resultItem->Release();
        dialog->Release();
        if (shouldUninitialize) CoUninitialize();
        return result;
#else
        QFileDialog dialog(parent,
            title,
            QString::fromStdWString(directory.wstring()),
            filters);
        dialog.setAcceptMode(QFileDialog::AcceptOpen);
        dialog.setFileMode(QFileDialog::ExistingFile);
        dialog.setOption(QFileDialog::DontUseNativeDialog, false);

        if (dialog.exec() != QDialog::Accepted) return QString();
        return dialog.selectedFiles().value(0);
#endif
    }

    int backendTypeRank(const QString &name) {
        const QString normalized = name.toLower();
        if (normalized.contains("scalar")) return 0;
        if (normalized.contains("avx") ||
            normalized.contains("sse") ||
            normalized.contains("vector")) {
            return 1;
        }
        if (normalized.contains("mpfr")) return 2;
        return 3;
    }

    int backendPrecisionRank(const QString &name) {
        const QString normalized = name.toLower();
        if (normalized.contains("float")) return 0;
        if (normalized.contains("double")) return 1;
        if (normalized.contains("qd")) return 2;
        if (normalized.contains("mpfr")) return 3;
        return 4;
    }

    QStringList listBackendNames(QString &errorMessage) {
        errorMessage.clear();
        QStringList names;

        std::error_code ec;
        const std::filesystem::path dir = PathUtil::executableDir() / "backends";
        if (!std::filesystem::exists(dir, ec) || ec) {
            errorMessage = "Backend directory was not found.";
            return names;
        }

        for (const auto &entry : std::filesystem::directory_iterator(dir, ec)) {
            if (ec) break;
            if (!entry.is_regular_file()) continue;

            const std::filesystem::path path = entry.path();
            if (path.extension() != ".dll") continue;
            const QString stem = QString::fromStdWString(path.stem().wstring());
            if (!stem.contains(" - ")) continue;
            names.push_back(stem);
        }

        names.removeDuplicates();
        std::sort(names.begin(), names.end(), [](const QString &a, const QString &b) {
            const int typeA = backendTypeRank(a);
            const int typeB = backendTypeRank(b);
            if (typeA != typeB) return typeA < typeB;

            const int precisionA = backendPrecisionRank(a);
            const int precisionB = backendPrecisionRank(b);
            if (precisionA != precisionB) return precisionA < precisionB;

            return a.compare(b, Qt::CaseInsensitive) < 0;
            });
        if (names.isEmpty()) {
            errorMessage = "No backend DLLs were found in the backends directory.";
        }

        return names;
    }

    bool ensurePaletteDirectory(QString &errorMessage) {
        errorMessage.clear();

        std::error_code ec;
        std::filesystem::create_directories(paletteDirectoryPath(), ec);
        if (!ec) return true;

        errorMessage = QString("Failed to create palette directory: %1")
            .arg(QString::fromStdString(ec.message()));
        return false;
    }

    QString normalizePaletteName(const QString &name) {
        return name.trimmed();
    }

    bool isValidPaletteName(const QString &name) {
        const QString trimmed = normalizePaletteName(name);
        if (trimmed.isEmpty()) return false;
        if (trimmed == "." || trimmed == "..") return false;

        for (const QChar ch : trimmed) {
            if (ch.unicode() > 0x7F) return false;
            if (ch.isLetterOrNumber()) continue;
            if (ch == ' ' || ch == '_' || ch == '-' || ch == '.') continue;
            return false;
        }

        return true;
    }

    QString sanitizePaletteName(const QString &name) {
        QString sanitized = normalizePaletteName(name);
        for (QChar &ch : sanitized) {
            const bool allowed = ch.unicode() <= 0x7F &&
                (ch.isLetterOrNumber() || ch == ' ' || ch == '_' ||
                    ch == '-' || ch == '.');
            if (!allowed) ch = '_';
        }

        sanitized = sanitized.simplified();
        if (sanitized.isEmpty() || sanitized == "." || sanitized == "..") {
            sanitized = "palette";
        }

        return sanitized;
    }

    QStringList listPaletteNames() {
        QStringList names;
        std::error_code ec;
        const std::filesystem::path dir = paletteDirectoryPath();
        if (!std::filesystem::exists(dir, ec) || ec) {
            return names;
        }

        for (const auto &entry : std::filesystem::directory_iterator(dir, ec)) {
            if (ec) break;
            if (!entry.is_regular_file()) continue;

            const std::filesystem::path path = entry.path();
            if (path.extension() != ".txt") continue;
            names.push_back(QString::fromStdWString(path.stem().wstring()));
        }

        names.removeDuplicates();
        std::sort(names.begin(), names.end(), [](const QString &a, const QString &b) {
            if (a.compare(kDefaultPaletteName, Qt::CaseInsensitive) == 0) return true;
            if (b.compare(kDefaultPaletteName, Qt::CaseInsensitive) == 0) return false;
            return a.compare(b, Qt::CaseInsensitive) < 0;
            });
        return names;
    }

    QString uniquePaletteName(const QString &baseName,
        const QStringList &existingNames) {
        const QString sanitized = sanitizePaletteName(baseName);
        if (!existingNames.contains(sanitized, Qt::CaseInsensitive)) {
            return sanitized;
        }

        for (int suffix = 2;; ++suffix) {
            const QString candidate = QString("%1 %2")
                .arg(sanitized)
                .arg(suffix);
            if (!existingNames.contains(candidate, Qt::CaseInsensitive)) {
                return candidate;
            }
        }
    }

    QString uniqueIndexedNameFromList(const QString &baseName,
        const QStringList &existingNames) {
        return QString::fromStdString(FormatUtil::uniqueIndexedName(
            baseName.toStdString(),
            [&existingNames](const std::string_view candidate) {
                return existingNames.contains(
                    QString::fromUtf8(candidate.data(),
                        static_cast<int>(candidate.size())),
                    Qt::CaseInsensitive);
            }));
    }

    QString uniqueIndexedPathWithExtension(const std::filesystem::path &directory,
        const QString &baseName,
        const QString &extension) {
        const std::string uniqueName = FormatUtil::uniqueIndexedName(
            baseName.toStdString(),
            [&directory, &extension](const std::string_view candidate) {
                std::error_code ec;
                const std::filesystem::path path = directory /
                    std::filesystem::path(
                        std::string(candidate) + "." + extension.toStdString());
                return std::filesystem::exists(path, ec) && !ec;
            });

        return QString::fromStdWString(
            (directory / std::filesystem::path(
                uniqueName + "." + extension.toStdString()))
            .wstring());
    }

    QStringList listSineNames() {
        QStringList names;
        std::error_code ec;
        const std::filesystem::path dir = sineDirectoryPath();
        if (!std::filesystem::exists(dir, ec) || ec) {
            return names;
        }

        for (const auto &entry : std::filesystem::directory_iterator(dir, ec)) {
            if (ec) break;
            if (!entry.is_regular_file()) continue;

            const std::filesystem::path path = entry.path();
            if (path.extension() != ".txt") continue;
            names.push_back(QString::fromStdWString(path.stem().wstring()));
        }

        names.removeDuplicates();
        std::sort(names.begin(), names.end(), [](const QString &a, const QString &b) {
            if (a.compare(kDefaultSineName, Qt::CaseInsensitive) == 0) return true;
            if (b.compare(kDefaultSineName, Qt::CaseInsensitive) == 0) return false;
            return a.compare(b, Qt::CaseInsensitive) < 0;
            });
        return names;
    }

    bool ensureSineDirectory(QString &errorMessage) {
        errorMessage.clear();

        std::error_code ec;
        std::filesystem::create_directories(sineDirectoryPath(), ec);
        if (!ec) return true;

        errorMessage = QString("Failed to create sine directory: %1")
            .arg(QString::fromStdString(ec.message()));
        return false;
    }

    bool loadSineFromPath(const std::filesystem::path &path,
        Backend::SinePaletteConfig &palette, QString &errorMessage) {
        SineParser parser("-");
        std::string err;
        if (parser.parse({ path.string() }, palette, err)) {
            errorMessage.clear();
            return true;
        }

        errorMessage = QString::fromStdString(err);
        return false;
    }

    bool saveSineToPath(const std::filesystem::path &path,
        const Backend::SinePaletteConfig &palette, QString &errorMessage) {
        SineWriter writer(palette);
        std::string err;
        if (writer.write(path.string(), err)) {
            errorMessage.clear();
            return true;
        }

        errorMessage = QString::fromStdString(err);
        return false;
    }

    bool loadPaletteFromPath(const std::filesystem::path &path,
        Backend::PaletteHexConfig &palette, QString &errorMessage) {
        PaletteParser parser("-");
        std::string err;
        if (parser.parse({ path.string() }, palette, err)) {
            errorMessage.clear();
            return true;
        }

        errorMessage = QString::fromStdString(err);
        return false;
    }

    bool savePaletteToPath(const std::filesystem::path &path,
        const Backend::PaletteHexConfig &palette, QString &errorMessage) {
        PaletteWriter writer(palette);
        std::string err;
        if (writer.write(path.string(), err)) {
            errorMessage.clear();
            return true;
        }

        errorMessage = QString::fromStdString(err);
        return false;
    }

    bool ensureViewDirectory(QString &errorMessage) {
        errorMessage.clear();

        std::error_code ec;
        std::filesystem::create_directories(viewDirectoryPath(), ec);
        if (!ec) return true;

        errorMessage = QString("Failed to create views directory: %1")
            .arg(QString::fromStdString(ec.message()));
        return false;
    }

    bool loadPointFromPath(const std::filesystem::path &path,
        PointConfig &point, QString &errorMessage) {
        PointParser parser;
        std::string err;
        if (parser.parse(path.string(), point, err)) {
            errorMessage.clear();
            return true;
        }

        errorMessage = QString::fromStdString(err);
        return false;
    }

    bool savePointToPath(const std::filesystem::path &path,
        const PointConfig &point, QString &errorMessage) {
        PointWriter writer(point);
        std::string err;
        if (writer.write(path.string(), err)) {
            errorMessage.clear();
            return true;
        }

        errorMessage = QString::fromStdString(err);
        return false;
    }

    void stabilizePushButton(QPushButton *button) {
        if (!button) return;
        button->setAutoDefault(false);
        button->setDefault(false);
    }

    void setLayoutItemsVisible(QLayout *layout, bool visible) {
        if (!layout) return;

        for (int i = 0; i < layout->count(); ++i) {
            QLayoutItem *item = layout->itemAt(i);
            if (!item) continue;

            if (QWidget *widget = item->widget()) {
                widget->setVisible(visible);
            } else if (QLayout *childLayout = item->layout()) {
                setLayoutItemsVisible(childLayout, visible);
            }
        }
    }

    void setLayoutItemsEnabled(QLayout *layout, bool enabled) {
        if (!layout) return;

        for (int i = 0; i < layout->count(); ++i) {
            QLayoutItem *item = layout->itemAt(i);
            if (!item) continue;

            if (QWidget *widget = item->widget()) {
                widget->setEnabled(enabled);
            } else if (QLayout *childLayout = item->layout()) {
                setLayoutItemsEnabled(childLayout, enabled);
            }
        }
    }

    class CollapsibleGroupBox final : public QGroupBox {
    public:
        explicit CollapsibleGroupBox(const QString &title,
            bool expanded = true,
            std::function<void()> onToggled = {},
            QWidget *parent = nullptr)
            : QGroupBox(title, parent),
            _onToggled(std::move(onToggled)) {
            setCheckable(true);
            setChecked(expanded);
            setFlat(true);

            QObject::connect(this, &QGroupBox::toggled, this, [this](bool checked) {
                applyExpandedState(checked);
                if (_onToggled) _onToggled();
                });
        }

        void applyExpandedState(bool expanded) {
            if (layout()) {
                setLayoutItemsVisible(layout(), expanded);
            }
        }

        void setContentEnabled(bool enabled) {
            if (layout()) {
                setLayoutItemsEnabled(layout(), enabled);
            }

            setEnabled(true);

            QPalette groupPalette = palette();
            const QPalette appPalette = QApplication::palette(this);
            groupPalette.setColor(
                QPalette::WindowText,
                appPalette.color(enabled ? QPalette::WindowText : QPalette::Mid));
            setPalette(groupPalette);
        }

    private:
        std::function<void()> _onToggled;
    };

    QImage wrapImageViewToImage(const Backend::ImageView &view) {
        if (!view.outputPixels || view.outputWidth <= 0 || view.outputHeight <= 0) {
            return {};
        }

        const int bytesPerLine = view.downscaling ?
            view.outputWidth * 3 : view.strideWidth;
        return QImage(view.outputPixels,
            view.outputWidth,
            view.outputHeight,
            bytesPerLine,
            QImage::Format_RGB888);
    }

    QImage imageViewToImage(const Backend::ImageView &view) {
        const QImage wrapped = wrapImageViewToImage(view);
        return wrapped.copy();
    }

    QImage makeBlankViewportImage() {
        QImage image(1, 1, QImage::Format_RGB888);
        image.fill(Qt::black);
        return image;
    }

    double effectiveDevicePixelRatio(const QWidget *widget) {
        if (!widget) return 1.0;

        if (const QWindow *window = widget->windowHandle()) {
            return std::max(1.0, window->devicePixelRatio());
        }
        if (const QScreen *screen = widget->screen()) {
            return std::max(1.0, screen->devicePixelRatio());
        }
        return std::max(1.0, widget->devicePixelRatioF());
    }

    class IterationSpinBox final : public QSpinBox {
    public:
        explicit IterationSpinBox(QWidget *parent = nullptr) :
            QSpinBox(parent) {
            setKeyboardTracking(false);
        }

        std::function<int()> resolveAutoIterations;

    protected:
        QString textFromValue(int value) const override {
            if (value <= minimum()) {
                return specialValueText();
            }

            return QSpinBox::textFromValue(value);
        }

        int valueFromText(const QString &text) const override {
            const QString trimmed = text.trimmed();
            if (trimmed.compare(specialValueText(), Qt::CaseInsensitive) == 0) {
                return minimum();
            }

            return QSpinBox::valueFromText(trimmed);
        }

        QValidator::State validate(QString &input, int &pos) const override {
            const QString trimmed = input.trimmed();
            const QString autoText = specialValueText();
            if (trimmed.isEmpty()) {
                return QValidator::Intermediate;
            }

            if (autoText.compare(trimmed, Qt::CaseInsensitive) == 0) {
                return QValidator::Acceptable;
            }

            if (autoText.startsWith(trimmed, Qt::CaseInsensitive)) {
                return QValidator::Intermediate;
            }

            return QSpinBox::validate(input, pos);
        }

        void fixup(QString &input) const override {
            if (input.trimmed().compare(specialValueText(), Qt::CaseInsensitive) == 0) {
                input = specialValueText();
                return;
            }

            QSpinBox::fixup(input);
        }

        void wheelEvent(QWheelEvent *event) override {
            const int steps = event->angleDelta().y() / 120;
            if (steps == 0) {
                QSpinBox::wheelEvent(event);
                return;
            }

            int nextValue = value();
            if (nextValue <= minimum()) {
                nextValue = std::clamp(
                    resolveAutoIterations ? resolveAutoIterations() : 1,
                    1,
                    maximum());
                setValue(nextValue);
                event->accept();
                return;
            }

            for (int i = 0; i < std::abs(steps); ++i) {
                if (steps > 0) {
                    nextValue = std::clamp(
                        nextValue > maximum() / 2 ? maximum() : nextValue * 2,
                        minimum(),
                        maximum());
                } else {
                    nextValue = std::clamp(nextValue / 2, 1, maximum());
                }
            }

            setValue(nextValue);
            event->accept();
        }
    };

    class AdaptiveDoubleSpinBox final : public QDoubleSpinBox {
    public:
        explicit AdaptiveDoubleSpinBox(int defaultDisplayDecimals, QWidget *parent = nullptr) :
            QDoubleSpinBox(parent),
            _defaultDisplayDecimals(std::max(0, defaultDisplayDecimals)),
            _displayDecimals(_defaultDisplayDecimals) {
            QDoubleSpinBox::setDecimals(17);
            setKeyboardTracking(false);

            if (QLineEdit *edit = lineEdit()) {
                QObject::connect(edit, &QLineEdit::textEdited,
                    this, [this](const QString &text) {
                        updateDisplayDecimals(text);
                    });
            }
        }

        void resetDisplayDecimals() {
            _displayDecimals = _defaultDisplayDecimals;
        }

    protected:
        QString textFromValue(double value) const override {
            return locale().toString(value, 'f', _displayDecimals);
        }

        void stepBy(int steps) override {
            _displayDecimals = _defaultDisplayDecimals;
            QDoubleSpinBox::stepBy(steps);
        }

    private:
        int _defaultDisplayDecimals;
        int _displayDecimals;

        void updateDisplayDecimals(const QString &text) {
            const QString decimalPoint = locale().decimalPoint();
            const int decimalIndex = text.indexOf(decimalPoint);
            if (decimalIndex < 0) {
                _displayDecimals = _defaultDisplayDecimals;
                return;
            }

            int typedDecimals = 0;
            for (int i = decimalIndex + 1; i < text.size(); ++i) {
                if (!text[i].isDigit()) {
                    break;
                }

                ++typedDecimals;
            }

            _displayDecimals = std::max(_defaultDisplayDecimals, typedDecimals);
        }
    };

    void setAdaptiveSpinValue(QDoubleSpinBox *spinBox, double value) {
        if (!spinBox) return;

        if (auto *adaptiveSpinBox = dynamic_cast<AdaptiveDoubleSpinBox *>(spinBox)) {
            adaptiveSpinBox->resetDisplayDecimals();
        }

        spinBox->setValue(value);
    }

    void resizeViewportToImageSize(QWidget *viewport, const QSize &imageSize) {
        if (!viewport || !imageSize.isValid()) return;

        const double dpr = effectiveDevicePixelRatio(viewport);
        const QSize logicalSize(
            std::max(1, static_cast<int>(std::lround(
                static_cast<double>(imageSize.width()) / dpr))),
            std::max(1, static_cast<int>(std::lround(
                static_cast<double>(imageSize.height()) / dpr))));

        viewport->showNormal();
        viewport->setMinimumSize(logicalSize);
        viewport->setMaximumSize(logicalSize);
        viewport->resize(logicalSize);
    }

    double paletteSegmentLengthSum(const Backend::PaletteHexConfig &palette) {
        if (palette.entries.empty()) return 0.0;

        const size_t segmentCount = palette.blendEnds ?
            palette.entries.size() :
            palette.entries.size() - 1;

        double total = 0.0;
        for (size_t i = 0; i < segmentCount; ++i) {
            total += std::max(0.0f, palette.entries[i].length);
        }

        return total;
    }

    QColor lerpPreviewColor(const QColor &from, const QColor &to, double t) {
        const double clamped = std::clamp(t, 0.0, 1.0);
        return QColor(
            static_cast<int>(std::lround(from.red() +
                (to.red() - from.red()) * clamped)),
            static_cast<int>(std::lround(from.green() +
                (to.green() - from.green()) * clamped)),
            static_cast<int>(std::lround(from.blue() +
                (to.blue() - from.blue()) * clamped)));
    }

    QColor samplePalettePreviewColor(
        const Backend::PaletteHexConfig &palette,
        double x
    ) {
        if (palette.entries.empty()) return Qt::black;

        const size_t colorCount = palette.entries.size();
        size_t segmentCount = colorCount;
        if (!palette.blendEnds) {
            if (colorCount < 2) {
                return QColor(QString::fromStdString(palette.entries.front().color));
            }
            segmentCount--;
        }

        if (segmentCount == 0) {
            return QColor(QString::fromStdString(palette.entries.front().color));
        }

        std::vector<QColor> colors;
        colors.reserve(colorCount);
        for (const auto &entry : palette.entries) {
            QColor color(QString::fromStdString(entry.color));
            colors.push_back(color.isValid() ? color : QColor(Qt::black));
        }

        std::vector<double> accum(segmentCount + 1, 0.0);
        std::vector<double> spans(segmentCount, 0.0);
        const double totalLength =
            std::max(1.0, static_cast<double>(palette.totalLength));
        const double lengthSum = paletteSegmentLengthSum(palette);
        const double defaultSpan = totalLength / static_cast<double>(segmentCount);

        for (size_t i = 0; i < segmentCount; ++i) {
            const double rawLength = std::max(0.0,
                static_cast<double>(palette.entries[i].length));
            spans[i] = lengthSum > 0.0
                ? rawLength / lengthSum * totalLength
                : defaultSpan;
            accum[i + 1] = accum[i] + spans[i];
        }

        const double wrappedTotal = palette.blendEnds
            ? totalLength
            : std::max(accum.back(), 1.0e-12);
        const double offset = std::clamp(
            static_cast<double>(palette.offset),
            0.0,
            wrappedTotal);

        double t = std::fmod(x + offset, wrappedTotal);
        if (t < 0.0) t += wrappedTotal;
        if (t >= wrappedTotal) {
            t = std::nextafter(wrappedTotal, 0.0);
        }

        size_t idx = 0;
        while (idx + 1 < segmentCount && accum[idx + 1] <= t) {
            idx++;
        }

        const double span = spans[idx];
        const double u = span > 0.0 ? (t - accum[idx]) / span : 0.0;

        size_t next = idx + 1;
        if (palette.blendEnds) {
            next %= colors.size();
        } else if (next >= colors.size()) {
            next = colors.size() - 1;
        }

        return lerpPreviewColor(colors[idx], colors[next], u);
    }

    QImage makePalettePreviewImage(
        const Backend::PaletteHexConfig &palette,
        int width,
        int height
    ) {
        if (width <= 0 || height <= 0) return {};

        QImage image(width, height, QImage::Format_RGB32);
        const double totalLength =
            std::max(1.0, static_cast<double>(palette.totalLength));
        const double denom = width > 1 ? static_cast<double>(width - 1) : 1.0;

        for (int x = 0; x < width; ++x) {
            const QColor color = samplePalettePreviewColor(
                palette,
                totalLength * static_cast<double>(x) / denom);
            const QRgb pixel = color.rgb();

            for (int y = 0; y < height; ++y) {
                image.setPixel(x, y, pixel);
            }
        }

        return image;
    }

    std::vector<PaletteStop> configToStops(const Backend::PaletteHexConfig &palette) {
        std::vector<PaletteStop> stops;
        if (palette.entries.empty()) return stops;

        double accum = 0.0;
        const double segmentSum = paletteSegmentLengthSum(palette);
        const bool useEqualSpacing = segmentSum <= 0.0;
        const size_t stopCount = palette.entries.size();
        int nextId = 1;

        if (!palette.blendEnds && palette.entries.size() >= 2) {
            for (size_t i = 0; i + 1 < palette.entries.size(); ++i) {
                const auto &entry = palette.entries[i];
                stops.push_back({
                    nextId++,
                    useEqualSpacing ?
                        static_cast<double>(i) /
                            std::max<size_t>(1, palette.entries.size() - 1) :
                        std::clamp(accum / segmentSum, 0.0, 1.0),
                    QColor(QString::fromStdString(entry.color))
                    });
                accum += std::max(0.0f, entry.length);
            }

            stops.push_back({
                nextId++,
                1.0,
                QColor(QString::fromStdString(palette.entries.back().color))
                });
            return stops;
        }

        for (const auto &entry : palette.entries) {
            stops.push_back({
                nextId++,
                useEqualSpacing ?
                    static_cast<double>(stops.size()) / std::max<size_t>(1, stopCount) :
                    std::clamp(accum / segmentSum, 0.0, 1.0),
                QColor(QString::fromStdString(entry.color))
                });
            accum += std::max(0.0f, entry.length);
        }

        if (stops.size() == 1) {
            stops.push_back({ nextId++, 1.0, stops.front().color });
        }

        return stops;
    }

    Backend::PaletteHexConfig stopsToConfig(const std::vector<PaletteStop> &stops,
        float totalLength, float offset, bool blendEnds) {
        constexpr double kLoopEndpointEpsilon = 1e-4;
        Backend::PaletteHexConfig palette;
        palette.totalLength = totalLength;
        palette.offset = offset;
        palette.blendEnds = blendEnds;

        if (stops.size() < 2) {
            const auto fallback = makeNewColorPaletteConfig();
            palette.entries = fallback.entries;
            return palette;
        }

        std::vector<PaletteStop> sorted = stops;
        std::sort(sorted.begin(), sorted.end(),
            [](const PaletteStop &a, const PaletteStop &b) {
                return a.pos < b.pos;
            });

        if (blendEnds && sorted.size() >= 2 && sorted.back().pos >= 1.0) {
            const double minPos = std::min(
                1.0 - kLoopEndpointEpsilon,
                sorted[sorted.size() - 2].pos + kLoopEndpointEpsilon);
            sorted.back().pos = minPos;
        }

        if (!blendEnds) {
            for (size_t i = 0; i + 1 < sorted.size(); ++i) {
                const double start = std::clamp(sorted[i].pos, 0.0, 1.0);
                const double end = std::clamp(sorted[i + 1].pos, 0.0, 1.0);
                const double len = std::max((end - start) * totalLength, 0.0001);

                palette.entries.push_back({
                    sorted[i].color.name(QColor::HexRgb).toStdString(),
                    static_cast<float>(len)
                    });
            }

            palette.entries.push_back({
                sorted.back().color.name(QColor::HexRgb).toStdString(),
                0.0f
                });
            return palette;
        }

        for (size_t i = 0; i < sorted.size(); ++i) {
            const size_t next = (i + 1) % sorted.size();
            double len = sorted[next].pos - sorted[i].pos;
            if (next == 0) len += 1.0;
            len = std::max(len * totalLength, 0.0001);

            palette.entries.push_back({
                sorted[i].color.name(QColor::HexRgb).toStdString(),
                static_cast<float>(len)
                });
        }

        return palette;
    }

    std::optional<SaveDialogResult> runSaveDialog(
        QWidget *parent,
        const QString &suggestedFile
    ) {
        std::error_code ec;
        std::filesystem::create_directories(savesDirectoryPath(), ec);

        SaveDialogResult result;
#ifdef _WIN32
        HRESULT initHr = CoInitializeEx(nullptr,
            COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        const bool shouldUninitialize = SUCCEEDED(initHr);

        IFileSaveDialog *dialog = nullptr;
        const HRESULT createHr = CoCreateInstance(CLSID_FileSaveDialog,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&dialog));
        if (FAILED(createHr) || !dialog) {
            if (shouldUninitialize) CoUninitialize();
            return std::nullopt;
        }

        dialog->SetTitle(L"Save Image");
        dialog->SetFileName(
            reinterpret_cast<LPCWSTR>(suggestedFile.utf16()));

        const COMDLG_FILTERSPEC filters[] = {
            { L"PNG Files (*.png)", L"*.png" },
            { L"JPEG Files (*.jpg;*.jpeg)", L"*.jpg;*.jpeg" },
            { L"Bitmap Files (*.bmp)", L"*.bmp" }
        };
        dialog->SetFileTypes(static_cast<UINT>(std::size(filters)), filters);
        dialog->SetFileTypeIndex(1);
        dialog->SetDefaultExtension(L"png");

        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options |
            FOS_FORCEFILESYSTEM |
            FOS_PATHMUSTEXIST |
            FOS_OVERWRITEPROMPT);

        IShellItem *folder = nullptr;
        const std::wstring savesDir = savesDirectoryPath().wstring();
        if (SUCCEEDED(SHCreateItemFromParsingName(
            savesDir.c_str(),
            nullptr,
            IID_PPV_ARGS(&folder)))) {
            dialog->SetFolder(folder);
            folder->Release();
        }

        IFileDialogCustomize *customize = nullptr;
        bool appendDate = true;
        if (SUCCEEDED(dialog->QueryInterface(IID_PPV_ARGS(&customize))) &&
            customize) {
            customize->AddCheckButton(1, L"Append Date", TRUE);
        }

        const HWND owner = parent
            ? reinterpret_cast<HWND>(parent->window()->winId())
            : nullptr;
        const HRESULT showHr = dialog->Show(owner);
        if (showHr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
            if (customize) customize->Release();
            dialog->Release();
            if (shouldUninitialize) CoUninitialize();
            return std::nullopt;
        }

        if (FAILED(showHr)) {
            if (customize) customize->Release();
            dialog->Release();
            if (shouldUninitialize) CoUninitialize();
            return std::nullopt;
        }

        UINT filterIndex = 1;
        dialog->GetFileTypeIndex(&filterIndex);

        if (customize) {
            BOOL checked = TRUE;
            if (SUCCEEDED(customize->GetCheckButtonState(1, &checked))) {
                appendDate = checked == TRUE;
            }
            customize->Release();
        }

        IShellItem *selectedItem = nullptr;
        if (FAILED(dialog->GetResult(&selectedItem)) || !selectedItem) {
            dialog->Release();
            if (shouldUninitialize) CoUninitialize();
            return std::nullopt;
        }

        PWSTR selectedPath = nullptr;
        if (FAILED(selectedItem->GetDisplayName(SIGDN_FILESYSPATH, &selectedPath)) ||
            !selectedPath) {
            selectedItem->Release();
            dialog->Release();
            if (shouldUninitialize) CoUninitialize();
            return std::nullopt;
        }

        result.path = QString::fromWCharArray(selectedPath);
        result.appendDate = appendDate;
        if (filterIndex == 2) {
            result.type = "jpg";
        } else if (filterIndex == 3) {
            result.type = "bmp";
        } else {
            result.type = "png";
        }

        CoTaskMemFree(selectedPath);
        selectedItem->Release();
        dialog->Release();
        if (shouldUninitialize) CoUninitialize();
#else
        QString selectedFilter;
        result.path = showNativeSaveFileDialog(parent,
            "Save Image",
            savesDirectoryPath(),
            suggestedFile,
            "PNG Files (*.png);;JPEG Files (*.jpg *.jpeg);;Bitmap Files (*.bmp)",
            &selectedFilter);
        if (result.path.isEmpty()) return std::nullopt;

        const QMessageBox::StandardButton appendDateChoice = QMessageBox::question(
            parent,
            "Save Image",
            "Append date to filename?",
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
            QMessageBox::Yes);
        if (appendDateChoice == QMessageBox::Cancel) return std::nullopt;
        result.appendDate = appendDateChoice == QMessageBox::Yes;

        const QString filter = selectedFilter.toLower();
        if (filter.contains("*.jpg") || filter.contains("*.jpeg")) {
            result.type = "jpg";
        } else if (filter.contains("*.bmp")) {
            result.type = "bmp";
        } else {
            result.type = "png";
        }
#endif

        result.path = QString::fromStdString(PathUtil::appendExtension(
            result.path.toStdString(),
            result.type.toStdString()));
        return result;
    }

    class SinePreviewWidget final : public QWidget {
    public:
        explicit SinePreviewWidget(QWidget *parent = nullptr)
            : QWidget(parent) {
            setFixedHeight(48);
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            setMouseTracking(true);
            setToolTip("Drag the middle to pan the range. Drag the edges to resize it.");
        }

        std::function<void(double, double)> onRangeChanged;

        [[nodiscard]] std::pair<double, double> range() const {
            return { _rangeMin, _rangeMax };
        }

        void setPreviewImage(const QImage &image) {
            _previewImage = image;
            update();
        }

        void resetRange(double minValue, double maxValue) {
            _rangeMin = minValue;
            _rangeMax = std::max(minValue + kMinSpan, maxValue);
            if (onRangeChanged) onRangeChanged(_rangeMin, _rangeMax);
            update();
        }

    protected:
        void paintEvent(QPaintEvent *) override {
            QPainter painter(this);
            painter.fillRect(rect(), palette().window());

            const QRect strip = previewRect();
            if (strip.width() <= 0 || strip.height() <= 0) return;
            const bool enabled = isEnabled();

            if (_previewImage.isNull()) {
                painter.fillRect(strip, palette().base());
            } else {
                if (enabled) {
                    painter.drawImage(strip, _previewImage);
                } else {
                    const QImage disabledImage = _previewImage
                        .convertToFormat(QImage::Format_Grayscale8)
                        .convertToFormat(QImage::Format_RGB32);
                    painter.drawImage(strip, disabledImage);
                    painter.fillRect(strip, QColor(255, 255, 255, 90));
                }
            }
            painter.setPen(enabled ? palette().dark().color() : palette().mid().color());
            painter.drawRect(strip.adjusted(0, 0, -1, -1));

            painter.setPen(QPen(
                enabled ? QColor(255, 255, 255, 180) : palette().mid().color(),
                1.0));
            painter.drawLine(strip.left(), strip.top(), strip.left(), strip.bottom());
            painter.drawLine(strip.right(), strip.top(), strip.right(), strip.bottom());

            drawHandle(painter, strip.left(), strip);
            drawHandle(painter, strip.right(), strip);

            painter.setPen(enabled ? palette().text().color() : palette().mid().color());
            painter.drawText(
                rect().adjusted(2, strip.bottom() + 3, -2, -2),
                Qt::AlignLeft | Qt::AlignVCenter,
                QString("Range %1 - %2")
                .arg(QString::number(_rangeMin, 'f', 2))
                .arg(QString::number(_rangeMax, 'f', 2)));
        }

        void mousePressEvent(QMouseEvent *event) override {
            if (event->button() == Qt::RightButton) {
                const QRect strip = previewRect();
                if (strip.contains(event->position().toPoint())) {
                    resetRange(kDefaultMin, kDefaultMax);
                }
                return;
            }

            if (event->button() != Qt::LeftButton) return;

            const QRect strip = previewRect();
            if (!strip.contains(event->position().toPoint())) return;

            _dragMode = hitTest(event->position().toPoint(), strip);
            _dragStartX = event->position().x();
            _dragRangeMin = _rangeMin;
            _dragRangeMax = _rangeMax;
            updateCursor(_dragMode);
        }

        void mouseMoveEvent(QMouseEvent *event) override {
            const QRect strip = previewRect();
            if (_dragMode == DragMode::none) {
                updateCursor(hitTest(event->position().toPoint(), strip));
                return;
            }

            if (strip.width() <= 0) return;

            const double span = std::max(kMinSpan, _dragRangeMax - _dragRangeMin);
            const double deltaValue =
                (_dragStartX - event->position().x()) / strip.width() * span;

            switch (_dragMode) {
                case DragMode::pan:
                {
                    applyRange(
                        _dragRangeMin + deltaValue,
                        _dragRangeMax + deltaValue);
                    break;
                }
                case DragMode::left:
                    applyRange(
                        std::min(_dragRangeMin + deltaValue,
                            _dragRangeMax - kMinSpan),
                        _dragRangeMax);
                    break;
                case DragMode::right:
                    applyRange(
                        _dragRangeMin,
                        std::max(_dragRangeMin + kMinSpan, _dragRangeMax + deltaValue));
                    break;
                case DragMode::none:
                    break;
            }
        }

        void mouseReleaseEvent(QMouseEvent *) override {
            _dragMode = DragMode::none;
            updateCursor(DragMode::none);
        }

        void leaveEvent(QEvent *) override {
            if (_dragMode == DragMode::none) {
                unsetCursor();
            }
        }

    private:
        enum class DragMode {
            none,
            left,
            right,
            pan
        };

        static constexpr double kDefaultMin = 0.0;
        static constexpr double kDefaultMax = 100.0;
        static constexpr double kMinSpan = 1.0;
        static constexpr int kHandleHitWidth = 10;
        double _rangeMin = kDefaultMin;
        double _rangeMax = kDefaultMax;
        double _dragStartX = 0.0;
        double _dragRangeMin = kDefaultMin;
        double _dragRangeMax = kDefaultMax;
        DragMode _dragMode = DragMode::none;
        QImage _previewImage;

        QRect previewRect() const {
            return rect().adjusted(2, 2, -2, -16);
        }

        DragMode hitTest(const QPoint &point, const QRect &strip) const {
            if (!strip.contains(point)) return DragMode::none;
            if (std::abs(point.x() - strip.left()) <= kHandleHitWidth) return DragMode::left;
            if (std::abs(point.x() - strip.right()) <= kHandleHitWidth) return DragMode::right;
            return DragMode::pan;
        }

        void drawHandle(QPainter &painter, int x, const QRect &strip) const {
            const QRect handle(x - 2, strip.top() + 4, 4, std::max(6, strip.height() - 8));
            painter.fillRect(handle,
                isEnabled() ? QColor(255, 255, 255, 180) : palette().mid().color());
        }

        void updateCursor(DragMode mode) {
            switch (mode) {
                case DragMode::left:
                case DragMode::right:
                    setCursor(Qt::SizeHorCursor);
                    break;
                case DragMode::pan:
                    setCursor(Qt::OpenHandCursor);
                    break;
                case DragMode::none:
                    unsetCursor();
                    break;
            }
        }

        void applyRange(double minValue, double maxValue) {
            const double clampedMin = minValue;
            const double clampedMax = std::max(clampedMin + kMinSpan, maxValue);
            if (qFuzzyCompare(_rangeMin, clampedMin) &&
                qFuzzyCompare(_rangeMax, clampedMax)) {
                return;
            }

            _rangeMin = clampedMin;
            _rangeMax = clampedMax;
            if (onRangeChanged) onRangeChanged(_rangeMin, _rangeMax);
            update();
        }
    };

    class PalettePreviewWidget final : public QWidget {
    public:
        explicit PalettePreviewWidget(QWidget *parent = nullptr)
            : QWidget(parent) {
            setFixedHeight(30);
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        }

        void setPreviewPixmap(const QPixmap &pixmap) {
            _previewPixmap = pixmap;
            update();
        }

        void clearPreview() {
            _previewPixmap = {};
            update();
        }

        [[nodiscard]] QSize sizeHint() const override {
            return { 0, 30 };
        }

        [[nodiscard]] QSize minimumSizeHint() const override {
            return { 0, 30 };
        }

    protected:
        void paintEvent(QPaintEvent *) override {
            QPainter painter(this);
            const QRect frameRect = rect().adjusted(0, 0, -1, -1);
            painter.fillRect(rect(), palette().base());

            if (!_previewPixmap.isNull()) {
                const QPixmap scaled = _previewPixmap.scaled(
                    frameRect.size(),
                    Qt::IgnoreAspectRatio,
                    Qt::SmoothTransformation);
                if (isEnabled()) {
                    painter.drawPixmap(frameRect.topLeft(), scaled);
                } else {
                    const QImage disabledImage = scaled.toImage()
                        .convertToFormat(QImage::Format_Grayscale8)
                        .convertToFormat(QImage::Format_RGB32);
                    painter.drawImage(frameRect, disabledImage);
                    painter.fillRect(frameRect, QColor(255, 255, 255, 90));
                }
            }

            painter.setPen(isEnabled() ? palette().dark().color() : palette().mid().color());
            painter.drawRect(frameRect);
        }

    private:
        QPixmap _previewPixmap;
    };

    class PaletteTimelineWidget final : public QWidget {
    public:
        explicit PaletteTimelineWidget(QWidget *parent = nullptr)
            : QWidget(parent) {
            setMinimumHeight(0);
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            setMouseTracking(true);

            _positionEditor = new QLineEdit(this);
            _positionEditor->setFixedWidth(54);
            _positionEditor->setAlignment(Qt::AlignCenter);
            _positionEditor->setValidator(new QIntValidator(0, 100, _positionEditor));
            _positionEditor->hide();

            QObject::connect(_positionEditor, &QLineEdit::editingFinished,
                this, [this]() { commitPositionEdit(); });
        }

        std::function<void()> onChanged;
        std::function<void(int)> onSelectionChanged;
        std::function<void(int)> onEditColorRequested;

        void setBlendEnds(bool blendEnds) {
            _blendEnds = blendEnds;
            normalizeStopLayout();
            updatePositionEditor();
            update();
        }

        void setStops(const std::vector<PaletteStop> &stops) {
            _stops = stops;
            if (_stops.empty()) {
                _selectedIds.clear();
                _primarySelectedId.reset();
            } else if (!_primarySelectedId || !_selectedIds.contains(*_primarySelectedId)) {
                _setSingleSelection(_stops.front().id);
            }

            _sortStops();
            normalizeStopLayout();
            updatePositionEditor();
            update();
        }

        const std::vector<PaletteStop> &stops() const {
            return _stops;
        }

        int selectedIndex() const {
            if (!_primarySelectedId) return -1;

            for (int i = 0; i < static_cast<int>(_stops.size()); ++i) {
                if (_stops[i].id == *_primarySelectedId) return i;
            }

            return -1;
        }

        int selectedCount() const {
            return static_cast<int>(_selectedIds.size());
        }

        void setSelectedColor(const QColor &color) {
            if (_selectedIds.empty()) return;

            for (auto &stop : _stops) {
                if (_selectedIds.contains(stop.id)) {
                    stop.color = color;
                }
            }
            emitChanged();
            update();
        }

        void addStop() {
            _sortStops();
            normalizeStopLayout();

            PaletteStop stop;
            stop.id = ++_nextId;

            if (_stops.size() < 2) {
                stop.pos = 0.5;
            } else {
                double bestStart = 0.0,
                    bestEnd = _blendEnds ? _stops.front().pos + 1.0 : 1.0,
                    bestGap = -1.0;

                for (size_t i = 1; i < _stops.size(); ++i) {
                    const double start = _stops[i - 1].pos;
                    const double end = _stops[i].pos;
                    const double gap = end - start;
                    if (gap > bestGap) {
                        bestGap = gap;
                        bestStart = start;
                        bestEnd = end;
                    }
                }

                if (_blendEnds && _stops.size() >= 2) {
                    const double wrapStart = _stops.back().pos;
                    const double wrapEnd = _stops.front().pos + 1.0;
                    const double wrapGap = wrapEnd - wrapStart;
                    if (wrapGap > bestGap) {
                        bestGap = wrapGap;
                        bestStart = wrapStart;
                        bestEnd = wrapEnd;
                    }
                }

                stop.pos = (bestStart + bestEnd) * 0.5;
                if (stop.pos >= 1.0) stop.pos -= 1.0;
            }

            stop.color = sample(stop.pos);
            _stops.push_back(stop);
            _setSingleSelection(stop.id);
            _sortStops();
            normalizeStopLayout();
            emitSelectionChanged();
            emitChanged();
            update();
        }

        void removeSelectedStops() {
            if (_selectedIds.empty()) return;
            if (static_cast<int>(_stops.size()) - selectedCount() < 2) return;

            _stops.erase(std::remove_if(_stops.begin(), _stops.end(),
                [this](const PaletteStop &stop) {
                    return _selectedIds.contains(stop.id);
                }),
                _stops.end());

            _selectedIds.clear();
            if (_stops.empty()) {
                _primarySelectedId.reset();
            } else {
                _primarySelectedId = _stops.front().id;
                _selectedIds.insert(*_primarySelectedId);
            }

            normalizeStopLayout();
            emitSelectionChanged();
            emitChanged();
            update();
        }

        void evenSpacing() {
            if (_stops.size() < 2) return;

            _sortStops();
            for (size_t i = 0; i < _stops.size(); ++i) {
                if (_blendEnds) {
                    _stops[i].pos = static_cast<double>(i) /
                        static_cast<double>(_stops.size());
                } else {
                    const double denom = static_cast<double>(_stops.size() - 1);
                    _stops[i].pos = denom > 0.0 ? static_cast<double>(i) / denom : 0.0;
                }
            }

            normalizeStopLayout();
            emitChanged();
            update();
        }

    protected:
        void resizeEvent(QResizeEvent *event) override {
            QWidget::resizeEvent(event);
            updatePositionEditor();
        }

        void paintEvent(QPaintEvent *) override {
            QPainter painter(this);
            painter.fillRect(rect(), palette().window());

            const QRect grad = gradientRect();
            QLinearGradient gradient(grad.topLeft(), grad.topRight());
            for (const auto &stop : _stops) {
                gradient.setColorAt(std::clamp(stop.pos, 0.0, 1.0), stop.color);
            }
            if (!_stops.empty()) {
                gradient.setColorAt(1.0, _blendEnds ?
                    _stops.front().color :
                    _stops.back().color);
            }

            painter.fillRect(grad, gradient);
            painter.setPen(palette().dark().color());
            painter.drawRect(grad.adjusted(0, 0, -1, -1));

            for (const auto &stop : _stops) {
                const QPoint handlePos = stopPoint(stop.pos);
                QPainterPath path;
                path.moveTo(handlePos.x(), grad.bottom() + 4);
                path.lineTo(handlePos.x() - 6, grad.bottom() + 16);
                path.lineTo(handlePos.x() + 6, grad.bottom() + 16);
                path.closeSubpath();

                painter.fillPath(path, stop.color);
                painter.setPen(_selectedIds.contains(stop.id) ?
                    QPen(Qt::black, 1.5) :
                    QPen(palette().mid().color(), 1.0));
                painter.drawPath(path);
            }
        }

        void mousePressEvent(QMouseEvent *event) override {
            if (event->button() == Qt::LeftButton) {
                const int hit = hitTest(event->position().toPoint());
                const bool toggleSelection =
                    event->modifiers().testFlag(Qt::ControlModifier);

                if (hit >= 0) {
                    const int id = _stops[hit].id;
                    if (toggleSelection) {
                        if (_selectedIds.contains(id) && _selectedIds.size() > 1) {
                            _selectedIds.erase(id);
                            if (_primarySelectedId == id) {
                                _primarySelectedId = *_selectedIds.begin();
                            }
                        } else {
                            _selectedIds.insert(id);
                            _primarySelectedId = id;
                        }
                    } else if (!_selectedIds.contains(id) || _selectedIds.size() != 1) {
                        _setSingleSelection(id);
                    }

                    _draggingId.reset();
                    _dragStartPositions.clear();
                    if (!isLockedId(id)) {
                        _draggingId = id;
                        _dragAnchorPos = posFromPixel(event->position().x());
                        for (const auto &stop : _stops) {
                            if (_selectedIds.contains(stop.id) && !isLockedId(stop.id)) {
                                _dragStartPositions[stop.id] = stop.pos;
                            }
                        }
                    }
                    emitSelectionChanged();
                } else if (gradientRect().contains(event->position().toPoint())) {
                    PaletteStop stop;
                    stop.id = ++_nextId;
                    stop.pos = posFromPixel(event->position().x());
                    stop.color = sample(stop.pos);

                    _stops.push_back(stop);
                    _setSingleSelection(stop.id);
                    _draggingId = stop.id;
                    _dragAnchorPos = stop.pos;
                    _dragStartPositions = { { stop.id, stop.pos } };
                    _sortStops();
                    normalizeStopLayout();
                    emitSelectionChanged();
                    emitChanged();
                } else if (!toggleSelection) {
                    _selectedIds.clear();
                    _primarySelectedId.reset();
                    emitSelectionChanged();
                }

                update();
            }

            if (event->button() == Qt::RightButton) {
                const int hit = hitTest(event->position().toPoint());
                if (hit >= 0) {
                    if (!_selectedIds.contains(_stops[hit].id)) {
                        _setSingleSelection(_stops[hit].id);
                    }
                    removeSelectedStops();
                }
            }
        }

        void mouseMoveEvent(QMouseEvent *event) override {
            if (!_draggingId) return;
            if (_dragStartPositions.empty()) return;

            const double targetPos = posFromPixel(event->position().x());
            double delta = targetPos - _dragAnchorPos,
                minDelta = -1.0,
                maxDelta = 1.0;
            for (const auto &[id, pos] : _dragStartPositions) {
                (void)id;
                minDelta = std::max(minDelta, -pos);
                maxDelta = std::min(maxDelta, 1.0 - pos);
            }

            if (minDelta > maxDelta) {
                delta = minDelta;
            } else {
                delta = std::clamp(delta, minDelta, maxDelta);
            }

            for (auto &stop : _stops) {
                const auto it = _dragStartPositions.find(stop.id);
                if (it != _dragStartPositions.end()) {
                    stop.pos = it->second + delta;
                }
            }

            _sortStops();
            emitChanged();
            updatePositionEditor();
            update();
        }

        void mouseReleaseEvent(QMouseEvent *) override {
            _draggingId.reset();
            _dragStartPositions.clear();
        }

        void mouseDoubleClickEvent(QMouseEvent *event) override {
            const int hit = hitTest(event->position().toPoint());
            if (hit < 0) return;

            _setSingleSelection(_stops[hit].id);
            emitSelectionChanged();
            if (onEditColorRequested) onEditColorRequested(hit);
            update();
        }

    private:
        QRect gradientRect() const {
            return rect().adjusted(12, 12, -12, -28);
        }

        QPoint stopPoint(double pos) const {
            const QRect grad = gradientRect();
            const int x = grad.left() +
                static_cast<int>(std::round(pos * grad.width()));

            return {
                std::clamp(x, grad.left(), grad.right()),
                grad.bottom()
            };
        }

        double posFromPixel(double x) const {
            const QRect grad = gradientRect();
            if (grad.width() <= 0) return 0.0;

            return std::clamp(
                (x - grad.left()) / grad.width(),
                0.0,
                1.0
            );
        }

        QColor sample(double pos) const {
            if (_stops.empty()) return Qt::white;

            std::vector<PaletteStop> sorted = _stops;
            std::sort(sorted.begin(), sorted.end(),
                [](const PaletteStop &a, const PaletteStop &b) {
                    return a.pos < b.pos;
                });

            if (_blendEnds && sorted.size() >= 2) {
                if (pos < sorted.front().pos || pos > sorted.back().pos) {
                    const double start = sorted.back().pos;
                    const double end = sorted.front().pos + 1.0;
                    const double samplePos = pos < sorted.front().pos ? pos + 1.0 : pos;
                    const double span = std::max(end - start, 1e-6);
                    const double t = (samplePos - start) / span;

                    return QColor::fromRgbF(
                        sorted.back().color.redF() +
                        (sorted.front().color.redF() -
                            sorted.back().color.redF()) * t,
                        sorted.back().color.greenF() +
                        (sorted.front().color.greenF() -
                            sorted.back().color.greenF()) * t,
                        sorted.back().color.blueF() +
                        (sorted.front().color.blueF() -
                            sorted.back().color.blueF()) * t
                    );
                }
            } else {
                if (pos <= sorted.front().pos) return sorted.front().color;
                if (pos >= sorted.back().pos) return sorted.back().color;
            }

            for (size_t i = 0; i + 1 < sorted.size(); ++i) {
                if (pos < sorted[i].pos || pos > sorted[i + 1].pos) continue;

                const double span = std::max(
                    sorted[i + 1].pos - sorted[i].pos,
                    1e-6
                );
                const double t = (pos - sorted[i].pos) / span;

                return QColor::fromRgbF(
                    sorted[i].color.redF() +
                    (sorted[i + 1].color.redF() - sorted[i].color.redF()) * t,
                    sorted[i].color.greenF() +
                    (sorted[i + 1].color.greenF() - sorted[i].color.greenF()) * t,
                    sorted[i].color.blueF() +
                    (sorted[i + 1].color.blueF() - sorted[i].color.blueF()) * t
                );
            }

            return sorted.back().color;
        }

        int hitTest(const QPoint &point) const {
            for (int i = 0; i < static_cast<int>(_stops.size()); ++i) {
                const QRect area(stopPoint(_stops[i].pos) - QPoint(8, 6),
                    QSize(16, 18));
                if (area.contains(point)) return i;
            }

            return -1;
        }

        void _sortStops() {
            std::sort(_stops.begin(), _stops.end(),
                [](const PaletteStop &a, const PaletteStop &b) {
                    return a.pos < b.pos;
                });
        }

        bool isLockedIndex(int index) const {
            if (index < 0 || index >= static_cast<int>(_stops.size())) return false;
            if (index == 0) return true;
            if (!_blendEnds && index == static_cast<int>(_stops.size()) - 1) return true;
            return false;
        }

        bool isLockedId(int id) const {
            for (int i = 0; i < static_cast<int>(_stops.size()); ++i) {
                if (_stops[i].id == id) return isLockedIndex(i);
            }
            return false;
        }

        double minimumStopSpacing() const {
            return 0.0;
        }

        void normalizeStopLayout() {
            if (_stops.empty()) return;

            _sortStops();
            _stops.front().pos = 0.0;

            const double minGap = minimumStopSpacing();
            const int firstMovable = 1;
            const int lastMovable = _blendEnds ?
                static_cast<int>(_stops.size()) - 1 :
                static_cast<int>(_stops.size()) - 2;

            if (!_blendEnds && _stops.size() >= 2) {
                _stops.back().pos = 1.0;
            }

            if (lastMovable >= firstMovable) {
                for (int i = firstMovable; i <= lastMovable; ++i) {
                    _stops[i].pos = std::max(
                        _stops[i].pos,
                        _stops[i - 1].pos + minGap);
                }

                const double upperBound = _blendEnds ? 1.0 - minGap : 1.0 - minGap;
                if (_stops[lastMovable].pos > upperBound) {
                    _stops[lastMovable].pos = upperBound;
                    for (int i = lastMovable - 1; i >= firstMovable; --i) {
                        _stops[i].pos = std::min(
                            _stops[i].pos,
                            _stops[i + 1].pos - minGap);
                    }
                }

                if (_stops[firstMovable].pos < minGap) {
                    const int movableCount = lastMovable - firstMovable + 1;
                    if (movableCount > 0) {
                        const double start = minGap;
                        const double end = _blendEnds ? 1.0 - minGap : 1.0 - minGap;
                        const double denom = movableCount + (_blendEnds ? 1.0 : 0.0);
                        for (int i = 0; i < movableCount; ++i) {
                            _stops[firstMovable + i].pos =
                                start + (end - start) *
                                static_cast<double>(i + 1) / std::max(1.0, denom);
                        }
                    }
                }
            }

            if (!_blendEnds && _stops.size() >= 2) {
                for (int i = 1; i + 1 < static_cast<int>(_stops.size()); ++i) {
                    const double minPos = _stops[i - 1].pos + minGap;
                    const double maxPos = _stops[i + 1].pos - minGap;
                    if (minPos > maxPos) {
                        _stops[i].pos = minPos;
                    } else {
                        _stops[i].pos = std::clamp(_stops[i].pos, minPos, maxPos);
                    }
                }
                _stops.back().pos = 1.0;
            }
        }

        void updatePositionEditor() {
            if (!_positionEditor) return;
            if (_selectedIds.size() != 1) {
                _positionEditor->hide();
                return;
            }

            const int index = selectedIndex();
            if (index < 0 || index >= static_cast<int>(_stops.size())) {
                _positionEditor->hide();
                return;
            }

            const QString text = QString::number(
                static_cast<int>(std::lround(_stops[index].pos * 100.0)));
            if (!_positionEditor->hasFocus()) {
                _positionEditor->setText(text);
            }

            const QRect grad = gradientRect();
            const QPoint handle = stopPoint(_stops[index].pos);
            const int x = std::clamp(handle.x() - _positionEditor->width() / 2,
                grad.left(),
                std::max(grad.left(), grad.right() - _positionEditor->width() + 1));
            const int y = std::max(0, grad.top() - _positionEditor->height() - 6);
            _positionEditor->move(x, y);
            _positionEditor->setReadOnly(isLockedIndex(index));
            _positionEditor->show();
            _positionEditor->raise();
        }

        void commitPositionEdit() {
            if (!_positionEditor || !_positionEditor->isVisible()) return;

            const int index = selectedIndex();
            if (index < 0 || index >= static_cast<int>(_stops.size())) return;
            if (isLockedIndex(index)) {
                updatePositionEditor();
                return;
            }

            bool ok = false;
            const int entered = _positionEditor->text().toInt(&ok);
            if (!ok) {
                updatePositionEditor();
                return;
            }

            double pos = std::clamp(static_cast<double>(entered) / 100.0, 0.0, 1.0);
            _stops[index].pos = pos;
            _sortStops();
            normalizeStopLayout();
            emitChanged();
            emitSelectionChanged();
            update();
        }

        void _setSingleSelection(int id) {
            _selectedIds.clear();
            _selectedIds.insert(id);
            _primarySelectedId = id;
        }

        void emitChanged() {
            if (onChanged) onChanged();
        }

        void emitSelectionChanged() {
            updatePositionEditor();
            if (onSelectionChanged) onSelectionChanged(selectedIndex());
        }

        bool _blendEnds = true;
        std::vector<PaletteStop> _stops;
        int _nextId = 100;
        std::optional<int> _primarySelectedId;
        std::unordered_set<int> _selectedIds;
        std::optional<int> _draggingId;
        double _dragAnchorPos = 0.0;
        std::unordered_map<int, double> _dragStartPositions;
        QLineEdit *_positionEditor = nullptr;
    };

    class PaletteEditorDialog final : public QDialog {
    public:
        PaletteEditorDialog(const Backend::PaletteHexConfig &palette,
            const QString &paletteName,
            std::function<void(const QString &)> onSavedPath,
            QWidget *parent = nullptr)
            : QDialog(parent),
            _palette(palette),
            _savedPaletteName(normalizePaletteName(paletteName)),
            _onSavedPath(std::move(onSavedPath)) {
            setWindowTitle("Palette");
            setModal(true);
            resize(420, 200);

            auto *root = new QVBoxLayout(this);

            auto *nameLayout = new QFormLayout();
            _nameEdit = new QLineEdit(_savedPaletteName.isEmpty() ?
                kDefaultPaletteName : _savedPaletteName);
            _nameEdit->setMaxLength(64);
            _nameEdit->setValidator(new QRegularExpressionValidator(
                QRegularExpression("[A-Za-z0-9 _.\\-]*"),
                _nameEdit));
            _nameEdit->setPlaceholderText("ASCII palette name");
            nameLayout->addRow("Name", _nameEdit);
            root->addLayout(nameLayout);

            _timeline = new PaletteTimelineWidget();
            root->addWidget(_timeline, 1);

            auto *optionsRow = new QHBoxLayout();
            _blendEndsCheck = new QCheckBox("Blend Ends");
            _blendEndsCheck->setChecked(_palette.blendEnds);
            optionsRow->addWidget(_blendEndsCheck);
            optionsRow->addStretch(1);
            root->addLayout(optionsRow);

            auto *buttonRow = new QHBoxLayout();
            _addButton = new QPushButton("+");
            _removeButton = new QPushButton("-");
            _evenButton = new QPushButton("Even");
            _colorButton = new QPushButton("Color");
            _importButton = new QPushButton("Import");
            _saveButton = new QPushButton("Save");
            stabilizePushButton(_addButton);
            stabilizePushButton(_removeButton);
            stabilizePushButton(_evenButton);
            stabilizePushButton(_colorButton);
            stabilizePushButton(_importButton);
            stabilizePushButton(_saveButton);
            _addButton->setFixedWidth(34);
            _removeButton->setFixedWidth(34);
            buttonRow->addWidget(_addButton);
            buttonRow->addWidget(_removeButton);
            buttonRow->addWidget(_evenButton);
            buttonRow->addWidget(_colorButton);
            buttonRow->addStretch(1);
            buttonRow->addWidget(_importButton);
            buttonRow->addWidget(_saveButton);
            root->addLayout(buttonRow);

            auto *dialogButtons = new QDialogButtonBox(
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
            root->addWidget(dialogButtons);

            applyPalette(_palette);
            _timeline->onChanged = [this]() {
                _palette = currentPalette();
                _savedPaletteDirty = true;
                };
            _timeline->onSelectionChanged = [this](int index) {
                _colorButton->setEnabled(index >= 0);
                _removeButton->setEnabled(_timeline->selectedCount() > 0 &&
                    static_cast<int>(_timeline->stops().size()) -
                    _timeline->selectedCount() >= 2);
                };
            _timeline->onEditColorRequested = [this](int) {
                editColor();
                };

            QObject::connect(_addButton, &QPushButton::clicked, this, [this]() {
                _timeline->addStop();
                });
            QObject::connect(_removeButton, &QPushButton::clicked, this, [this]() {
                _timeline->removeSelectedStops();
                });
            QObject::connect(_evenButton, &QPushButton::clicked, this, [this]() {
                _timeline->evenSpacing();
                });
            QObject::connect(_colorButton, &QPushButton::clicked, this, [this]() {
                editColor();
                });
            QObject::connect(_blendEndsCheck, &QCheckBox::toggled, this, [this](bool checked) {
                _palette.blendEnds = checked;
                _timeline->setBlendEnds(checked);
                _palette = currentPalette();
                _savedPaletteDirty = true;
                });
            QObject::connect(_nameEdit, &QLineEdit::textEdited, this, [this](const QString &) {
                _savedPaletteDirty = true;
                });
            QObject::connect(_importButton, &QPushButton::clicked, this, [this]() {
                importPalette();
                });
            QObject::connect(_saveButton, &QPushButton::clicked, this, [this]() {
                savePalette();
                });
            QObject::connect(dialogButtons, &QDialogButtonBox::accepted,
                this, &QDialog::accept);
            QObject::connect(dialogButtons, &QDialogButtonBox::rejected,
                this, &QDialog::reject);

            _colorButton->setEnabled(_timeline->selectedIndex() >= 0);
            _removeButton->setEnabled(_timeline->selectedCount() > 0 &&
                static_cast<int>(_timeline->stops().size()) -
                _timeline->selectedCount() >= 2);
        }

        Backend::PaletteHexConfig palette() const {
            return currentPalette();
        }

        QString savedPaletteName() const {
            return _savedPaletteName;
        }

        bool savedStateDirty() const {
            return _savedPaletteDirty;
        }

    private:
        Backend::PaletteHexConfig currentPalette() const {
            if (!_timeline) return _palette;

            return stopsToConfig(
                _timeline->stops(),
                _palette.totalLength,
                _palette.offset,
                _blendEndsCheck && _blendEndsCheck->isChecked()
            );
        }

        void applyPalette(const Backend::PaletteHexConfig &palette) {
            _palette = palette;
            if (_blendEndsCheck) {
                const QSignalBlocker blocker(_blendEndsCheck);
                _blendEndsCheck->setChecked(_palette.blendEnds);
            }
            if (_timeline) {
                _timeline->setBlendEnds(_palette.blendEnds);
                _timeline->setStops(configToStops(_palette));
            }
        }

        QString validatedPaletteName() const {
            const QString name = normalizePaletteName(
                _nameEdit ? _nameEdit->text() : QString());
            return isValidPaletteName(name) ? name : QString();
        }

        void editColor() {
            const int index = _timeline->selectedIndex();
            if (index < 0) return;

            const QColor initial = _timeline->stops()[index].color;
            const QColor color = QColorDialog::getColor(initial, this, "Color");
            if (!color.isValid()) return;

            _timeline->setSelectedColor(color);
        }

        void importPalette() {
            const QString sourcePath = showNativeOpenFileDialog(
                this,
                "Import Palette",
                paletteDirectoryPath(),
                "Palette Files (*.txt);;All Files (*.*)"
            );
            if (sourcePath.isEmpty()) return;

            Backend::PaletteHexConfig loaded;
            QString errorMessage;
            if (!loadPaletteFromPath(sourcePath.toStdString(), loaded, errorMessage)) {
                QMessageBox::warning(this, "Palette", errorMessage);
                return;
            }

            loaded.totalLength = _palette.totalLength;
            loaded.offset = _palette.offset;

            if (!ensurePaletteDirectory(errorMessage)) {
                QMessageBox::warning(this, "Palette", errorMessage);
                return;
            }

            const QStringList existingNames = listPaletteNames();
            const QString importedName = uniquePaletteName(
                QFileInfo(sourcePath).completeBaseName(),
                existingNames);
            const std::filesystem::path destinationPath = paletteFilePath(importedName);
            if (!savePaletteToPath(destinationPath, loaded, errorMessage)) {
                QMessageBox::warning(this, "Palette", errorMessage);
                return;
            }

            _savedPaletteName = importedName;
            if (_nameEdit) _nameEdit->setText(importedName);
            applyPalette(loaded);
            _savedPaletteDirty = false;
            if (_onSavedPath) {
                _onSavedPath(QString::fromStdWString(destinationPath.wstring()));
            }
        }

        void savePalette() {
            const QString paletteName = validatedPaletteName();
            if (paletteName.isEmpty()) {
                QMessageBox::warning(this, "Palette",
                    "Palette names must be non-empty ASCII and use only letters, numbers, spaces, '.', '-', or '_'.");
                return;
            }

            QString errorMessage;
            if (!ensurePaletteDirectory(errorMessage)) {
                QMessageBox::warning(this, "Palette", errorMessage);
                return;
            }

            _palette = currentPalette();
            QString targetName = paletteName;
            std::filesystem::path destinationPath = paletteFilePath(targetName);
            std::error_code existsError;
            const bool destinationExists =
                std::filesystem::exists(destinationPath, existsError) && !existsError;
            if (destinationExists) {
                const QMessageBox::StandardButton choice = QMessageBox::question(
                    this,
                    "Palette",
                    QString("Palette \"%1\" already exists. Overwrite it?")
                    .arg(targetName),
                    QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                    QMessageBox::Yes);
                if (choice == QMessageBox::Cancel) return;
                if (choice == QMessageBox::No) {
                    const QString savePath = showNativeSaveFileDialog(
                        this,
                        "Save Palette As",
                        paletteDirectoryPath(),
                        QFileInfo(uniqueIndexedPathWithExtension(
                            paletteDirectoryPath(),
                            targetName,
                            "txt")).fileName(),
                        "Palette Files (*.txt);;All Files (*.*)");
                    if (savePath.isEmpty()) return;

                    const QString savePathWithExtension = QString::fromStdString(
                        PathUtil::appendExtension(
                            savePath.toStdString(),
                            "txt"));
                    const QString saveName = normalizePaletteName(
                        QFileInfo(savePathWithExtension).completeBaseName());
                    if (!isValidPaletteName(saveName)) {
                        QMessageBox::warning(this, "Palette",
                            "Use an ASCII name with letters, numbers, spaces, ., _, or -.");
                        return;
                    }

                    targetName = saveName;
                    destinationPath = paletteFilePath(targetName);
                }
            }

            if (!savePaletteToPath(destinationPath, _palette, errorMessage)) {
                QMessageBox::warning(this, "Palette", errorMessage);
                return;
            }

            _savedPaletteName = targetName;
            if (_nameEdit) _nameEdit->setText(_savedPaletteName);
            _savedPaletteDirty = false;
            if (_onSavedPath) {
                _onSavedPath(QString::fromStdWString(destinationPath.wstring()));
            }
        }

        Backend::PaletteHexConfig _palette;
        QString _savedPaletteName;
        PaletteTimelineWidget *_timeline = nullptr;
        QLineEdit *_nameEdit = nullptr;
        QCheckBox *_blendEndsCheck = nullptr;
        QPushButton *_addButton = nullptr;
        QPushButton *_removeButton = nullptr;
        QPushButton *_evenButton = nullptr;
        QPushButton *_colorButton = nullptr;
        QPushButton *_importButton = nullptr;
        QPushButton *_saveButton = nullptr;
        bool _savedPaletteDirty = false;
        std::function<void(const QString &)> _onSavedPath;
    };

    class ViewportWindow final : public QWidget {
    public:
        friend class ::mandelbrotgui;

        explicit ViewportWindow(mandelbrotgui *owner)
            : QWidget(nullptr), _owner(owner) {
            updateWindowTitle();
            setMouseTracking(true);
            setFocusPolicy(Qt::StrongFocus);

            const int interactionTickIntervalMs =
                _owner ? _owner->interactionFrameIntervalMs() : 16;
            _rtZoomTimer.setTimerType(Qt::PreciseTimer);
            _rtZoomTimer.setInterval(interactionTickIntervalMs);
            QObject::connect(&_rtZoomTimer, &QTimer::timeout, this, [this]() {
                syncRealtimeZoomTimerInterval();
                applyRealtimeZoomStep(false);
                });

            _panRedrawTimer.setTimerType(Qt::PreciseTimer);
            _panRedrawTimer.setSingleShot(true);
            _panRedrawTimer.setInterval(interactionTickIntervalMs);
            QObject::connect(&_panRedrawTimer, &QTimer::timeout, this, [this]() {
                if (!_panning) return;

                if (_owner && _owner->renderInFlight()) {
                    _panRedrawTimer.start();
                    return;
                }

                commitPanOffset();
                if (_panning) {
                    syncPanRedrawTimerInterval();
                    _panRedrawTimer.start();
                }
                });

            _zoomOutRedrawTimer.setTimerType(Qt::PreciseTimer);
            _zoomOutRedrawTimer.setSingleShot(true);
            _zoomOutRedrawTimer.setInterval(interactionTickIntervalMs);
            QObject::connect(&_zoomOutRedrawTimer, &QTimer::timeout, this, [this]() {
                if (!_zoomOutDragActive) return;

                if (_owner && _owner->renderInFlight()) {
                    _zoomOutRedrawTimer.start();
                    return;
                }

                commitZoomOutPreview();
                if (_zoomOutDragActive && _zoomOutPendingCommit) {
                    _zoomOutRedrawTimer.start();
                }
                });

            _arrowPanTimer.setTimerType(Qt::PreciseTimer);
            _arrowPanTimer.setInterval(interactionTickIntervalMs);
            QObject::connect(&_arrowPanTimer, &QTimer::timeout, this, [this]() {
                syncArrowPanTimerInterval();
                applyArrowPanStep();
                });

            _fullscreenResizeTimer.setTimerType(Qt::PreciseTimer);
            _fullscreenResizeTimer.setSingleShot(true);
            _fullscreenResizeTimer.setInterval(120);
            QObject::connect(&_fullscreenResizeTimer, &QTimer::timeout, this, [this]() {
                if (_fullscreenTransitionPending && _owner) {
                    _owner->finalizeViewportFullscreenTransition();
                }
                });
        }

        void clearPreviewOffset() {
            const bool panNeedsClear = !_panning && !_panOffset.isNull();
            const bool zoomNeedsClear =
                !_zoomOutDragActive &&
                !NumberUtil::almostEqual(_zoomOutPreviewScale, 1.0);
            if (!panNeedsClear && !zoomNeedsClear && !_zoomOutPendingCommit) {
                return;
            }

            if (!_panning) {
                _panOffset = {};
            }
            if (!_zoomOutDragActive) {
                _zoomOutPending = false;
                _zoomOutDragMoved = false;
                _zoomOutPendingCommit = false;
                _zoomOutPreviewScale = 1.0;
            }
            update();
        }

        struct PreviewTransform {
            QPointF center;
            QPointF translation;
            double scaleX = 1.0;
            double scaleY = 1.0;
        };

        [[nodiscard]] mandelbrotgui::ViewTextState displayedPreviewView() const {
            if (!_owner || !_owner->hasDisplayedViewState()) {
                return {};
            }

            return _owner->displayedViewTextState();
        }

        [[nodiscard]] mandelbrotgui::ViewTextState targetPreviewView() const {
            if (!_owner) {
                return {};
            }

            mandelbrotgui::ViewTextState view = _owner->currentViewTextState();
            if (!view.valid) return view;

            if (_panning && !_panOffset.isNull()) {
                QString error;
                mandelbrotgui::ViewTextState pannedView;
                if (_owner->previewPannedViewState(
                    mapToOutputDelta(_panOffset), pannedView, error)) {
                    view = pannedView;
                } else {
                    view.valid = false;
                    return view;
                }
            }

            if (!NumberUtil::almostEqual(_zoomOutPreviewScale, 1.0)) {
                QString error;
                mandelbrotgui::ViewTextState scaledView;
                const QPoint outputCenter = mapToOutputPixel(
                    QPoint(width() / 2, height() / 2));
                if (_owner->previewScaledViewState(
                    outputCenter, _zoomOutPreviewScale, scaledView, error)) {
                    view = scaledView;
                } else {
                    view.valid = false;
                    return view;
                }
            }

            return view;
        }

        [[nodiscard]] std::optional<PreviewTransform> previewTransform() const {
            const QSize logicalSize = size();
            if (!_owner || logicalSize.width() <= 0 || logicalSize.height() <= 0) {
                return std::nullopt;
            }

            const mandelbrotgui::ViewTextState sourceView = displayedPreviewView();
            const mandelbrotgui::ViewTextState targetView = targetPreviewView();
            if (!sourceView.valid || !targetView.valid) {
                return std::nullopt;
            }

            if (sourceView.outputSize.width() <= 0 ||
                sourceView.outputSize.height() <= 0 ||
                targetView.outputSize.width() <= 0 ||
                targetView.outputSize.height() <= 0) {
                return std::nullopt;
            }

            const int targetMidX = std::clamp(
                targetView.outputSize.width() / 2,
                0,
                std::max(0, targetView.outputSize.width() - 1));
            const int targetMidY = std::clamp(
                targetView.outputSize.height() / 2,
                0,
                std::max(0, targetView.outputSize.height() - 1));

            QPointF sourceLeft;
            QPointF sourceRight;
            QPointF sourceTop;
            QPointF sourceBottom;
            QString error;
            if (!_owner->mapViewPixelToViewPixel(
                sourceView, targetView,
                QPoint(0, targetMidY), sourceLeft, error) ||
                !_owner->mapViewPixelToViewPixel(
                    sourceView, targetView,
                    QPoint(std::max(0, targetView.outputSize.width() - 1), targetMidY),
                    sourceRight, error) ||
                !_owner->mapViewPixelToViewPixel(
                    sourceView, targetView,
                    QPoint(targetMidX, 0), sourceTop, error) ||
                !_owner->mapViewPixelToViewPixel(
                    sourceView, targetView,
                    QPoint(targetMidX, std::max(0, targetView.outputSize.height() - 1)),
                    sourceBottom, error)) {
                return std::nullopt;
            }

            const auto sourceToLogical = [&](const QPointF &pixel) {
                return QPointF(
                    pixel.x() * logicalSize.width() /
                        std::max(1, sourceView.outputSize.width()),
                    pixel.y() * logicalSize.height() /
                        std::max(1, sourceView.outputSize.height()));
            };
            const auto targetToLogical = [&](const QPoint &pixel) {
                return QPointF(
                    static_cast<double>(pixel.x()) * logicalSize.width() /
                        std::max(1, targetView.outputSize.width()),
                    static_cast<double>(pixel.y()) * logicalSize.height() /
                        std::max(1, targetView.outputSize.height()));
            };

            const QPointF sourceLeftLogical = sourceToLogical(sourceLeft);
            const QPointF sourceRightLogical = sourceToLogical(sourceRight);
            const QPointF sourceTopLogical = sourceToLogical(sourceTop);
            const QPointF sourceBottomLogical = sourceToLogical(sourceBottom);
            const QPointF targetLeftLogical = targetToLogical(QPoint(0, targetMidY));
            const QPointF targetRightLogical = targetToLogical(
                QPoint(std::max(0, targetView.outputSize.width() - 1), targetMidY));
            const QPointF targetTopLogical = targetToLogical(QPoint(targetMidX, 0));
            const QPointF targetBottomLogical = targetToLogical(
                QPoint(targetMidX, std::max(0, targetView.outputSize.height() - 1)));

            const double sourceSpanX = sourceRightLogical.x() - sourceLeftLogical.x();
            const double sourceSpanY = sourceBottomLogical.y() - sourceTopLogical.y();
            if (NumberUtil::almostEqual(sourceSpanX, 0.0, 1e-9) ||
                NumberUtil::almostEqual(sourceSpanY, 0.0, 1e-9)) {
                return std::nullopt;
            }

            const QPointF center(
                static_cast<double>(logicalSize.width()) / 2.0,
                static_cast<double>(logicalSize.height()) / 2.0);
            const double scaleX =
                (targetRightLogical.x() - targetLeftLogical.x()) / sourceSpanX;
            const double scaleY =
                (targetBottomLogical.y() - targetTopLogical.y()) / sourceSpanY;
            const double translationXLeft =
                targetLeftLogical.x() - center.x() -
                scaleX * (sourceLeftLogical.x() - center.x());
            const double translationXRight =
                targetRightLogical.x() - center.x() -
                scaleX * (sourceRightLogical.x() - center.x());
            const double translationYTop =
                targetTopLogical.y() - center.y() -
                scaleY * (sourceTopLogical.y() - center.y());
            const double translationYBottom =
                targetBottomLogical.y() - center.y() -
                scaleY * (sourceBottomLogical.y() - center.y());
            const QPointF translation(
                (translationXLeft + translationXRight) * 0.5,
                (translationYTop + translationYBottom) * 0.5);
            if (!std::isfinite(translation.x()) || !std::isfinite(translation.y()) ||
                !std::isfinite(scaleX) || !std::isfinite(scaleY) ||
                !(scaleX > 0.0) || !(scaleY > 0.0)) {
                return std::nullopt;
            }

            const bool active =
                !NumberUtil::almostEqual(scaleX, 1.0, 1e-6) ||
                !NumberUtil::almostEqual(scaleY, 1.0, 1e-6) ||
                !NumberUtil::almostEqual(translation.x(), 0.0, 1e-3) ||
                !NumberUtil::almostEqual(translation.y(), 0.0, 1e-3);
            if (!active) {
                return std::nullopt;
            }

            return PreviewTransform{ center, translation, scaleX, scaleY };
        }

    protected:
        void paintEvent(QPaintEvent *) override {
            updateWindowTitle();
            QPainter painter(this);
            painter.fillRect(rect(), Qt::black);

            const QImage &image = _owner->previewImage();
            if (!image.isNull()) {
                const std::optional<PreviewTransform> transform =
                    usePreviewFallback() ? previewTransform() : std::nullopt;
                const bool transformedPreview = transform.has_value();
                const QImage drawImage = (transformedPreview &&
                    _owner->previewUsesBackendMemory()) ?
                    image.copy() :
                    image;
                if (transform) {
                    painter.save();
                    painter.translate(transform->translation);
                    painter.translate(transform->center);
                    painter.scale(transform->scaleX, transform->scaleY);
                    painter.translate(-transform->center);
                    painter.drawImage(rect(), drawImage);
                    painter.restore();
                } else {
                    painter.drawImage(rect(), image);
                }
            }

            if (_minimalUI) {
                return;
            }

            painter.setRenderHint(QPainter::Antialiasing, true);
            drawGrid(painter);

            if (!_selectionRect.isNull()) {
                painter.setPen(QPen(QColor(255, 255, 255, 180),
                    1.0, Qt::DashLine));
                painter.drawRect(_selectionRect.normalized());
            }

            const QString statusText = _owner->viewportStatusText();
            constexpr int overlayMargin = 8;
            constexpr int overlayPaddingX = 8;
            constexpr int overlayPaddingY = 6;
            const QFontMetrics metrics = painter.fontMetrics();
            const QString maxPrecisionMouseLine =
                "Mouse: 999999, 999999  |  "
                "-1.7976931348623157e+308  "
                "-1.7976931348623157e+308";
            int overlayTextWidth = metrics.horizontalAdvance(maxPrecisionMouseLine);
            for (const QString &line : statusText.split('\n')) {
                overlayTextWidth = std::max(overlayTextWidth,
                    metrics.horizontalAdvance(line));
            }
            const int overlayWidth = std::min(
                std::max(1, width() - overlayMargin * 2),
                overlayTextWidth + overlayPaddingX * 2 + 2);
            const QRect textRect(
                overlayMargin + overlayPaddingX,
                overlayMargin + overlayPaddingY,
                std::max(1, overlayWidth - overlayPaddingX * 2),
                std::max(1, height() - (overlayMargin + overlayPaddingY) * 2)
            );
            const QRect textBounds = metrics.boundingRect(
                textRect,
                Qt::AlignTop | Qt::AlignLeft,
                statusText
            );
            const QRect overlayRect(
                overlayMargin,
                overlayMargin,
                overlayWidth,
                textBounds.height() + overlayPaddingY * 2
            );

            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(80, 80, 80, 150));
            painter.drawRoundedRect(overlayRect, 8.0, 8.0);

            painter.setPen(QColor(230, 230, 230));
            painter.drawText(overlayRect.adjusted(
                overlayPaddingX,
                overlayPaddingY,
                -overlayPaddingX,
                -overlayPaddingY),
                Qt::AlignTop | Qt::AlignLeft,
                statusText);
        }

        void mousePressEvent(QMouseEvent *event) override {
            _lastMousePos = event->position().toPoint();
            _owner->updateMouseCoords(mapToOutputPixel(_lastMousePos));

            if (_owner->viewportUsesDirectPick() &&
                event->button() == Qt::LeftButton &&
                !_spacePan) {
                _owner->pickAtPixel(mapToOutputPixel(_lastMousePos));
                return;
            }

            const auto mode = effectiveMode();
            const bool panDrag =
                event->button() == Qt::MiddleButton ||
                (mode == mandelbrotgui::NavMode::pan &&
                    (event->button() == Qt::LeftButton ||
                        event->button() == Qt::RightButton));
            if (panDrag) {
                beginPanInteraction(event->button());
                return;
            }

            if (mode == mandelbrotgui::NavMode::realtimeZoom) {
                if (event->button() == Qt::LeftButton ||
                    event->button() == Qt::RightButton) {
                    beginRealtimeZoom(event->button() == Qt::LeftButton);
                    return;
                }
            }

            if (mode == mandelbrotgui::NavMode::zoom) {
                if (event->button() == Qt::LeftButton) {
                    _selectionOrigin = event->position().toPoint();
                    _selectionRect = QRect(_selectionOrigin, QSize(1, 1));
                    _zoomOutPending = false;
                    _zoomOutDragActive = false;
                    _zoomOutDragMoved = false;
                    _zoomOutPendingCommit = false;
                    _zoomOutPreviewScale = 1.0;
                    _zoomOutRedrawTimer.stop();
                    grabMouse();
                    return;
                }

                if (event->button() == Qt::RightButton) {
                    _zoomOutPending = true;
                    _zoomOutDragActive = true;
                    _zoomOutDragMoved = false;
                    _zoomOutDragLastPos = _lastMousePos;
                    _zoomOutPendingCommit = false;
                    _zoomOutPreviewScale = 1.0;
                    _selectionRect = {};
                    _zoomOutRedrawTimer.stop();
                    grabMouse();
                    return;
                }
            }
        }

        void mouseMoveEvent(QMouseEvent *event) override {
            _lastMousePos = event->position().toPoint();
            _owner->updateMouseCoords(mapToOutputPixel(_lastMousePos));

            if (_panning) {
                _panOffset = _lastMousePos - _dragOrigin;
                if (usePreviewFallback()) update();
                return;
            }

            if (_zoomOutDragActive) {
                const QPoint delta = _lastMousePos - _zoomOutDragLastPos;
                _zoomOutDragLastPos = _lastMousePos;
                const int dragMagnitude = std::abs(delta.x()) + std::abs(delta.y());
                if (dragMagnitude > 0) {
                    _zoomOutDragMoved = true;
                    const double zoomFactor =
                        _owner ? _owner->zoomRateFactor() : kZoomStepTable[8];
                    constexpr double zoomOutDragPixelsPerStep = 12.0;
                    const double stepScale =
                        std::pow(zoomFactor, static_cast<double>(dragMagnitude) /
                            zoomOutDragPixelsPerStep);
                    _zoomOutPreviewScale = std::clamp(
                        _zoomOutPreviewScale * stepScale, 1.0, 64.0);
                    _zoomOutPendingCommit = _zoomOutPreviewScale > 1.0001;
                    if (_zoomOutPendingCommit && !_zoomOutRedrawTimer.isActive()) {
                        _zoomOutRedrawTimer.start();
                    }
                    if (usePreviewFallback()) update();
                }
                return;
            }

            if (!_selectionRect.isNull()) {
                _selectionRect.setBottomRight(_lastMousePos);
                update();
            }
        }

        void mouseReleaseEvent(QMouseEvent *event) override {
            _lastMousePos = event->position().toPoint();
            _owner->updateMouseCoords(mapToOutputPixel(_lastMousePos));

            if (_panning) {
                if (_panButton == Qt::NoButton ||
                    event->button() == _panButton ||
                    event->button() == Qt::NoButton) {
                    endPanInteraction();
                }
                return;
            }

            if (_rtZoomTimer.isActive()) {
                _rtZoomTimer.stop();
                _rtZoomLastStepAt = {};
                releaseMouse();
                updateCursor();
                return;
            }

            if (_zoomOutDragActive &&
                (event->button() == Qt::RightButton ||
                    event->button() == Qt::NoButton)) {
                _zoomOutDragActive = false;
                _zoomOutPending = false;
                _zoomOutRedrawTimer.stop();
                if (mouseGrabber() == this) {
                    releaseMouse();
                }
                if (_zoomOutPendingCommit) {
                    commitZoomOutPreview();
                } else if (!_zoomOutDragMoved) {
                    _owner->zoomAtPixel(mapToOutputPixel(_lastMousePos), false);
                }
                _zoomOutDragMoved = false;
                _zoomOutPendingCommit = false;
                _zoomOutPreviewScale = 1.0;
                updateCursor();
                update();
                return;
            }

            if (!_selectionRect.isNull()) {
                const bool commitSelection =
                    event->button() == Qt::LeftButton ||
                    event->button() == Qt::NoButton;
                const QRect rect = mapToOutputRect(_selectionRect.normalized());
                _selectionRect = {};
                if (mouseGrabber() == this) {
                    releaseMouse();
                }

                if (commitSelection) {
                    if (rect.width() >= 8 && rect.height() >= 8) {
                        _owner->boxZoom(rect);
                    }
                }

                _zoomOutPending = false;
                updateCursor();
                update();
                return;
            }

            if (_zoomOutPending && event->button() == Qt::RightButton) {
                _zoomOutPending = false;
                _owner->zoomAtPixel(mapToOutputPixel(_lastMousePos), false);
            }
        }

        void wheelEvent(QWheelEvent *event) override {
            _lastMousePos = event->position().toPoint();
            _owner->updateMouseCoords(mapToOutputPixel(_lastMousePos));

            if (_panning) {
                event->accept();
                return;
            }

            const auto mode = effectiveMode();
            const bool zoomIn = event->angleDelta().y() > 0;

            if (mode == mandelbrotgui::NavMode::realtimeZoom) {
                const QSize output = _owner->outputSize();
                const QPointF defaultAnchor(
                    static_cast<double>(std::max(0, output.width() / 2)),
                    static_cast<double>(std::max(0, output.height() / 2)));
                const QPointF anchor = _rtZoomAnchorPixel.value_or(defaultAnchor);
                _rtZoomAnchorPixel = anchor;
                _owner->zoomAtPixel(clampToOutputPixel(anchor), zoomIn);
                return;
            }

            if (mode == mandelbrotgui::NavMode::zoom) {
                const bool resizeSelection =
                    !_selectionRect.isNull() &&
                    (event->buttons() & Qt::LeftButton);
                if (!resizeSelection) {
                    event->accept();
                    return;
                }

                const QRect current = _selectionRect.normalized();
                const QPointF center = current.center();
                const double factor = zoomIn ? 0.9 : 1.1;
                const double maxWidth = std::max(2.0, static_cast<double>(width() - 2));
                const double maxHeight = std::max(2.0, static_cast<double>(height() - 2));
                const double nextWidth = std::clamp(current.width() * factor, 2.0, maxWidth);
                const double nextHeight = std::clamp(current.height() * factor, 2.0, maxHeight);
                QRect scaled(
                    static_cast<int>(std::lround(center.x() - nextWidth / 2.0)),
                    static_cast<int>(std::lround(center.y() - nextHeight / 2.0)),
                    std::max(2, static_cast<int>(std::lround(nextWidth))),
                    std::max(2, static_cast<int>(std::lround(nextHeight))));
                const QRect bounds(0, 0, std::max(1, width()), std::max(1, height()));
                scaled = scaled.intersected(bounds);
                if (scaled.width() < 2 || scaled.height() < 2) {
                    event->accept();
                    return;
                }

                _selectionOrigin = scaled.topLeft();
                _selectionRect = QRect(_selectionOrigin, scaled.bottomRight());
                update();
                event->accept();
                return;
            }

            const int delta = zoomIn ? 10 : -10;
            _owner->adjustIterationsBy(delta);
        }

        void keyPressEvent(QKeyEvent *event) override {
            if (!event->isAutoRepeat() &&
                (event->key() == Qt::Key_Shift ||
                    event->key() == Qt::Key_Control)) {
                refreshActiveInteractionTimers();
            }
            if (handleArrowPanKeyPress(event)) {
                event->accept();
                return;
            }
            if (event->isAutoRepeat()) return;

            if (event->key() == Qt::Key_Escape) {
                if (_owner) _owner->cancelQueuedRenders();
                event->accept();
                return;
            }

            if (event->key() == Qt::Key_Z) {
                _owner->cycleNavMode();
                event->accept();
                return;
            }

            if (event->key() == Qt::Key_Space) {
                _spacePan = true;
                if (_owner) {
                    _owner->setDisplayedNavModeOverride(mandelbrotgui::NavMode::pan);
                }
                if (canBeginPanInteraction()) {
                    beginPanInteraction(Qt::NoButton);
                } else {
                    updateCursor();
                }
                return;
            }

            if (event->key() == Qt::Key_F1) {
                toggleMinimalUI();
                event->accept();
                return;
            }

            if (event->key() == Qt::Key_F2) {
                _owner->quickSaveImage();
                event->accept();
                return;
            }

            if (event->key() == Qt::Key_F12) {
                toggleFullscreen();
                event->accept();
                return;
            }

            if (event->key() == Qt::Key_H) {
                _owner->applyHomeView();
                event->accept();
                return;
            }

            if (event->key() == Qt::Key_G) {
                cycleGridMode();
                event->accept();
                return;
            }

            QWidget::keyPressEvent(event);
        }

        void keyReleaseEvent(QKeyEvent *event) override {
            if (!event->isAutoRepeat() &&
                (event->key() == Qt::Key_Shift ||
                    event->key() == Qt::Key_Control)) {
                refreshActiveInteractionTimers();
            }
            if (handleArrowPanKeyRelease(event)) {
                event->accept();
                return;
            }
            if (event->isAutoRepeat()) return;
            if (event->key() != Qt::Key_Space) {
                QWidget::keyReleaseEvent(event);
                return;
            }

            _spacePan = false;
            if (_panning && _panButton == Qt::NoButton) {
                endPanInteraction();
                return;
            }

            if (_owner && !_panning) {
                _owner->setDisplayedNavModeOverride(std::nullopt);
            }
            updateCursor();
        }

        void enterEvent(QEnterEvent *) override {
            updateCursor();
        }

        void leaveEvent(QEvent *) override {
            if (_owner) {
                _owner->clearMouseCoords();
            }
        }

        void resizeEvent(QResizeEvent *event) override {
            QWidget::resizeEvent(event);
            if (_fullscreenTransitionPending) {
                _fullscreenResizeTimer.start();
            }
        }

        void closeEvent(QCloseEvent *event) override {
            _rtZoomTimer.stop();
            _rtZoomLastStepAt = {};
            _panRedrawTimer.stop();
            _zoomOutRedrawTimer.stop();
            _arrowPanTimer.stop();
            if (mouseGrabber() == this) {
                releaseMouse();
            }
            event->accept();
            if (_owner) {
                _owner->requestApplicationShutdown(false);
            }
        }

    private:
        void cycleGridMode() {
            int idx = 0;
            while (idx < kGridModeCount && kGridModes[idx] != _gridDivisions)
                ++idx;
            idx = (idx + 1) % kGridModeCount;

            _gridDivisions = kGridModes[idx];
            update();
        }

        void toggleMinimalUI() {
            _minimalUI = !_minimalUI;
            updateCursor();
            update();
        }

        void toggleFullscreen() {
            if (_owner) {
                _owner->toggleViewportFullscreen();
            }
        }

        [[nodiscard]] bool precisionModifierActive() const {
            return QApplication::keyboardModifiers().testFlag(Qt::ShiftModifier);
        }

        [[nodiscard]] bool speedModifierActive() const {
            return QApplication::keyboardModifiers().testFlag(Qt::ControlModifier);
        }

        [[nodiscard]] int baseInteractionTickIntervalMs() const {
            return _owner ? _owner->interactionFrameIntervalMs() : 16;
        }

        [[nodiscard]] int precisionInteractionTickIntervalMs() const {
            return precisionModifierActive() ?
                kPrecisionInteractionDelayMs :
                baseInteractionTickIntervalMs();
        }

        void syncPanRedrawTimerInterval() {
            _panRedrawTimer.setInterval(precisionInteractionTickIntervalMs());
        }

        void syncRealtimeZoomTimerInterval() {
            _rtZoomTimer.setInterval(precisionInteractionTickIntervalMs());
        }

        void syncArrowPanTimerInterval() {
            _arrowPanTimer.setInterval(precisionInteractionTickIntervalMs());
        }

        void refreshActiveInteractionTimers() {
            syncPanRedrawTimerInterval();
            syncRealtimeZoomTimerInterval();
            syncArrowPanTimerInterval();
        }

        void resetZoomOutDragState() {
            _zoomOutPending = false;
            _zoomOutDragActive = false;
            _zoomOutDragMoved = false;
            _zoomOutPendingCommit = false;
            _zoomOutPreviewScale = 1.0;
        }

        void stopRealtimeZoom() {
            _rtZoomTimer.stop();
            _rtZoomLastStepAt = {};
        }

        [[nodiscard]] bool canBeginPanInteraction() const {
            return !_panning &&
                !_rtZoomTimer.isActive() &&
                _selectionRect.isNull() &&
                !_zoomOutDragActive;
        }

        void beginPanInteraction(Qt::MouseButton button) {
            _panning = true;
            _panButton = button;
            if (_owner) {
                _owner->setDisplayedNavModeOverride(mandelbrotgui::NavMode::pan);
            }
            _dragOrigin = _lastMousePos;
            _panOffset = {};
            _selectionRect = {};
            resetZoomOutDragState();
            stopRealtimeZoom();
            _panRedrawTimer.stop();
            _zoomOutRedrawTimer.stop();
            syncPanRedrawTimerInterval();
            _panRedrawTimer.start();
            grabMouse();
            updateCursor();
            update();
        }

        void endPanInteraction() {
            _panRedrawTimer.stop();
            _panning = false;
            commitPanOffset();
            _panButton = Qt::NoButton;
            if (_owner) {
                _owner->setDisplayedNavModeOverride(std::nullopt);
            }
            if (mouseGrabber() == this) {
                releaseMouse();
            }
            updateCursor();
            update();
        }

        void beginRealtimeZoom(bool zoomIn) {
            if (!_rtZoomAnchorPixel.has_value()) {
                const QSize output = _owner->outputSize();
                _rtZoomAnchorPixel = QPointF(
                    static_cast<double>(std::max(0, output.width() / 2)),
                    static_cast<double>(std::max(0, output.height() / 2)));
            }
            _rtZoomZoomIn = zoomIn;
            _rtZoomLastStepAt = std::chrono::steady_clock::now();
            syncRealtimeZoomTimerInterval();
            grabMouse();
            if (!precisionModifierActive()) {
                applyRealtimeZoomStep(true);
            }
            _rtZoomTimer.start();
        }

        bool handleArrowPanKeyPress(QKeyEvent *event) {
            if (!_owner || effectiveMode() != mandelbrotgui::NavMode::pan) {
                return false;
            }
            if (_panning || _zoomOutDragActive || !_selectionRect.isNull() ||
                _rtZoomTimer.isActive()) {
                return false;
            }

            bool *pressed = nullptr;
            switch (event->key()) {
                case Qt::Key_Left:
                    pressed = &_arrowPanLeft;
                    break;
                case Qt::Key_Right:
                    pressed = &_arrowPanRight;
                    break;
                case Qt::Key_Up:
                    pressed = &_arrowPanUp;
                    break;
                case Qt::Key_Down:
                    pressed = &_arrowPanDown;
                    break;
                default:
                    return false;
            }

            const bool alreadyPressed = *pressed;
            *pressed = true;
            syncArrowPanTimerInterval();
            if (!_arrowPanTimer.isActive()) {
                _arrowPanTimer.start();
            }
            const bool precisionMode =
                event->modifiers().testFlag(Qt::ShiftModifier);
            if (!event->isAutoRepeat() && !alreadyPressed && !precisionMode) {
                applyArrowPanStep();
            }
            return true;
        }

        bool handleArrowPanKeyRelease(QKeyEvent *event) {
            bool *pressed = nullptr;
            switch (event->key()) {
                case Qt::Key_Left:
                    pressed = &_arrowPanLeft;
                    break;
                case Qt::Key_Right:
                    pressed = &_arrowPanRight;
                    break;
                case Qt::Key_Up:
                    pressed = &_arrowPanUp;
                    break;
                case Qt::Key_Down:
                    pressed = &_arrowPanDown;
                    break;
                default:
                    return false;
            }

            *pressed = false;
            if (!_arrowPanLeft && !_arrowPanRight &&
                !_arrowPanUp && !_arrowPanDown) {
                _arrowPanTimer.stop();
            } else {
                syncArrowPanTimerInterval();
            }
            return true;
        }

        void applyArrowPanStep() {
            if (!_owner || effectiveMode() != mandelbrotgui::NavMode::pan) {
                _arrowPanTimer.stop();
                return;
            }
            if (_panning || _zoomOutDragActive || !_selectionRect.isNull() ||
                _rtZoomTimer.isActive()) {
                return;
            }

            const int xDir =
                (_arrowPanLeft ? 1 : 0) +
                (_arrowPanRight ? -1 : 0);
            const int yDir =
                (_arrowPanUp ? 1 : 0) +
                (_arrowPanDown ? -1 : 0);
            if (xDir == 0 && yDir == 0) {
                _arrowPanTimer.stop();
                return;
            }

            const int panRate = _owner->panRateValue();
            if (panRate <= 0) {
                return;
            }

            int step = 1;
            if (panRate > 1) {
                const int shiftedRate = panRate - 1;
                const double panFactor = std::pow(1.125,
                    static_cast<double>(shiftedRate - 8));
                if (!(panFactor > 0.0) || !std::isfinite(panFactor)) {
                    return;
                }
                step = std::max(1,
                    static_cast<int>(std::lround(16.0 * panFactor)));
            }
            if (speedModifierActive()) {
                step = std::max(1,
                    static_cast<int>(std::lround(
                        static_cast<double>(step) *
                        kBoostedPanSpeedFactor)));
            }
            _owner->panByPixels(QPoint(xDir * step, yDir * step));
        }

        void drawGrid(QPainter &painter) {
            if (_gridDivisions <= 1) return;

            const QRect area = rect();
            if (area.width() <= 1 || area.height() <= 1) return;

            painter.save();
            painter.setRenderHint(QPainter::Antialiasing, false);
            painter.setPen(QPen(QColor(255, 255, 255, 110), 1.0));

            for (int i = 1; i < _gridDivisions; ++i) {
                const int x = static_cast<int>(std::lround(
                    static_cast<double>(area.width()) * i / _gridDivisions));
                const int y = static_cast<int>(std::lround(
                    static_cast<double>(area.height()) * i / _gridDivisions));
                painter.drawLine(x, area.top(), x, area.bottom());
                painter.drawLine(area.left(), y, area.right(), y);
            }

            painter.restore();
        }

        void updateWindowTitle() {
            setWindowTitle(usePreviewFallback() ?
                "Viewport (Preview)" :
                "Viewport (Direct)");
        }

        QPoint clampToOutputPixel(const QPointF &pixel) const {
            const QSize outputSize = _owner->outputSize();
            return {
                std::clamp(static_cast<int>(std::lround(pixel.x())), 0,
                    std::max(0, outputSize.width() - 1)),
                std::clamp(static_cast<int>(std::lround(pixel.y())), 0,
                    std::max(0, outputSize.height() - 1))
            };
        }

        QPoint mapToOutputPixel(const QPoint &logicalPoint) const {
            return mapToOutputPixelRaw(logicalPoint);
        }

        QPoint mapToOutputPixelRaw(const QPoint &logicalPoint) const {
            return mapToOutputPixelRaw(QPointF(logicalPoint));
        }

        QPoint mapToOutputPixelRaw(const QPointF &logicalPoint) const {
            const QSize logicalSize = size();
            const QSize outputSize = _owner->outputSize();
            if (logicalSize.width() <= 0 || logicalSize.height() <= 0) {
                return {};
            }

            const double x = static_cast<double>(logicalPoint.x()) *
                outputSize.width() / std::max(1, logicalSize.width());
            const double y = static_cast<double>(logicalPoint.y()) *
                outputSize.height() / std::max(1, logicalSize.height());

            return {
                std::clamp(static_cast<int>(std::lround(x)), 0,
                    std::max(0, outputSize.width() - 1)),
                std::clamp(static_cast<int>(std::lround(y)), 0,
                    std::max(0, outputSize.height() - 1))
            };
        }

        QPoint mapToOutputPixel(const QPointF &logicalPoint) const {
            return mapToOutputPixel(logicalPoint.toPoint());
        }

        QPoint mapToOutputDelta(const QPoint &logicalDelta) const {
            const QSize logicalSize = size();
            const QSize outputSize = _owner->outputSize();
            if (logicalSize.width() <= 0 || logicalSize.height() <= 0) {
                return {};
            }

            return {
                static_cast<int>(std::lround(
                    static_cast<double>(logicalDelta.x()) *
                    outputSize.width() / std::max(1, logicalSize.width()))),
                static_cast<int>(std::lround(
                    static_cast<double>(logicalDelta.y()) *
                    outputSize.height() / std::max(1, logicalSize.height())))
            };
        }

        QRect mapToOutputRect(const QRect &logicalRect) const {
            const QPoint topLeft = mapToOutputPixel(logicalRect.topLeft());
            const QPoint bottomRight = mapToOutputPixel(logicalRect.bottomRight());
            return QRect(topLeft, bottomRight).normalized();
        }

        mandelbrotgui::NavMode effectiveMode() const {
            return (_spacePan || (_panning && _panButton == Qt::MiddleButton)) ?
                mandelbrotgui::NavMode::pan :
                _owner->navMode();
        }

        void updateCursor() {
            if (_minimalUI) {
                setCursor(Qt::BlankCursor);
                return;
            }

            if (_panning) {
                setCursor(Qt::ClosedHandCursor);
                return;
            }

            switch (effectiveMode()) {
                case mandelbrotgui::NavMode::realtimeZoom:
                case mandelbrotgui::NavMode::zoom:
                    setCursor(Qt::CrossCursor);
                    break;
                case mandelbrotgui::NavMode::pan:
                    setCursor(Qt::OpenHandCursor);
                    break;
            }
        }

        void commitPanOffset() {
            if (!_owner || _panOffset.isNull()) return;

            const QPoint delta = mapToOutputDelta(_panOffset);
            if (delta.isNull()) return;

            _owner->panByPixels(delta);
            _dragOrigin = _lastMousePos;
            _panOffset = {};
            update();
        }

        void commitZoomOutPreview() {
            if (!_owner) return;
            if (!_zoomOutPendingCommit ||
                NumberUtil::almostEqual(_zoomOutPreviewScale, 1.0)) {
                return;
            }

            const QPoint viewportCenter(width() / 2, height() / 2);
            const double scaleMultiplier = _zoomOutPreviewScale;
            _zoomOutPreviewScale = 1.0;
            _zoomOutPendingCommit = false;
            _owner->scaleAtPixel(
                mapToOutputPixel(viewportCenter),
                scaleMultiplier,
                true);
            update();
        }

        void applyRealtimeZoomStep(bool firstStep) {
            if (!_owner || !_rtZoomAnchorPixel.has_value()) return;

            const auto now = std::chrono::steady_clock::now();
            constexpr double baseRateFPS = 60.0;
            double elapsedSeconds = 1.0 / baseRateFPS;

            if (!firstStep) {
                if (_rtZoomLastStepAt.time_since_epoch().count() == 0) {
                    _rtZoomLastStepAt = now;
                    return;
                }

                const auto elapsed = std::chrono::duration_cast<
                    std::chrono::duration<double, std::milli>>(now - _rtZoomLastStepAt);
                elapsedSeconds = elapsed.count() / 1000.0;
                if (!(elapsedSeconds > 0.0) || !std::isfinite(elapsedSeconds)) return;
            }

            _rtZoomLastStepAt = now;
            updateRealtimeZoomAnchor(elapsedSeconds);
            const double zoomFactor = _owner->zoomRateFactor();
            if (!(zoomFactor > 0.0) || !std::isfinite(zoomFactor)) return;

            const double scalePerSecond = std::pow(zoomFactor, baseRateFPS);
            const double scaleMultiplier = _rtZoomZoomIn ?
                std::pow(scalePerSecond, -elapsedSeconds) :
                std::pow(scalePerSecond, elapsedSeconds);
            if (!(scaleMultiplier > 0.0) || !std::isfinite(scaleMultiplier)) return;

            _owner->scaleAtPixel(clampToOutputPixel(*_rtZoomAnchorPixel),
                scaleMultiplier, true);
        }

        void updateRealtimeZoomAnchor(double elapsedSeconds) {
            if (!_owner || !_rtZoomAnchorPixel.has_value()) return;

            const QPointF targetPixel(mapToOutputPixel(_lastMousePos));
            QPointF anchorPixel = *_rtZoomAnchorPixel;
            const double dx = targetPixel.x() - anchorPixel.x();
            const double dy = targetPixel.y() - anchorPixel.y();
            if (NumberUtil::almostEqual(dx, 0.0, 1e-6) &&
                NumberUtil::almostEqual(dy, 0.0, 1e-6)) {
                _rtZoomAnchorPixel = targetPixel;
                return;
            }

            const double distance = std::hypot(dx, dy);
            if (!(distance > 0.0) || !std::isfinite(distance)) {
                _rtZoomAnchorPixel = targetPixel;
                return;
            }

            double panFactor = std::max(0.1, _owner->panRateFactor());
            if (speedModifierActive()) {
                panFactor *= kBoostedPanSpeedFactor;
            }
            const double followPixelsPerSecond = 32.0 * panFactor * 60.0;
            const double maxStep = std::max(1.0, followPixelsPerSecond * elapsedSeconds);
            if (distance <= maxStep) {
                _rtZoomAnchorPixel = targetPixel;
                return;
            }

            const double t = maxStep / distance;
            anchorPixel.rx() += dx * t;
            anchorPixel.ry() += dy * t;
            const QSize output = _owner->outputSize();
            _rtZoomAnchorPixel = QPointF(
                std::clamp(anchorPixel.x(), 0.0,
                    static_cast<double>(std::max(0, output.width() - 1))),
                std::clamp(anchorPixel.y(), 0.0,
                    static_cast<double>(std::max(0, output.height() - 1))));
        }

        [[nodiscard]] bool usePreviewFallback() const {
            return _owner &&
                _owner->shouldUseInteractionPreviewFallback() &&
                previewTransform().has_value();
        }

        mandelbrotgui *_owner = nullptr;
        QTimer _rtZoomTimer;
        QTimer _panRedrawTimer;
        QTimer _zoomOutRedrawTimer;
        QTimer _arrowPanTimer;
        QPoint _lastMousePos;
        QPoint _dragOrigin;
        QPoint _selectionOrigin;
        QPoint _panOffset;
        QRect _selectionRect;
        std::optional<QPointF> _rtZoomAnchorPixel;
        std::chrono::steady_clock::time_point _rtZoomLastStepAt{};
        bool _rtZoomZoomIn = true;
        bool _panning = false;
        bool _spacePan = false;
        Qt::MouseButton _panButton = Qt::NoButton;
        bool _zoomOutPending = false;
        bool _zoomOutDragActive = false;
        bool _zoomOutDragMoved = false;
        bool _zoomOutPendingCommit = false;
        bool _arrowPanLeft = false;
        bool _arrowPanRight = false;
        bool _arrowPanUp = false;
        bool _arrowPanDown = false;
        QPoint _zoomOutDragLastPos;
        double _zoomOutPreviewScale = 1.0;
        int _gridDivisions = 0;
        bool _minimalUI = false;
        bool _fullscreenManaged = false;
        bool _fullscreenTransitionPending = false;
        bool _restoreMaximized = false;
        QRect _restoreGeometry;
        QSize _restoreOutputSize;
        QTimer _fullscreenResizeTimer;
    };
}

mandelbrotgui::mandelbrotgui(QWidget *parent)
    : QWidget(parent) {
    qApp->installEventFilter(this);
    buildUI();
    populateControls();
    if (_startupBackendError) return;
    initializeState();
    if (_startupBackendError) return;
    connectUI();
    loadSelectedBackend();
    if (_startupBackendError) return;
    syncControlsFromState();
    updateModeEnablement();
    updateControlWindowSize();

    _viewport = new ViewportWindow(this);
    _viewport->show();
    resizeViewportWindowToImageSize();
    requestRender(true);
}

mandelbrotgui::~mandelbrotgui() {
    qApp->removeEventFilter(this);
    shutdownForExit(true);
}

void mandelbrotgui::buildUI() {
    setWindowFlag(Qt::Window, true);
    setWindowTitle("Control");

    auto *cancelQueuedShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    cancelQueuedShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    QObject::connect(cancelQueuedShortcut, &QShortcut::activated,
        this, [this]() { cancelQueuedRenders(); });

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(8, 8, 8, 8);
    outerLayout->setSpacing(4);

    _menuBar = new QMenuBar(this);
    _menuBar->setNativeMenuBar(false);
    _helpMenu = _menuBar->addMenu("Help");
    _helpAction = _helpMenu->addAction("Help");
    _aboutAction = _helpMenu->addAction("About");
    outerLayout->setMenuBar(_menuBar);

    _controlScrollArea = new QScrollArea(this);
    _controlScrollArea->setWidgetResizable(true);
    _controlScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _controlScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    _controlScrollArea->setFrameShape(QFrame::NoFrame);
    _controlScrollArea->verticalScrollBar()->setFixedWidth(kControlScrollBarWidth);
    _controlScrollArea->setStyleSheet(QString(
        "QScrollBar:vertical {"
        " width: %1px;"
        " margin: 0;"
        "}"
        "QScrollBar::handle:vertical {"
        " min-height: 24px;"
        " background: palette(mid);"
        " border-radius: 4px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        " height: 0px;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        " background: transparent;"
        "}").arg(kControlScrollBarWidth));
    outerLayout->addWidget(_controlScrollArea);

    _controlScrollContent = new QWidget(_controlScrollArea);
    _controlScrollArea->setWidget(_controlScrollContent);

    auto *root = new QVBoxLayout(_controlScrollContent);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(3);
    const auto onSectionToggled = [this]() { updateControlWindowSize(); };

    _cpuGroup = new CollapsibleGroupBox("CPU", true, onSectionToggled);
    auto *cpuLayout = new QGridLayout(_cpuGroup);
    cpuLayout->addWidget(new QLabel("Name"), 0, 0);
    _cpuNameEdit = new QLineEdit();
    _cpuNameEdit->setReadOnly(true);
    _cpuNameEdit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    cpuLayout->addWidget(_cpuNameEdit, 0, 1, 1, 5);
    cpuLayout->addWidget(new QLabel("Cores"), 1, 0);
    _cpuCoresEdit = new QLineEdit();
    _cpuCoresEdit->setReadOnly(true);
    _cpuCoresEdit->setFixedWidth(48);
    cpuLayout->addWidget(_cpuCoresEdit, 1, 1);
    cpuLayout->addWidget(new QLabel("Threads"), 1, 2);
    _cpuThreadsEdit = new QLineEdit();
    _cpuThreadsEdit->setReadOnly(true);
    _cpuThreadsEdit->setFixedWidth(48);
    cpuLayout->addWidget(_cpuThreadsEdit, 1, 3);
    _useThreadsCheckBox = new QCheckBox("Use Threads");
    cpuLayout->addWidget(_useThreadsCheckBox, 1, 4, 1, 2);
    root->addWidget(_cpuGroup);
    static_cast<CollapsibleGroupBox *>(_cpuGroup)->applyExpandedState(
        _cpuGroup->isChecked());

    _renderGroup = new CollapsibleGroupBox("Render", true, onSectionToggled);
    auto *renderLayout = new QGridLayout(_renderGroup);
    renderLayout->addWidget(new QLabel("Backend"), 0, 0);
    _backendCombo = new QComboBox();
    _backendCombo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    _backendCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    _backendCombo->setMinimumContentsLength(14);
    renderLayout->addWidget(_backendCombo, 0, 1, 1, 3);
    renderLayout->addWidget(new QLabel("Iterations"), 1, 0);
    _iterationsSpin = new IterationSpinBox();
    _iterationsSpin->setRange(0, 1000000000);
    _iterationsSpin->setSpecialValueText("Auto");
    static_cast<IterationSpinBox *>(_iterationsSpin)->resolveAutoIterations =
        [this]() { return resolveCurrentIterationCount(); };
    if (QLineEdit *iterationsEdit = _iterationsSpin->findChild<QLineEdit *>()) {
        iterationsEdit->installEventFilter(this);
    }
    renderLayout->addWidget(_iterationsSpin, 1, 1, 1, 3);
    renderLayout->addWidget(new QLabel("Color"), 2, 0);
    _colorMethodCombo = new QComboBox();
    _colorMethodCombo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    _colorMethodCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    _colorMethodCombo->setMinimumContentsLength(12);
    renderLayout->addWidget(_colorMethodCombo, 2, 1, 1, 3);
    renderLayout->addWidget(new QLabel("Fractal"), 3, 0);
    _fractalCombo = new QComboBox();
    _fractalCombo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    _fractalCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    _fractalCombo->setMinimumContentsLength(12);
    renderLayout->addWidget(_fractalCombo, 3, 1, 1, 3);
    _juliaCheck = new QCheckBox("Julia");
    _inverseCheck = new QCheckBox("Inverse");
    renderLayout->addWidget(_juliaCheck, 4, 1);
    renderLayout->addWidget(_inverseCheck, 4, 2);
    root->addWidget(_renderGroup);
    static_cast<CollapsibleGroupBox *>(_renderGroup)->applyExpandedState(
        _renderGroup->isChecked());

    _infoGroup = new CollapsibleGroupBox("Info", true, onSectionToggled);
    auto *infoLayout = new QFormLayout(_infoGroup);
    _infoRealEdit = new QLineEdit();
    _infoRealEdit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    infoLayout->addRow("Real", _infoRealEdit);
    _infoImagEdit = new QLineEdit();
    _infoImagEdit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    infoLayout->addRow("Imag", _infoImagEdit);
    _infoZoomEdit = new QLineEdit();
    _infoZoomEdit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    infoLayout->addRow("Zoom", _infoZoomEdit);
    _infoSeedRealEdit = new QLineEdit();
    _infoSeedRealEdit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    infoLayout->addRow("Seed Real", _infoSeedRealEdit);
    _infoSeedImagEdit = new QLineEdit();
    _infoSeedImagEdit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    infoLayout->addRow("Seed Imag", _infoSeedImagEdit);
    auto *pointButtonsLayout = new QHBoxLayout();
    pointButtonsLayout->setContentsMargins(0, 0, 0, 0);
    _savePointButton = new QPushButton("Save View");
    _loadPointButton = new QPushButton("Load View");
    stabilizePushButton(_savePointButton);
    stabilizePushButton(_loadPointButton);
    pointButtonsLayout->addWidget(_savePointButton);
    pointButtonsLayout->addWidget(_loadPointButton);
    infoLayout->addRow(QString(), pointButtonsLayout);
    root->addWidget(_infoGroup);
    static_cast<CollapsibleGroupBox *>(_infoGroup)->applyExpandedState(
        _infoGroup->isChecked());

    _viewportGroup = new CollapsibleGroupBox("Viewport", true, onSectionToggled);
    auto *navLayout = new QGridLayout(_viewportGroup);
    navLayout->setColumnStretch(0, 0);
    navLayout->setColumnStretch(1, 1);
    navLayout->setColumnStretch(2, 0);
    navLayout->setHorizontalSpacing(8);
    navLayout->addWidget(new QLabel("Mode"), 0, 0);
    _navModeCombo = new QComboBox();
    _navModeCombo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    _navModeCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    _navModeCombo->setMinimumContentsLength(10);
    navLayout->addWidget(_navModeCombo, 0, 1, 1, 2);
    navLayout->addWidget(new QLabel("Pick"), 1, 0);
    _pickTargetCombo = new QComboBox();
    _pickTargetCombo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    _pickTargetCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    _pickTargetCombo->setMinimumContentsLength(10);
    navLayout->addWidget(_pickTargetCombo, 1, 1, 1, 2);

    navLayout->addWidget(new QLabel("Pan Rate"), 2, 0);
    _panRateSlider = new QSlider(Qt::Horizontal);
    _panRateSlider->setRange(0, 20);
    _panRateSlider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    _panRateSpin = new QSpinBox();
    _panRateSpin->setRange(0, 1000000000);
    _panRateSpin->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
    _panRateSpin->setFixedWidth(72);
    navLayout->addWidget(_panRateSlider, 2, 1);
    navLayout->addWidget(_panRateSpin, 2, 2);

    navLayout->addWidget(new QLabel("Zoom Rate"), 3, 0);
    _zoomRateSlider = new QSlider(Qt::Horizontal);
    _zoomRateSlider->setRange(0, 20);
    _zoomRateSlider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    _zoomRateSpin = new QSpinBox();
    _zoomRateSpin->setRange(0, 1000000000);
    _zoomRateSpin->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
    _zoomRateSpin->setFixedWidth(72);
    navLayout->addWidget(_zoomRateSlider, 3, 1);
    navLayout->addWidget(_zoomRateSpin, 3, 2);

    navLayout->addWidget(new QLabel("Exponent"), 4, 0);
    _exponentSlider = new QSlider(Qt::Horizontal);
    _exponentSlider->setRange(101, 1600);
    _exponentSlider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    _exponentSpin = new AdaptiveDoubleSpinBox(2);
    _exponentSpin->setRange(1.01, 1000000.0);
    _exponentSpin->setSingleStep(0.01);
    _exponentSpin->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
    _exponentSpin->setFixedWidth(72);
    navLayout->addWidget(_exponentSlider, 4, 1);
    navLayout->addWidget(_exponentSpin, 4, 2);
    root->addWidget(_viewportGroup);
    static_cast<CollapsibleGroupBox *>(_viewportGroup)->applyExpandedState(
        _viewportGroup->isChecked());

    _sineGroup = new CollapsibleGroupBox("Sine Color", false, onSectionToggled);
    auto *sineLayout = new QGridLayout(_sineGroup);
    sineLayout->setHorizontalSpacing(6);
    sineLayout->setColumnStretch(1, 1);
    sineLayout->setColumnStretch(3, 1);
    sineLayout->setColumnMinimumWidth(4, 6);
    sineLayout->addWidget(new QLabel("Name"), 0, 0);
    _sineCombo = new QComboBox();
    _sineCombo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    _sineCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    _sineCombo->setMinimumContentsLength(10);
    sineLayout->addWidget(_sineCombo, 0, 1, 1, 3);
    _randomizeSineButton = new QPushButton("Randomize RGB");
    stabilizePushButton(_randomizeSineButton);
    _randomizeSineButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    sineLayout->addWidget(_randomizeSineButton, 0, 5);
    _importSineButton = new QPushButton("Import");
    stabilizePushButton(_importSineButton);
    _importSineButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    _saveSineButton = new QPushButton("Save");
    stabilizePushButton(_saveSineButton);
    _saveSineButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    QDoubleSpinBox compactSpinProbe;
    compactSpinProbe.setRange(0.0001, 1000.0);
    compactSpinProbe.setDecimals(4);
    compactSpinProbe.setSingleStep(0.01);
    const int compactSpinWidth = compactSpinProbe.sizeHint().width();
    sineLayout->addWidget(new QLabel("R"), 1, 0);
    _freqRSpin = new AdaptiveDoubleSpinBox(4);
    _freqRSpin->setRange(0.0, 1000.0);
    _freqRSpin->setSingleStep(0.01);
    _freqRSpin->setFixedWidth(compactSpinWidth);
    sineLayout->addWidget(_freqRSpin, 1, 1);
    sineLayout->addWidget(new QLabel("G"), 1, 2);
    _freqGSpin = new AdaptiveDoubleSpinBox(4);
    _freqGSpin->setRange(0.0, 1000.0);
    _freqGSpin->setSingleStep(0.01);
    _freqGSpin->setFixedWidth(compactSpinWidth);
    sineLayout->addWidget(_freqGSpin, 1, 3);
    sineLayout->addWidget(_importSineButton, 1, 5);
    sineLayout->addWidget(new QLabel("B"), 2, 0);
    _freqBSpin = new AdaptiveDoubleSpinBox(4);
    _freqBSpin->setRange(0.0, 1000.0);
    _freqBSpin->setSingleStep(0.01);
    _freqBSpin->setFixedWidth(compactSpinWidth);
    sineLayout->addWidget(_freqBSpin, 2, 1);
    sineLayout->addWidget(new QLabel("Mult"), 2, 2);
    _freqMultSpin = new AdaptiveDoubleSpinBox(4);
    _freqMultSpin->setRange(0.0001, 1000.0);
    _freqMultSpin->setSingleStep(0.01);
    _freqMultSpin->setFixedWidth(compactSpinWidth);
    sineLayout->addWidget(_freqMultSpin, 2, 3);
    sineLayout->addWidget(_saveSineButton, 2, 5);
    const int compactComboWidth = _sineCombo->sizeHint().width();
    auto *sinePreview = new SinePreviewWidget();
    sinePreview->onRangeChanged = [this](double, double) { updateSinePreview(); };
    _sinePreview = sinePreview;
    sineLayout->addWidget(_sinePreview, 3, 0, 1, 6);
    root->addWidget(_sineGroup);
    static_cast<CollapsibleGroupBox *>(_sineGroup)->applyExpandedState(
        _sineGroup->isChecked());

    _paletteGroup = new CollapsibleGroupBox("Palette", false, onSectionToggled);
    auto *paletteLayout = new QGridLayout(_paletteGroup);
    paletteLayout->setHorizontalSpacing(6);
    paletteLayout->setColumnStretch(1, 1);
    paletteLayout->setColumnStretch(3, 1);
    paletteLayout->setColumnMinimumWidth(4, 6);
    paletteLayout->addWidget(new QLabel("Name"), 0, 0);
    _paletteCombo = new QComboBox();
    _paletteCombo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    _paletteCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    _paletteCombo->setMinimumContentsLength(10);
    _paletteCombo->setFixedWidth(compactComboWidth);
    paletteLayout->addWidget(_paletteCombo, 0, 1, 1, 3);
    _paletteEditorButton = new QPushButton("Edit");
    stabilizePushButton(_paletteEditorButton);
    _paletteEditorButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    paletteLayout->addWidget(_paletteEditorButton, 0, 5);
    paletteLayout->addWidget(new QLabel("Length"), 1, 0);
    _paletteLengthSpin = new AdaptiveDoubleSpinBox(4);
    _paletteLengthSpin->setRange(0.0001, 1000000.0);
    _paletteLengthSpin->setFixedWidth(compactSpinWidth);
    paletteLayout->addWidget(_paletteLengthSpin, 1, 1);
    paletteLayout->addWidget(new QLabel("Offset"), 1, 2);
    _paletteOffsetSpin = new AdaptiveDoubleSpinBox(4);
    _paletteOffsetSpin->setRange(0.0, 1000000.0);
    _paletteOffsetSpin->setFixedWidth(compactSpinWidth);
    paletteLayout->addWidget(_paletteOffsetSpin, 1, 3);
    _palettePreviewLabel = new PalettePreviewWidget();
    paletteLayout->addWidget(_palettePreviewLabel, 2, 0, 1, 6);
    root->addWidget(_paletteGroup);
    static_cast<CollapsibleGroupBox *>(_paletteGroup)->applyExpandedState(
        _paletteGroup->isChecked());

    _lightGroup = new CollapsibleGroupBox("Light", false, onSectionToggled);
    auto *lightLayout = new QFormLayout(_lightGroup);
    _lightRealEdit = new QLineEdit();
    _lightRealEdit->setReadOnly(true);
    _lightRealEdit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    lightLayout->addRow("Real", _lightRealEdit);
    _lightImagEdit = new QLineEdit();
    _lightImagEdit->setReadOnly(true);
    _lightImagEdit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    lightLayout->addRow("Imag", _lightImagEdit);
    _lightColorButton = new QPushButton();
    stabilizePushButton(_lightColorButton);
    _lightColorButton->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    lightLayout->addRow("Color", _lightColorButton);
    root->addWidget(_lightGroup);
    static_cast<CollapsibleGroupBox *>(_lightGroup)->applyExpandedState(
        _lightGroup->isChecked());

    auto *outputGroup = new CollapsibleGroupBox("Image", true, onSectionToggled);
    auto *outputLayout = new QGridLayout(outputGroup);
    outputLayout->setColumnStretch(0, 0);
    outputLayout->setColumnStretch(1, 0);
    outputLayout->setColumnStretch(2, 0);
    outputLayout->setColumnStretch(3, 0);
    outputLayout->setColumnStretch(4, 0);
    outputLayout->setColumnStretch(5, 0);
    outputLayout->setColumnStretch(6, 1);
    outputLayout->setHorizontalSpacing(8);
    outputLayout->addWidget(new QLabel("W"), 0, 0);
    _outputWidthSpin = new QSpinBox();
    _outputWidthSpin->setRange(1, 32768);
    _outputWidthSpin->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
    _outputWidthSpin->setFixedWidth(96);
    outputLayout->addWidget(_outputWidthSpin, 0, 1);
    auto *timesLabel = new QLabel("x");
    timesLabel->setAlignment(Qt::AlignCenter);
    outputLayout->addWidget(timesLabel, 0, 2);
    outputLayout->addWidget(new QLabel("H"), 0, 3);
    _outputHeightSpin = new QSpinBox();
    _outputHeightSpin->setRange(1, 32768);
    _outputHeightSpin->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
    _outputHeightSpin->setFixedWidth(96);
    outputLayout->addWidget(_outputHeightSpin, 0, 4);
    _resizeButton = new QPushButton("Resize");
    stabilizePushButton(_resizeButton);
    outputLayout->addWidget(_resizeButton, 0, 5);
    outputLayout->addWidget(new QLabel("AA"), 1, 0);
    _aaSpin = new QSpinBox();
    _aaSpin->setRange(1, 16);
    outputLayout->addWidget(_aaSpin, 1, 1, 1, 2);
    _preserveRatioCheck = new QCheckBox("Keep Aspect Ratio");
    outputLayout->addWidget(_preserveRatioCheck, 1, 3, 1, 3);
    _imageMemoryLabel = new QLabel("Render: -  Output: -");
    _imageMemoryLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    _imageMemoryLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    outputLayout->addWidget(_imageMemoryLabel, 2, 0, 1, 7);
    root->addWidget(outputGroup);
    outputGroup->applyExpandedState(outputGroup->isChecked());

    root->addStretch(1);

    auto *buttonPanel = new QWidget(this);
    auto *buttonRow = new QGridLayout(buttonPanel);
    buttonRow->setContentsMargins(0, 0, 0, 0);
    _calculateButton = new QPushButton("Calculate");
    _homeButton = new QPushButton("Home");
    _zoomButton = new QPushButton("Zoom");
    _saveButton = new QPushButton("Save");
    stabilizePushButton(_calculateButton);
    stabilizePushButton(_homeButton);
    stabilizePushButton(_zoomButton);
    stabilizePushButton(_saveButton);
    buttonRow->addWidget(_calculateButton, 0, 0);
    buttonRow->addWidget(_homeButton, 0, 1);
    buttonRow->addWidget(_zoomButton, 1, 0);
    buttonRow->addWidget(_saveButton, 1, 1);
    outerLayout->addWidget(buttonPanel);

    auto *statusDivider = new QFrame(this);
    statusDivider->setFrameShape(QFrame::HLine);
    statusDivider->setFrameShadow(QFrame::Sunken);
    outerLayout->addWidget(statusDivider);

    auto *statusPanel = new QWidget(this);
    auto *statusRow = new QHBoxLayout(statusPanel);
    statusRow->setContentsMargins(0, 0, 0, 0);
    _progressLabel = new QLabel("0%");
    _progressLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    _progressLabel->setFixedWidth(40);
    _progressBar = new QProgressBar();
    _progressBar->setRange(0, 100);
    _progressBar->setValue(0);
    _progressBar->setTextVisible(false);
    _progressBar->setFixedWidth(100);
    _pixelsPerSecondLabel = new QLabel("0 pixels/s");
    _pixelsPerSecondLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    _pixelsPerSecondLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    _statusRightLabel = new QLabel();
    _statusRightLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    _statusRightLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    _statusRightLabel->setTextFormat(Qt::RichText);
    _statusRightLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
    _statusRightLabel->setOpenExternalLinks(false);
    _statusRightLabel->installEventFilter(this);
    QObject::connect(_statusRightLabel, &QLabel::linkActivated,
        this, [](const QString &link) {
            QDesktopServices::openUrl(QUrl(link));
        });
    QObject::connect(_statusRightLabel, &QLabel::linkHovered,
        this, [this](const QString &link) {
            if (_statusRightLabel) {
                if (link.isEmpty()) {
                    _statusRightLabel->unsetCursor();
                } else {
                    _statusRightLabel->setCursor(Qt::PointingHandCursor);
                }
            }
        });
    _statusMarqueeTimer.setInterval(125);
    QObject::connect(&_statusMarqueeTimer, &QTimer::timeout,
        this, [this]() {
            ++_statusMarqueeOffset;
            updateStatusLabels();
        });
    statusRow->addWidget(_progressLabel);
    statusRow->addWidget(_progressBar);
    statusRow->addWidget(_pixelsPerSecondLabel);
    statusRow->addSpacing(4);
    statusRow->addWidget(_statusRightLabel, 1);
    outerLayout->addWidget(statusPanel);
}

void mandelbrotgui::populateControls() {
    QString backendError;
    const QStringList backendNames = listBackendNames(backendError);
    _backendNames = backendNames;
    _backendCombo->addItems(backendNames);
    if (backendNames.isEmpty()) {
        _startupBackendError = true;
        QMessageBox::critical(this, "Backend", backendError);
        QTimer::singleShot(0, this, [this]() { close(); });
        return;
    }

    _colorMethodCombo->addItems({
        "Iterations",
        "Smooth Iterations",
        "Palette",
        "Light"
        });
    _fractalCombo->addItems({
        "Mandelbrot",
        "Perpendicular",
        "Burning Ship"
        });
    _navModeCombo->addItems({
        "Realtime Zoom",
        "Box Zoom",
        "Pan"
        });
    _pickTargetCombo->addItems({
        "Zoom Point",
        "Seed Point",
        "Light Point"
        });
}

void mandelbrotgui::initializeState() {
    const QString preferredType = QString::fromUtf8(defaultBackendType);
    int defaultIndex = -1;
    const QString suffixNeedle = QString(" - %1").arg(preferredType);
    for (int i = 0; i < _backendCombo->count(); ++i) {
        const QString backendName = _backendCombo->itemText(i).trimmed();
        if (backendName.compare(preferredType, Qt::CaseInsensitive) == 0 ||
            backendName.endsWith(suffixNeedle, Qt::CaseInsensitive)) {
            defaultIndex = i;
            break;
        }
    }
    if (defaultIndex < 0) {
        defaultIndex = _backendCombo->findText(
            preferredType,
            Qt::MatchContains);
    }
    if (defaultIndex < 0 && _backendCombo->count() > 0) {
        defaultIndex = 0;
    }
    if (defaultIndex < 0) {
        _startupBackendError = true;
        QMessageBox::critical(this, "Backend",
            "No backend options are available.");
        QTimer::singleShot(0, this, [this]() { close(); });
        return;
    }

    _backendCombo->setCurrentIndex(defaultIndex);
    _state.backend = _backendCombo->itemText(defaultIndex);
    _state.iterations = 0;
    _state.sineName = kDefaultSineName;
    refreshSineList(_state.sineName);
    QString sineError;
    if (!loadSineByName(_state.sineName, false, &sineError)) {
        bool loaded = false;
        const QStringList sineNames = listSineNames();
        if (!sineNames.isEmpty()) {
            loaded = loadSineByName(sineNames.front(), false, &sineError);
        }
        if (!loaded) {
            createNewSinePalette(false);
        }
    }

    _state.paletteName = kDefaultPaletteName;
    refreshPaletteList(_state.paletteName);
    QString paletteError;
    if (!loadPaletteByName(_state.paletteName, false, &paletteError)) {
        bool loaded = false;
        const QStringList paletteNames = listPaletteNames();
        if (!paletteNames.isEmpty()) {
            loaded = loadPaletteByName(paletteNames.front(), false, &paletteError);
        }
        if (!loaded) {
            createNewColorPalette(false);
        }
    }

    const CPUInfo cpu = queryCPUInfo();
    _cpuNameEdit->setText(QString::fromStdString(cpu.name));
    if (cpu.cores > 0) _cpuCoresEdit->setText(QString::number(cpu.cores));
    if (cpu.threads > 0) _cpuThreadsEdit->setText(QString::number(cpu.threads));

    setStatusMessage("Ready");
    _mouseText = "Mouse: -";
    _viewportRenderTimeText.clear();
    _imageMemoryText = "Render: -  Output: -";
    syncZoomTextFromState();
    syncPointTextFromState();
    syncSeedTextFromState();
    _displayedPointRealText = _pointRealText;
    _displayedPointImagText = _pointImagText;
    _displayedZoomText = _zoomText;
    _displayedOutputSize = outputSize();
    _hasDisplayedViewState = true;
    markPointViewSavedState();
}

void mandelbrotgui::connectUI() {
    QObject::connect(_backendCombo, &QComboBox::currentTextChanged,
        this, [this](const QString &text) {
            _state.backend = text;
            _state.manualBackendSelection = true;
            loadSelectedBackend();
        });

    QObject::connect(_useThreadsCheckBox, &QCheckBox::toggled,
        this, [this](bool checked) {
            _state.useThreads = checked;
            freezeViewportPreview(false, false);
        });

    auto requestFromControls = [this]() {
        syncStateFromControls();
        updateSinePreview();
        requestRender();
        };
    auto confirmDiscardDirtySine = [this]() {
        if (!isSineDirty()) return true;

        const QMessageBox::StandardButton choice = QMessageBox::question(
            this,
            "Sine Color",
            "Current sine palette has unsaved changes. Discard them?",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (choice != QMessageBox::Yes) {
            refreshSineList(_state.sineName);
            return false;
        }

        if (_hasSavedSineState) {
            _state.sineName = _savedSineName;
            _state.sinePalette = _savedSinePalette;
        }
        refreshSineList(_state.sineName);
        syncControlsFromState();
        updateViewport();
        return true;
        };
    auto confirmDiscardDirtyPalette = [this]() {
        if (!isPaletteDirty()) return true;

        const QMessageBox::StandardButton choice = QMessageBox::question(
            this,
            "Palette",
            "Current palette has unsaved changes. Discard them?",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (choice != QMessageBox::Yes) {
            refreshPaletteList(_state.paletteName);
            return false;
        }

        if (_hasSavedPaletteState) {
            _state.paletteName = _savedPaletteName;
            _state.palette = _savedPalette;
            _palettePreviewDirty = true;
        }
        refreshPaletteList(_state.paletteName);
        syncControlsFromState();
        updateViewport();
        return true;
        };

    QObject::connect(_iterationsSpin, QOverload<int>::of(&QSpinBox::valueChanged),
        this, [requestFromControls](int) { requestFromControls(); });
    QObject::connect(_colorMethodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [this](int) {
            syncStateFromControls();
            updateModeEnablement();
            requestRender();
        });
    QObject::connect(_fractalCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [requestFromControls](int) { requestFromControls(); });
    QObject::connect(_juliaCheck, &QCheckBox::toggled,
        this, [requestFromControls](bool) { requestFromControls(); });
    QObject::connect(_inverseCheck, &QCheckBox::toggled,
        this, [requestFromControls](bool) { requestFromControls(); });
    QObject::connect(_aaSpin, QOverload<int>::of(&QSpinBox::valueChanged),
        this, [requestFromControls](int) { requestFromControls(); });

    QObject::connect(_navModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [this](int index) { updateNavMode(index); });
    QObject::connect(_pickTargetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [this](int index) { updateSelectionTarget(index); });

    QObject::connect(_panRateSlider, &QSlider::valueChanged,
        this, [this](int value) {
            _panRateSpin->blockSignals(true);
            _panRateSpin->setValue(value);
            _panRateSpin->blockSignals(false);
            _state.panRate = value;
        });
    QObject::connect(_panRateSpin, QOverload<int>::of(&QSpinBox::valueChanged),
        this, [this](int value) {
            const QSignalBlocker sliderBlocker(_panRateSlider);
            _panRateSlider->setValue(std::clamp(
                value,
                _panRateSlider->minimum(),
                _panRateSlider->maximum()));
            _state.panRate = value;
        });

    QObject::connect(_zoomRateSlider, &QSlider::valueChanged,
        this, [this](int value) {
            _zoomRateSpin->blockSignals(true);
            _zoomRateSpin->setValue(value);
            _zoomRateSpin->blockSignals(false);
            _state.zoomRate = value;
        });
    QObject::connect(_zoomRateSpin, QOverload<int>::of(&QSpinBox::valueChanged),
        this, [this](int value) {
            const QSignalBlocker sliderBlocker(_zoomRateSlider);
            _zoomRateSlider->setValue(std::clamp(
                value,
                _zoomRateSlider->minimum(),
                _zoomRateSlider->maximum()));
            _state.zoomRate = value;
        });

    QObject::connect(_exponentSlider, &QSlider::valueChanged,
        this, [this](int value) {
            const double exponent = value / 100.0;
            _exponentSpin->blockSignals(true);
            setAdaptiveSpinValue(_exponentSpin, exponent);
            _exponentSpin->blockSignals(false);
            _state.exponent = exponent;
            requestRender();
        });
    QObject::connect(_exponentSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, [this](double value) {
            const QSignalBlocker sliderBlocker(_exponentSlider);
            const int sliderValue = std::clamp(
                static_cast<int>(std::round(value * 100.0)),
                _exponentSlider->minimum(),
                _exponentSlider->maximum());
            _exponentSlider->setValue(sliderValue);
            _state.exponent = value;
            requestRender();
        });
    QObject::connect(_sineCombo, &QComboBox::currentTextChanged,
        this, [this, confirmDiscardDirtySine](const QString &name) {
            if (!confirmDiscardDirtySine()) return;

            const QString normalizedName = undecoratedLabel(name);
            if (normalizedName == kNewEntryLabel) {
                createNewSinePalette();
                return;
            }
            if (normalizedName.isEmpty()) return;

            QString errorMessage;
            if (!loadSineByName(normalizedName, true, &errorMessage) &&
                !errorMessage.isEmpty()) {
                QMessageBox::warning(this, "Sine Color", errorMessage);
                refreshSineList(_state.sineName);
                requestRender();
            }
        });
    QObject::connect(_freqRSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, [requestFromControls](double) { requestFromControls(); });
    QObject::connect(_freqGSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, [requestFromControls](double) { requestFromControls(); });
    QObject::connect(_freqBSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, [requestFromControls](double) { requestFromControls(); });
    QObject::connect(_freqMultSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, [requestFromControls](double) { requestFromControls(); });
    QObject::connect(_randomizeSineButton, &QPushButton::clicked,
        this, [this]() {
            auto randomFrequency = []() {
                constexpr double minValue = 0.5;
                constexpr double maxValue = 1.5;
                const double value = minValue +
                    (maxValue - minValue) * QRandomGenerator::global()->generateDouble();
                return std::round(value * 10000.0) / 10000.0;
                };

            const QSignalBlocker rBlocker(_freqRSpin);
            const QSignalBlocker gBlocker(_freqGSpin);
            const QSignalBlocker bBlocker(_freqBSpin);
            setAdaptiveSpinValue(_freqRSpin, randomFrequency());
            setAdaptiveSpinValue(_freqGSpin, randomFrequency());
            setAdaptiveSpinValue(_freqBSpin, randomFrequency());

            syncStateFromControls();
            updateSinePreview();
            requestRender();
        });
    QObject::connect(_saveSineButton, &QPushButton::clicked,
        this, [this]() { saveSine(); });
    QObject::connect(_importSineButton, &QPushButton::clicked,
        this, [this]() { importSine(); });
    QObject::connect(_savePointButton, &QPushButton::clicked,
        this, [this]() { savePointView(); });
    QObject::connect(_loadPointButton, &QPushButton::clicked,
        this, [this]() { loadPointView(); });
    auto applyViewTextEdits = [this]() {
        syncStateFromControls();
        syncStateReadouts();
        requestRender();
        };
    auto connectViewTextEdit = [this, applyViewTextEdits](QLineEdit *edit) {
        if (!edit) return;
        markLineEditTextCommitted(edit);
        QObject::connect(edit, &QLineEdit::editingFinished,
            this, [edit, applyViewTextEdits]() {
                if (!hasUncommittedLineEditChange(edit)) return;
                applyViewTextEdits();
            });
        };
    connectViewTextEdit(_infoRealEdit);
    connectViewTextEdit(_infoImagEdit);
    connectViewTextEdit(_infoZoomEdit);
    connectViewTextEdit(_infoSeedRealEdit);
    connectViewTextEdit(_infoSeedImagEdit);

    QObject::connect(_paletteCombo, &QComboBox::currentTextChanged,
        this, [this, confirmDiscardDirtyPalette](const QString &name) {
            if (!confirmDiscardDirtyPalette()) return;

            const QString normalizedName = undecoratedLabel(name);
            if (normalizedName == kNewEntryLabel) {
                createNewColorPalette();
                return;
            }
            if (normalizedName.isEmpty()) return;

            QString errorMessage;
            if (!loadPaletteByName(normalizedName, true, &errorMessage) &&
                !errorMessage.isEmpty()) {
                QMessageBox::warning(this, "Palette", errorMessage);
                refreshPaletteList(_state.paletteName);
                requestRender();
            }
        });
    QObject::connect(_paletteLengthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, [this](double value) {
            _state.palette.totalLength = static_cast<float>(value);
            _state.palette = stopsToConfig(
                configToStops(_state.palette),
                _state.palette.totalLength,
                _state.palette.offset,
                _state.palette.blendEnds
            );
            _palettePreviewDirty = true;
            updateViewport();
            requestRender();
        });
    QObject::connect(_paletteOffsetSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, [this](double value) {
            _state.palette.offset = static_cast<float>(value);
            _palettePreviewDirty = true;
            updateViewport();
            requestRender();
        });
    QObject::connect(_paletteEditorButton, &QPushButton::clicked,
        this, [this]() { openPaletteEditor(); });
    QObject::connect(_lightColorButton, &QPushButton::clicked,
        this, [this]() {
            const QColor initial = lightColorToQColor(_state.lightColor);
            const QColor color = QColorDialog::getColor(
                initial, this, "Light Color");
            if (!color.isValid()) return;

            _state.lightColor = lightColorFromQColor(color);
            updateLightColorButton();
            requestRender();
        });

    QObject::connect(_outputWidthSpin, QOverload<int>::of(&QSpinBox::valueChanged),
        this, [this](int) { updateAspectLinkedSizes(true); });
    QObject::connect(_outputHeightSpin, QOverload<int>::of(&QSpinBox::valueChanged),
        this, [this](int) { updateAspectLinkedSizes(false); });
    QObject::connect(_preserveRatioCheck, &QCheckBox::toggled,
        this, [this](bool checked) { _state.preserveRatio = checked; });

    QObject::connect(_calculateButton, &QPushButton::clicked,
        this, [this]() {
            syncStateFromControls();
            requestRender(true);
        });
    QObject::connect(_homeButton, &QPushButton::clicked,
        this, [this]() { applyHomeView(); });
    QObject::connect(_zoomButton, &QPushButton::clicked,
        this, [this]() {
            if (_viewport) {
                const QSize size = outputSize();
                zoomAtPixel(QPoint(size.width() / 2, size.height() / 2), true);
            }
        });
    QObject::connect(_saveButton, &QPushButton::clicked,
        this, [this]() { saveImage(); });
    QObject::connect(_resizeButton, &QPushButton::clicked,
        this, [this]() { resizeViewportImage(); });
    QObject::connect(_helpAction, &QAction::triggered,
        this, [this]() { showHelpDialog(); });
    QObject::connect(_aboutAction, &QAction::triggered,
        this, [this]() { showAboutDialog(); });
}

void mandelbrotgui::loadSelectedBackend() {
    _backendGeneration.fetch_add(1, std::memory_order_acq_rel);
    _callbackRenderRequestId.store(0, std::memory_order_release);
    if (_backend && _renderSession) {
        _renderSession->setCallbacks({});
    }
    if (_state.backend.isEmpty()) {
        _startupBackendError = true;
        QMessageBox::critical(this, "Backend", "No backend is selected.");
        QTimer::singleShot(0, this, [this]() { close(); });
        return;
    }

    freezeViewportPreview(true, true);
    finishRenderThread();
    _interactionPreviewFallbackLatched.store(false, std::memory_order_release);
    _interactionPreviewForced.store(false, std::memory_order_release);
    _renderSession = nullptr;
    _navigationSession = nullptr;
    _previewSession = nullptr;
    _backend.reset();
    _lastPresentedRenderId = 0;
    _progressCancelled = false;

    std::string error;
    _backend = loadBackendModule(PathUtil::executableDir(),
        _state.backend.toStdString(), error);
    if (_backend) {
        _renderSession = _backend.makeSession();
        if (!_renderSession && error.empty()) {
            error = "Failed to create backend session.";
        }
    }
    if (_backend) {
        _navigationSession = _backend.makeSession();
        if (!_navigationSession && error.empty()) {
            error = "Failed to create navigation session.";
        }
    }
    if (_backend) {
        _previewSession = _backend.makeSession();
        if (!_previewSession && error.empty()) {
            error = "Failed to create preview session.";
        }
    }
    if (_renderSession && _navigationSession && _previewSession) {
        bindBackendCallbacks();
        startRenderWorker();
        setViewportInteractive(true);
        updateViewport();
        return;
    }

    const QString message = QString::fromStdString(error.empty() ?
        "Failed to load backend." : error);
    if (!_viewport) {
        _startupBackendError = true;
        QMessageBox::critical(this, "Backend", message);
        QTimer::singleShot(0, this, [this]() { close(); });
        return;
    }

    QMessageBox::warning(this, "Backend", message);
    setViewportInteractive(true);
}

QString mandelbrotgui::backendForRank(int rank) const {
    if (_backendNames.isEmpty()) return QString();

    const int targetPrecision = std::clamp(rank, 0, 3);
    const int currentType = backendTypeRank(_state.backend);

    QString fallback;
    int fallbackPrecision = -1;
    int fallbackTypeDist = INT_MAX;
    QString best;
    int bestPenalty = INT_MAX;
    int bestTypeDist = INT_MAX;

    for (const QString &backendName : _backendNames) {
        const QString name = backendName.trimmed();
        const int precision = backendPrecisionRank(name);
        const int typeDist = std::abs(backendTypeRank(name) - currentType);

        if (precision > fallbackPrecision ||
            (precision == fallbackPrecision && typeDist < fallbackTypeDist)) {
            fallback = name;
            fallbackPrecision = precision;
            fallbackTypeDist = typeDist;
        }

        if (precision < targetPrecision) continue;

        const int penalty = precision - targetPrecision;
        if (penalty < bestPenalty ||
            (penalty == bestPenalty && typeDist < bestTypeDist)) {
            best = name;
            bestPenalty = penalty;
            bestTypeDist = typeDist;
        }
    }

    return best.isEmpty() ? fallback : best;
}

void mandelbrotgui::switchBackend(
    int rank,
    uint64_t requestId,
    uint64_t backendGeneration,
    const std::optional<PendingPickAction> &pick
) {
    if (backendGeneration != _backendGeneration.load(std::memory_order_acquire)) return;
    if (requestId != _latestRenderRequestId.load(std::memory_order_acquire)) return;

    const QString backend = backendForRank(rank);
    if (backend.isEmpty() || backend == _state.backend) return;

    _state.backend = backend;
    {
        const QSignalBlocker blocker(_backendCombo);
        _backendCombo->setCurrentText(_state.backend);
    }
    loadSelectedBackend();
    _pendingPickAction = pick;
    requestRender(true, false);
}

void mandelbrotgui::bindBackendCallbacks() {
    _callbacks = {};
    const uint64_t backendGeneration = _backendGeneration.load(std::memory_order_acquire);

    _callbacks.onProgress = [this, backendGeneration](const Backend::ProgressEvent &event) {
        if (backendGeneration != _backendGeneration.load(std::memory_order_acquire)) return;
        const uint64_t renderId = _callbackRenderRequestId.load(std::memory_order_acquire);
        if (renderId == 0) return;

        const auto now = std::chrono::steady_clock::now();
        const int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        if (!event.completed && event.percentage < 100) {
            const int64_t lastMs =
                _lastProgressUIUpdateMs.load(std::memory_order_acquire);
            if (lastMs > 0 && (nowMs - lastMs) < 33) {
                return;
            }
        }
        _lastProgressUIUpdateMs.store(nowMs, std::memory_order_release);

        const QString progress = QString("%1%").arg(event.percentage);
        const QString pixelsPerSecond = QString("%1 pixels/s").arg(
            QString::fromStdString(FormatUtil::formatBigNumber(event.opsPerSecond)));

        QMetaObject::invokeMethod(this, [this, progress, pixelsPerSecond, event, renderId, backendGeneration]() {
            if (backendGeneration != _backendGeneration.load(std::memory_order_acquire)) return;
            if (renderId != _callbackRenderRequestId.load(std::memory_order_acquire)) return;

            _progressText = progress;
            _progressValue = std::clamp(event.percentage, 0, 100);
            _progressActive = !event.completed;
            _progressCancelled = false;
            _pixelsPerSecondText = pixelsPerSecond;
            updateStatusLabels();
            });
        };

    _callbacks.onImage = [this, backendGeneration](const Backend::ImageEvent &event) {
        if (backendGeneration != _backendGeneration.load(std::memory_order_acquire)) return;
        const uint64_t renderId = _callbackRenderRequestId.load(std::memory_order_acquire);
        if (event.kind == Backend::ImageEventKind::allocated) {
            if (renderId == 0) return;
            const QString imageMemoryText = formatImageMemoryText(event);

            QMetaObject::invokeMethod(this, [this, imageMemoryText, backendGeneration]() {
                if (backendGeneration != _backendGeneration.load(std::memory_order_acquire)) return;
                _imageMemoryText = imageMemoryText;
                updateStatusLabels();
                });
            return;
        }

        if (event.kind != Backend::ImageEventKind::saved || !event.path) return;
        const QString path = QString::fromUtf8(event.path);

        QMetaObject::invokeMethod(this, [this, path, backendGeneration]() {
            if (backendGeneration != _backendGeneration.load(std::memory_order_acquire)) return;
            setStatusSavedPath(path);
            updateStatusLabels();
            });
        };

    _callbacks.onInfo = [this, backendGeneration](const Backend::InfoEvent &event) {
        if (backendGeneration != _backendGeneration.load(std::memory_order_acquire)) return;
        const uint64_t renderId = _callbackRenderRequestId.load(std::memory_order_acquire);
        if (renderId == 0) return;
        const QString text = QString("Iterations: %1 | %2 GI/s")
            .arg(QString::fromStdString(
                FormatUtil::formatNumber(event.totalIterations)))
            .arg(QString::number(event.opsPerSecond, 'f', 2));

        QMetaObject::invokeMethod(this, [this, text, renderId, backendGeneration]() {
            if (backendGeneration != _backendGeneration.load(std::memory_order_acquire)) return;
            if (renderId != _callbackRenderRequestId.load(std::memory_order_acquire)) return;

            setStatusMessage(text);
            updateStatusLabels();
            });
        };

    _callbacks.onDebug = [this, backendGeneration](const Backend::DebugEvent &event) {
        if (backendGeneration != _backendGeneration.load(std::memory_order_acquire)) return;
        const uint64_t renderId = _callbackRenderRequestId.load(std::memory_order_acquire);
        if (renderId == 0) return;
        if (!event.message) return;
        const QString message = QString::fromUtf8(event.message);

        QMetaObject::invokeMethod(this, [this, message, backendGeneration]() {
            if (backendGeneration != _backendGeneration.load(std::memory_order_acquire)) return;
            setStatusMessage(message);
            updateStatusLabels();
            });
        };
    _renderSession->setCallbacks(_callbacks);
}

void mandelbrotgui::startRenderWorker() {
    if (_renderThread.joinable()) return;

    {
        const std::scoped_lock lock(_renderMutex);
        _renderStopRequested = false;
        _queuedRenderRequest.reset();
    }

    const uint64_t backendGeneration = _backendGeneration.load(std::memory_order_acquire);
    _renderThread = std::thread([this, backendGeneration]() {
        for (;;) {
            if (backendGeneration != _backendGeneration.load(std::memory_order_acquire)) break;
            RenderRequest request;

            {
                std::unique_lock lock(_renderMutex);
                _renderCv.wait(lock, [this]() {
                    return _renderStopRequested || _queuedRenderRequest.has_value();
                    });

                if (_renderStopRequested) break;
                if (backendGeneration != _backendGeneration.load(std::memory_order_acquire)) break;

                request = std::move(*_queuedRenderRequest);
                _queuedRenderRequest.reset();
            }

            if (_interactionPreviewFallbackLatched.load(std::memory_order_acquire)) {
                std::unique_lock lock(_renderMutex);
                _renderCv.wait_for(lock,
                    std::chrono::milliseconds(interactionFrameIntervalMs()),
                    [this]() {
                        return _renderStopRequested || _queuedRenderRequest.has_value();
                    });

                if (_renderStopRequested) break;
                if (backendGeneration != _backendGeneration.load(std::memory_order_acquire)) break;
                if (_queuedRenderRequest) {
                    request = std::move(*_queuedRenderRequest);
                    _queuedRenderRequest.reset();
                }
            }

            _callbackRenderRequestId.store(request.id, std::memory_order_release);

            QMetaObject::invokeMethod(this, [this, requestId = request.id, backendGeneration]() {
                if (backendGeneration != _backendGeneration.load(std::memory_order_acquire)) return;
                if (requestId != _callbackRenderRequestId.load(std::memory_order_acquire)) return;

                _renderInFlight = true;
                _progressActive = true;
                _progressCancelled = false;
                _progressValue = 0;
                _progressText = "Rendering";
                _pixelsPerSecondText = "0 pixels/s";
                _lastProgressUIUpdateMs.store(0, std::memory_order_release);
                updateStatusLabels();
                });

            QString error;
            int rank = 0;
            Backend::Status status;
            Backend::ImageView view;
            auto renderStart = std::chrono::steady_clock::now();
            auto renderEnd = renderStart;
            {
                if (!applyStateToSession(
                    request.state,
                    request.pointRealText,
                    request.pointImagText,
                    request.zoomText,
                    request.seedRealText,
                    request.seedImagText,
                    request.pickAction,
                    error)) {
                    status = Backend::Status::failure(error.toStdString());
                } else {
                    rank = _renderSession->precisionRank();
                    if (!request.state.manualBackendSelection &&
                        backendForRank(rank) != request.state.backend) {
                        status = Backend::Status::success();
                    } else {
                        renderStart = std::chrono::steady_clock::now();
                        status = _renderSession->render();
                        renderEnd = std::chrono::steady_clock::now();
                    }
                }
            }

            const bool staleRequest =
                backendGeneration != _backendGeneration.load(std::memory_order_acquire) ||
                request.id != _callbackRenderRequestId.load(std::memory_order_acquire) ||
                request.id != _latestRenderRequestId.load(std::memory_order_acquire);
            if (staleRequest) {
                continue;
            }

            if (!status) {
                const QString failureMessage = error.isEmpty() ?
                    QString::fromStdString(status.message) :
                    error;
                {
                    const std::scoped_lock lock(_renderMutex);
                    _queuedRenderRequest.reset();
                }

                QMetaObject::invokeMethod(this, [this, failureMessage, requestId = request.id, backendGeneration]() {
                    if (backendGeneration != _backendGeneration.load(std::memory_order_acquire)) return;
                    if (requestId != _latestRenderRequestId.load(std::memory_order_acquire)) return;

                    _renderInFlight = false;
                    _interactionPreviewForced.store(false, std::memory_order_release);
                    _progressActive = false;
                    _progressCancelled = false;
                    _progressValue = 0;
                    _progressText.clear();
                    _pixelsPerSecondText = "0 pixels/s";
                    handleRenderFailure(failureMessage);
                    updateStatusLabels();
                    });
                continue;
            }

            view = _renderSession->imageView();
            if (!request.state.manualBackendSelection &&
                backendForRank(rank) != request.state.backend) {
                QMetaObject::invokeMethod(this, [this,
                    rank,
                    requestId = request.id,
                    backendGeneration,
                    pick = request.pickAction]() {
                        switchBackend(rank, requestId, backendGeneration, pick);
                    });
                continue;
            }

            const auto renderElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                renderEnd - renderStart);
            const qint64 renderMs = std::max<qint64>(1, renderElapsed.count());
            const double renderFPS = 1000.0 / static_cast<double>(renderMs);
            _lastRenderDurationMs.store(
                static_cast<int32_t>(renderMs),
                std::memory_order_release);
            const int targetFrameMs = interactionFrameIntervalMs();
            const int recoveryFrameMs = std::max(1, targetFrameMs / 2);
            bool previewFallbackLatched =
                _interactionPreviewFallbackLatched.load(std::memory_order_acquire);
            if (previewFallbackLatched) {
                if (renderMs <= recoveryFrameMs) {
                    previewFallbackLatched = false;
                }
            } else if (renderMs > targetFrameMs) {
                previewFallbackLatched = true;
            }
            _interactionPreviewFallbackLatched.store(
                previewFallbackLatched,
                std::memory_order_release);

            bool hasQueuedRender = false;
            {
                const std::scoped_lock lock(_renderMutex);
                hasQueuedRender = _queuedRenderRequest.has_value();
            }
            const bool canUseDirectPreview =
                !hasQueuedRender &&
                !previewFallbackLatched;
            const UIState renderedState = request.state;
            const QString renderedPointReal = request.pointRealText;
            const QString renderedPointImag = request.pointImagText;
            const QString renderedZoomText = request.zoomText;
            if (backendGeneration != _backendGeneration.load(std::memory_order_acquire)) {
                continue;
            }
            QMetaObject::invokeMethod(this, [this,
                view,
                canUseDirectPreview,
                renderFPS,
                renderMs,
                renderedState,
                renderedPointReal,
                renderedPointImag,
                renderedZoomText,
                requestId = request.id,
                backendGeneration]() {
                    if (backendGeneration != _backendGeneration.load(std::memory_order_acquire)) return;
                    if (requestId <= _lastPresentedRenderId) return;
                    _lastPresentedRenderId = requestId;

                    const bool currentRender =
                        requestId == _callbackRenderRequestId.load(std::memory_order_acquire);
                    const bool latestRender =
                        requestId == _latestRenderRequestId.load(std::memory_order_acquire);
                    if (currentRender) {
                        _renderInFlight = false;
                        _progressActive = false;
                        _progressCancelled = false;
                        _progressValue = 0;
                        _progressText.clear();
                    }
                    _lastRenderFailureMessage.clear();
                    _viewportFPSText = QString("FPS %1")
                        .arg(QString::number(renderFPS, 'f', 1));
                    _viewportRenderTimeText = QString::fromStdString(
                        FormatUtil::formatDuration(renderMs));
                    const bool presentDirect =
                        canUseDirectPreview &&
                        latestRender &&
                        !_interactionPreviewForced.load(std::memory_order_acquire);
                    _previewImage = presentDirect ?
                        wrapImageViewToImage(view) :
                        imageViewToImage(view);
                    _previewUsesBackendMemory = presentDirect;
                    const QWidget *dprSource = _viewport ? _viewport : this;
                    _previewImage.setDevicePixelRatio(effectiveDevicePixelRatio(dprSource));
                    _interactionPreviewForced.store(false, std::memory_order_release);
                    _displayedPointRealText = renderedPointReal;
                    _displayedPointImagText = renderedPointImag;
                    _displayedZoomText = renderedZoomText;
                    _displayedOutputSize = QSize(
                        renderedState.outputWidth,
                        renderedState.outputHeight);
                    _hasDisplayedViewState = true;
                    updateViewport();
                    updateStatusLabels();
                }, Qt::BlockingQueuedConnection);
        }
        });
}

bool mandelbrotgui::ensureBackendReady(QString &errorMessage) const {
    if (_backend && _renderSession) return true;

    errorMessage = "Backend not loaded.";
    return false;
}

bool mandelbrotgui::ensureNavigationSessionReady(QString &errorMessage) const {
    if (_navigationSession) return true;

    errorMessage = "Navigation session unavailable.";
    return false;
}

bool mandelbrotgui::applyNavigationStateToSession(QString &errorMessage) {
    if (!ensureNavigationSessionReady(errorMessage)) return false;

    auto failIfNeeded = [&errorMessage](const Backend::Status &status) {
        if (status) return false;
        errorMessage = QString::fromStdString(status.message);
        return true;
        };

    if (failIfNeeded(_navigationSession->setImageSize(
        _state.outputWidth, _state.outputHeight, _state.aaPixels))) {
        return false;
    }
    _navigationSession->setUseThreads(_state.useThreads);
    if (failIfNeeded(_navigationSession->setZoom(
        _state.iterations,
        _zoomText.toStdString()))) {
        return false;
    }
    if (failIfNeeded(_navigationSession->setPoint(
        _pointRealText.toStdString(),
        _pointImagText.toStdString()))) {
        return false;
    }
    if (failIfNeeded(_navigationSession->setSeed(
        _seedRealText.toStdString(),
        _seedImagText.toStdString()))) {
        return false;
    }
    return true;
}

bool mandelbrotgui::backendPanPointByDelta(const QPoint &delta,
    QString &realText, QString &imagText, QString &errorMessage) {
    if (!applyNavigationStateToSession(errorMessage)) return false;

    std::string real;
    std::string imag;
    const Backend::Status status = _navigationSession->getPanPointByDelta(
        delta.x(), delta.y(), real, imag);
    if (!status) {
        errorMessage = QString::fromStdString(status.message);
        return false;
    }

    realText = QString::fromStdString(real);
    imagText = QString::fromStdString(imag);
    return true;
}

bool mandelbrotgui::backendPointAtPixel(const QPoint &pixel,
    QString &realText, QString &imagText, QString &errorMessage) {
    if (!applyNavigationStateToSession(errorMessage)) return false;

    std::string real;
    std::string imag;
    const Backend::Status status = _navigationSession->getPointAtPixel(
        pixel.x(), pixel.y(), real, imag);
    if (!status) {
        errorMessage = QString::fromStdString(status.message);
        return false;
    }

    realText = QString::fromStdString(real);
    imagText = QString::fromStdString(imag);
    return true;
}

bool mandelbrotgui::backendZoomViewAtPixel(const QPoint &pixel,
    double scaleMultiplier,
    ViewTextState &view, QString &errorMessage) {
    if (!applyNavigationStateToSession(errorMessage)) return false;

    std::string zoom;
    std::string real;
    std::string imag;
    const Backend::Status status = _navigationSession->getZoomPointByScale(
        pixel.x(), pixel.y(), scaleMultiplier, zoom, real, imag);
    if (!status) {
        errorMessage = QString::fromStdString(status.message);
        return false;
    }

    view.pointReal = QString::fromStdString(real);
    view.pointImag = QString::fromStdString(imag);
    view.zoomText = QString::fromStdString(zoom);
    view.outputSize = outputSize();
    view.valid = true;
    return true;
}

bool mandelbrotgui::backendBoxZoomView(const QRect &selectionRect,
    ViewTextState &view, QString &errorMessage) {
    if (!applyNavigationStateToSession(errorMessage)) return false;

    const QRect rect = selectionRect.normalized();
    std::string zoom;
    std::string real;
    std::string imag;
    const Backend::Status status = _navigationSession->getBoxZoomPoint(
        rect.left(), rect.top(), rect.right(), rect.bottom(),
        zoom, real, imag);
    if (!status) {
        errorMessage = QString::fromStdString(status.message);
        return false;
    }

    view.pointReal = QString::fromStdString(real);
    view.pointImag = QString::fromStdString(imag);
    view.zoomText = QString::fromStdString(zoom);
    view.outputSize = outputSize();
    view.valid = true;
    return true;
}

bool mandelbrotgui::applyStateToSession(const UIState &state,
    const QString &pointRealText,
    const QString &pointImagText,
    const QString &zoomText,
    const QString &seedRealText,
    const QString &seedImagText,
    const std::optional<PendingPickAction> &pickAction,
    QString &errorMessage) {
    if (!ensureBackendReady(errorMessage)) return false;

    auto failIfNeeded = [&errorMessage](const Backend::Status &status) {
        if (status) return false;
        errorMessage = QString::fromStdString(status.message);
        return true;
        };

    if (failIfNeeded(_renderSession->setImageSize(
        state.outputWidth, state.outputHeight, state.aaPixels)))
        return false;
    _renderSession->setUseThreads(state.useThreads);
    if (failIfNeeded(_renderSession->setZoom(
        state.iterations,
        zoomText.toStdString())))
        return false;
    if (failIfNeeded(_renderSession->setPoint(
        pointRealText.toStdString(),
        pointImagText.toStdString())))
        return false;
    if (failIfNeeded(_renderSession->setSeed(
        seedRealText.toStdString(),
        seedImagText.toStdString())))
        return false;
    if (failIfNeeded(_renderSession->setFractalType(state.fractalType)))
        return false;
    _renderSession->setFractalMode(state.julia, state.inverse);
    if (failIfNeeded(_renderSession->setFractalExponent(
        stateToString(state.exponent).toStdString())))
        return false;
    if (failIfNeeded(_renderSession->setColorMethod(state.colorMethod)))
        return false;
    if (failIfNeeded(_renderSession->setSinePalette(state.sinePalette)))
        return false;
    if (failIfNeeded(_renderSession->setColorPalette(state.palette)))
        return false;
    if (failIfNeeded(_renderSession->setLight(
        static_cast<float>(state.light.x()),
        static_cast<float>(state.light.y()))))
        return false;
    if (failIfNeeded(_renderSession->setLightColor(state.lightColor)))
        return false;

    if (pickAction) {
        Backend::Status status;
        switch (pickAction->target) {
            case SelectionTarget::zoomPoint:
                status = _renderSession->setPoint(
                    pickAction->pixel.x(), pickAction->pixel.y());
                break;
            case SelectionTarget::seedPoint:
                status = _renderSession->setSeed(
                    pickAction->pixel.x(), pickAction->pixel.y());
                break;
            case SelectionTarget::lightPoint:
                status = _renderSession->setLight(
                    pickAction->pixel.x(), pickAction->pixel.y());
                break;
        }

        if (failIfNeeded(status)) return false;
    }

    return true;
}

void mandelbrotgui::finishRenderThread(bool forceKillOnTimeout, int timeoutMs) {
    _latestRenderRequestId.fetch_add(1, std::memory_order_acq_rel);
    _lastPresentedRenderId = 0;
    _callbackRenderRequestId.store(0, std::memory_order_release);

    {
        const std::scoped_lock lock(_renderMutex);
        _renderStopRequested = true;
        _queuedRenderRequest.reset();
    }
    _renderCv.notify_all();

    if (_renderThread.joinable()) {
#ifdef _WIN32
        HANDLE renderThreadHandle =
            static_cast<HANDLE>(_renderThread.native_handle());
        const auto waitForThread = [renderThreadHandle](int waitMs) {
            if (!renderThreadHandle) return true;
            const DWORD waitResult = WaitForSingleObject(
                renderThreadHandle,
                waitMs <= 0 ? 0u : static_cast<DWORD>(waitMs));
            return waitResult == WAIT_OBJECT_0;
        };

        const int boundedWaitMs = std::max(0, timeoutMs);
        bool stopped = waitForThread(boundedWaitMs);
        if (!stopped && forceKillOnTimeout) {
            _backend.forceKill();
            stopped = waitForThread(std::max(250, boundedWaitMs));
            if (!stopped && renderThreadHandle) {
                TerminateThread(renderThreadHandle, 1);
                stopped = waitForThread(1000);
            }
        } else if (!stopped) {
            while (!stopped) {
                stopped = waitForThread(250);
            }
        }
#else
        (void)forceKillOnTimeout;
        (void)timeoutMs;
#endif
        _renderThread.join();
    }

    {
        const std::scoped_lock lock(_renderMutex);
        _renderStopRequested = false;
        _queuedRenderRequest.reset();
    }

    _renderInFlight = false;
    _progressActive = false;
    _progressCancelled = false;
    _progressValue = 0;
    _progressText.clear();
    _pixelsPerSecondText = "0 pixels/s";
    _lastRenderDurationMs.store(0, std::memory_order_release);
}

void mandelbrotgui::requestRender(bool force, bool syncControls) {
    (void)force;
    if (syncControls) {
        syncStateFromControls();
    }
    _state.zoom = clampGUIZoom(_state.zoom);
    syncStateReadouts();
    if (!_backend || !_renderSession) return;
    const auto pickAction = _pendingPickAction;
    _pendingPickAction.reset();
    const uint64_t requestId =
        _latestRenderRequestId.fetch_add(1, std::memory_order_acq_rel) + 1;

    {
        const std::scoped_lock lock(_renderMutex);
        _queuedRenderRequest = RenderRequest{
            .state = _state,
            .pointRealText = _pointRealText,
            .pointImagText = _pointImagText,
            .zoomText = _zoomText,
            .seedRealText = _seedRealText,
            .seedImagText = _seedImagText,
            .pickAction = pickAction,
            .id = requestId
        };
    }

    const bool wasInFlight = _renderInFlight;
    prepareInteractionPreview(false);
    _renderInFlight = true;
    if (!wasInFlight) {
        _progressActive = true;
        _progressCancelled = false;
        _progressValue = 0;
        _progressText = "Rendering";
        _pixelsPerSecondText = "0 pixels/s";
        updateStatusLabels();
    }

    _renderCv.notify_one();
}

void mandelbrotgui::applyHomeView() {
    _state.iterations = 0;
    _state.zoom = -0.65;
    _state.point = QPointF(0.0, 0.0);
    _state.seed = QPointF(0.0, 0.0);
    _state.light = QPointF(1.0, 1.0);
    syncZoomTextFromState();
    syncPointTextFromState();
    syncSeedTextFromState();
    syncControlsFromState();
    requestRender(true);
}

void mandelbrotgui::scaleAtPixel(
    const QPoint &pixel,
    double scaleMultiplier,
    bool realtimeStep) {
    if (!(scaleMultiplier > 0.0) || !std::isfinite(scaleMultiplier)) return;

    ViewTextState nextView;
    QString error;
    if (!backendZoomViewAtPixel(clampPixelToOutput(pixel),
        scaleMultiplier, nextView, error)) {
        setStatusMessage(error);
        updateStatusLabels();
        return;
    }
    if (NumberUtil::equalParsedDoubleText(
        _zoomText.toStdString(), nextView.zoomText.toStdString()) &&
        _pointRealText == nextView.pointReal &&
        _pointImagText == nextView.pointImag) {
        return;
    }

    _pointRealText = nextView.pointReal;
    _pointImagText = nextView.pointImag;
    _zoomText = nextView.zoomText;
    syncStatePointFromText();
    syncStateZoomFromText();
    prepareInteractionPreview(realtimeStep);
    if (_pendingPickAction &&
        _pendingPickAction->target == SelectionTarget::zoomPoint) {
        _pendingPickAction.reset();
    }
    syncStateReadouts();
    requestViewportRepaint();
    requestRender(false, false);
}

void mandelbrotgui::zoomAtPixel(const QPoint &pixel, bool zoomIn, bool realtimeStep) {
    const QPoint clampedPixel = clampPixelToOutput(pixel);
    const double factor = currentZoomFactor(_state.zoomRate);
    const double scaleMultiplier = zoomIn ? (1.0 / factor) : factor;
    scaleAtPixel(clampedPixel, scaleMultiplier, realtimeStep);
}

void mandelbrotgui::boxZoom(const QRect &selectionRect) {
    const QRect rect = selectionRect.normalized();
    if (rect.width() < 2 || rect.height() < 2) {
        zoomAtPixel(rect.center(), true);
        return;
    }

    ViewTextState nextView;
    QString error;
    if (!backendBoxZoomView(rect, nextView, error)) {
        setStatusMessage(error);
        updateStatusLabels();
        return;
    }

    _pointRealText = nextView.pointReal;
    _pointImagText = nextView.pointImag;
    _zoomText = nextView.zoomText;
    syncStatePointFromText();
    syncStateZoomFromText();
    prepareInteractionPreview(true);
    syncStateReadouts();
    requestViewportRepaint();
    requestRender(false, false);
}

void mandelbrotgui::panByPixels(const QPoint &delta) {
    if (delta.isNull()) return;

    QString pointReal;
    QString pointImag;
    QString error;
    if (!backendPanPointByDelta(delta, pointReal, pointImag, error)) {
        setStatusMessage(error);
        updateStatusLabels();
        return;
    }

    _pointRealText = pointReal;
    _pointImagText = pointImag;
    syncStatePointFromText();
    syncStateReadouts();
    requestViewportRepaint();
    requestRender(false, false);
}

void mandelbrotgui::pickAtPixel(const QPoint &pixel) {
    const QPoint clampedPixel = clampPixelToOutput(pixel);
    switch (_selectionTarget) {
        case SelectionTarget::zoomPoint:
        {
            QString real;
            QString imag;
            QString error;
            if (!backendPointAtPixel(clampedPixel, real, imag, error)) {
                setStatusMessage(error);
                updateStatusLabels();
                return;
            }
            _pointRealText = real;
            _pointImagText = imag;
            syncStatePointFromText();
            break;
        }
        case SelectionTarget::seedPoint:
        {
            QString real;
            QString imag;
            QString error;
            if (!backendPointAtPixel(clampedPixel, real, imag, error)) {
                setStatusMessage(error);
                updateStatusLabels();
                return;
            }
            _seedRealText = real;
            _seedImagText = imag;
            syncStateSeedFromText();
            break;
        }
        case SelectionTarget::lightPoint:
        {
            QString real;
            QString imag;
            QString error;
            if (!backendPointAtPixel(clampedPixel, real, imag, error)) {
                setStatusMessage(error);
                updateStatusLabels();
                return;
            }

            bool okReal = false, okImag = false;
            const double lightReal = real.toDouble(&okReal);
            const double lightImag = imag.toDouble(&okImag);
            if (okReal) _state.light.setX(lightReal);
            if (okImag) _state.light.setY(lightImag);
            break;
        }
    }

    if (_selectionTarget == SelectionTarget::zoomPoint) {
        _pendingPickAction.reset();
    } else {
        _pendingPickAction = PendingPickAction{ _selectionTarget, clampedPixel };
    }
    syncStateReadouts();
    requestRender(false, false);
}

void mandelbrotgui::updateMouseCoords(const QPoint &pixel) {
    _lastMousePixel = clampPixelToOutput(pixel);
    QString real;
    QString imag;
    QString error;
    if (!backendPointAtPixel(_lastMousePixel, real, imag, error)) {
        _mouseText = QString("Mouse: %1, %2  |  -")
            .arg(_lastMousePixel.x())
            .arg(_lastMousePixel.y());
        requestViewportRepaint();
        return;
    }

    _mouseText = QString("Mouse: %1, %2  |  %3  %4")
        .arg(_lastMousePixel.x())
        .arg(_lastMousePixel.y())
        .arg(real)
        .arg(imag);
    requestViewportRepaint();
}

void mandelbrotgui::clearMouseCoords() {
    _mouseText = "Mouse: -";
    requestViewportRepaint();
}

void mandelbrotgui::onViewportClosed() {
    requestApplicationShutdown(false);
}

void mandelbrotgui::adjustIterationsBy(int delta) {
    if (!_iterationsSpin) return;
    _iterationsSpin->setValue(std::max(0, _iterationsSpin->value() + delta));
}

void mandelbrotgui::cycleNavMode() {
    if (!_navModeCombo) return;
    _navModeCombo->setCurrentIndex((_navModeCombo->currentIndex() + 1) % 3);
}

void mandelbrotgui::cycleViewportGrid() {
    if (!_viewport) return;
    static_cast<ViewportWindow *>(_viewport)->cycleGridMode();
}

void mandelbrotgui::toggleViewportMinimalUI() {
    if (!_viewport) return;
    static_cast<ViewportWindow *>(_viewport)->toggleMinimalUI();
}

void mandelbrotgui::toggleViewportFullscreen() {
    auto *viewport = _viewport ? static_cast<ViewportWindow *>(_viewport) : nullptr;
    if (!viewport) return;

    // Detach from any backend-owned preview before changing viewport size/state.
    freezeViewportPreview(false, true);
    prepareInteractionPreview(true);

    if (!viewport->isFullScreen()) {
        viewport->_restoreGeometry = viewport->geometry();
        viewport->_restoreMaximized = viewport->isMaximized();
        viewport->_restoreOutputSize = outputSize();
        viewport->_fullscreenManaged = true;
        viewport->_fullscreenTransitionPending = true;
        viewport->setMinimumSize(1, 1);
        viewport->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        viewport->showFullScreen();
        viewport->raise();
        viewport->activateWindow();
        viewport->setFocus(Qt::ActiveWindowFocusReason);
        viewport->_fullscreenResizeTimer.start();
        return;
    }

    viewport->_fullscreenManaged = false;
    viewport->_fullscreenTransitionPending = true;
    if (viewport->_restoreMaximized) {
        viewport->showMaximized();
    } else {
        viewport->showNormal();
        if (viewport->_restoreGeometry.isValid()) {
            viewport->setGeometry(viewport->_restoreGeometry);
        }
    }
    viewport->_fullscreenResizeTimer.start();
}

void mandelbrotgui::finalizeViewportFullscreenTransition() {
    auto *viewport = _viewport ? static_cast<ViewportWindow *>(_viewport) : nullptr;
    if (!viewport || !viewport->_fullscreenTransitionPending) return;

    viewport->_fullscreenTransitionPending = false;
    if (viewport->isFullScreen()) {
        const double dpr = effectiveDevicePixelRatio(viewport);
        const QSize logicalSize = viewport->size();
        _state.outputWidth = std::max(1,
            static_cast<int>(std::lround(logicalSize.width() * dpr)));
        _state.outputHeight = std::max(1,
            static_cast<int>(std::lround(logicalSize.height() * dpr)));
        syncControlsFromState();
        requestRender(true, false);
        return;
    }

    if (viewport->_restoreOutputSize.isValid()) {
        _state.outputWidth = viewport->_restoreOutputSize.width();
        _state.outputHeight = viewport->_restoreOutputSize.height();
    }
    syncControlsFromState();
    resizeViewportWindowToImageSize();
    requestRender(true, false);
}

mandelbrotgui::NavMode mandelbrotgui::displayedNavMode() const {
    return _displayedNavModeOverride.value_or(_navMode);
}

void mandelbrotgui::setDisplayedNavModeOverride(std::optional<NavMode> mode) {
    if (_displayedNavModeOverride == mode) return;

    _displayedNavModeOverride = mode;
    syncControlsFromState();
}

void mandelbrotgui::cancelQueuedRenders() {
    const int cancelledProgressValue = _progressValue;
    _pendingPickAction.reset();
    freezeViewportPreview(true, true);

    const uint64_t cancelledRequestBoundary =
        _latestRenderRequestId.fetch_add(1, std::memory_order_acq_rel) + 1;
    _lastPresentedRenderId = std::max(_lastPresentedRenderId, cancelledRequestBoundary);
    _callbackRenderRequestId.store(0, std::memory_order_release);
    {
        const std::scoped_lock lock(_renderMutex);
        _queuedRenderRequest.reset();
    }
    _renderCv.notify_all();
    if (_backend) {
        _backend.forceKill();
    }
    _interactionPreviewFallbackLatched.store(false, std::memory_order_release);
    _interactionPreviewForced.store(false, std::memory_order_release);
    _lastRenderFailureMessage.clear();
    _renderInFlight = false;
    _progressValue = std::clamp(cancelledProgressValue, 0, 100);
    _progressActive = false;
    _progressCancelled = true;
    _progressText.clear();
    _pixelsPerSecondText = "0 pixels/s";
    setStatusMessage("Render cancelled");
    updateStatusLabels();
    if (!_closing) {
        setViewportInteractive(true);
        requestViewportRepaint();
    }
}

QSize mandelbrotgui::outputSize() const {
    return { _state.outputWidth, _state.outputHeight };
}

mandelbrotgui::ViewTextState mandelbrotgui::currentViewTextState() const {
    const QSize size = outputSize();
    return {
        .pointReal = _pointRealText,
        .pointImag = _pointImagText,
        .zoomText = _zoomText,
        .outputSize = size,
        .valid = size.width() > 0 && size.height() > 0
    };
}

mandelbrotgui::ViewTextState mandelbrotgui::displayedViewTextState() const {
    if (!_hasDisplayedViewState) {
        return {};
    }

    return {
        .pointReal = _displayedPointRealText,
        .pointImag = _displayedPointImagText,
        .zoomText = _displayedZoomText,
        .outputSize = _displayedOutputSize,
        .valid = _displayedOutputSize.width() > 0 && _displayedOutputSize.height() > 0
    };
}

bool mandelbrotgui::previewPannedViewState(const QPoint &delta,
    ViewTextState &view, QString &errorMessage) {
    view = currentViewTextState();
    if (!view.valid || delta.isNull()) {
        return view.valid;
    }

    QString real;
    QString imag;
    if (!backendPanPointByDelta(delta, real, imag, errorMessage)) {
        view = {};
        return false;
    }

    view.pointReal = real;
    view.pointImag = imag;
    return true;
}

bool mandelbrotgui::previewScaledViewState(const QPoint &pixel,
    double scaleMultiplier, ViewTextState &view, QString &errorMessage) {
    if (NumberUtil::almostEqual(scaleMultiplier, 1.0)) {
        view = currentViewTextState();
        return view.valid;
    }

    return backendZoomViewAtPixel(clampPixelToOutput(pixel),
        scaleMultiplier, view, errorMessage);
}

bool mandelbrotgui::previewBoxZoomViewState(const QRect &selectionRect,
    ViewTextState &view, QString &errorMessage) {
    const QRect rect = selectionRect.normalized();
    if (rect.width() < 2 || rect.height() < 2) {
        view = currentViewTextState();
        return view.valid;
    }

    return backendBoxZoomView(rect, view, errorMessage);
}

bool mandelbrotgui::mapViewPixelToViewPixel(const ViewTextState &sourceView,
    const ViewTextState &targetView, const QPoint &pixel,
    QPointF &mappedPixel, QString &errorMessage) {
    if (!ensureNavigationSessionReady(errorMessage)) {
        return false;
    }

    Backend::ViewportState source{
        .outputWidth = sourceView.outputSize.width(),
        .outputHeight = sourceView.outputSize.height(),
        .pointReal = sourceView.pointReal.toStdString(),
        .pointImag = sourceView.pointImag.toStdString(),
        .zoom = sourceView.zoomText.toStdString()
    };
    Backend::ViewportState target{
        .outputWidth = targetView.outputSize.width(),
        .outputHeight = targetView.outputSize.height(),
        .pointReal = targetView.pointReal.toStdString(),
        .pointImag = targetView.pointImag.toStdString(),
        .zoom = targetView.zoomText.toStdString()
    };

    double mappedX = 0.0;
    double mappedY = 0.0;
    const Backend::Status status = _navigationSession->mapViewPixelToViewPixel(
        source, target, pixel.x(), pixel.y(), mappedX, mappedY);
    if (!status) {
        errorMessage = QString::fromStdString(status.message);
        return false;
    }

    mappedPixel = QPointF(mappedX, mappedY);
    return true;
}

QString mandelbrotgui::viewportStatusText() const {
    QString text;
    if (!_viewportFPSText.isEmpty()) {
        text = _viewportFPSText;
        if (!_viewportRenderTimeText.isEmpty()) {
            text += QString("  |  %1").arg(_viewportRenderTimeText);
        }
    }
    if (!_mouseText.isEmpty()) {
        text = text.isEmpty() ? _mouseText :
            QString("%1\n%2").arg(text, _mouseText);
    }
    return text;
}

bool mandelbrotgui::eventFilter(QObject *watched, QEvent *event) {
    if (_viewport &&
        (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        const bool routePress =
            event->type() == QEvent::KeyPress &&
            (keyEvent->key() == Qt::Key_Escape ||
                keyEvent->key() == Qt::Key_Z ||
                keyEvent->key() == Qt::Key_H ||
                keyEvent->key() == Qt::Key_G ||
                keyEvent->key() == Qt::Key_F1 ||
                keyEvent->key() == Qt::Key_F2 ||
                keyEvent->key() == Qt::Key_F12 ||
                keyEvent->key() == Qt::Key_Left ||
                keyEvent->key() == Qt::Key_Right ||
                keyEvent->key() == Qt::Key_Up ||
                keyEvent->key() == Qt::Key_Down);
        const bool routeRelease =
            event->type() == QEvent::KeyRelease &&
            (keyEvent->key() == Qt::Key_Left ||
                keyEvent->key() == Qt::Key_Right ||
                keyEvent->key() == Qt::Key_Up ||
                keyEvent->key() == Qt::Key_Down);
        if (routePress || routeRelease) {
            QWidget *targetWidget = qobject_cast<QWidget *>(watched);
            const bool targetIsViewport =
                targetWidget &&
                (targetWidget == _viewport || _viewport->isAncestorOf(targetWidget));
            if (!targetIsViewport) {
                QKeyEvent forwarded(
                    keyEvent->type(),
                    keyEvent->key(),
                    keyEvent->modifiers(),
                    keyEvent->text(),
                    keyEvent->isAutoRepeat(),
                    keyEvent->count());
                QApplication::sendEvent(_viewport, &forwarded);
                if (forwarded.isAccepted()) {
                    return true;
                }
            }
        }
    }

    QLineEdit *iterationsEdit = _iterationsSpin ?
        _iterationsSpin->findChild<QLineEdit *>() : nullptr;
    if (iterationsEdit && watched == iterationsEdit &&
        event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        const QString text = iterationsEdit->text().trimmed();
        if (text.compare(_iterationsSpin->specialValueText(), Qt::CaseInsensitive) == 0) {
            const bool printableInput =
                !keyEvent->text().isEmpty() &&
                !keyEvent->text().at(0).isSpace() &&
                !(keyEvent->modifiers() &
                    (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));
            if (keyEvent->key() == Qt::Key_Backspace ||
                keyEvent->key() == Qt::Key_Delete ||
                printableInput) {
                iterationsEdit->selectAll();
            }
        }
    }

    if (_statusRightLabel && watched == _statusRightLabel &&
        event->type() == QEvent::Resize) {
        updateStatusRightEdgeAlignment();
        updateStatusLabels();
    }

    return QWidget::eventFilter(watched, event);
}

void mandelbrotgui::closeEvent(QCloseEvent *event) {
    event->accept();
    requestApplicationShutdown(true);
}

void mandelbrotgui::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);

    if (_controlWindowSized) return;

    updateControlWindowSize();
    positionWindowsForInitialShow();
    _controlWindowSized = true;
    QTimer::singleShot(0, this, [this]() {
        if (!_viewport || !_viewport->isVisible()) return;
        _viewport->raise();
        _viewport->activateWindow();
        _viewport->setFocus(Qt::ActiveWindowFocusReason);
        });
}

void mandelbrotgui::requestApplicationShutdown(bool closeViewportWindow) {
    if (_shutdownQueued || _closing) return;
    _shutdownQueued = true;

    setEnabled(false);
    hide();
    if (_viewport) {
        _viewport->setEnabled(false);
        _viewport->hide();
    }

    QTimer::singleShot(0, this, [this, closeViewportWindow]() {
        shutdownForExit(closeViewportWindow);
        QApplication::quit();
    });
}

void mandelbrotgui::shutdownForExit(bool closeViewportWindow) {
    if (_closing) return;
    _closing = true;
    _shutdownQueued = false;
    _backendGeneration.fetch_add(1, std::memory_order_acq_rel);
    _callbackRenderRequestId.store(0, std::memory_order_release);

    freezeViewportPreview(true, true);
    _pendingPickAction.reset();
    if (_backend && _renderSession) {
        _renderSession->setCallbacks({});
    }
    if (_backend) {
        _backend.forceKill();
    }

    if (closeViewportWindow && _viewport) {
        QWidget *viewport = _viewport;
        _viewport = nullptr;
        viewport->hide();
        viewport->deleteLater();
    }

    finishRenderThread(true, kForceKillDelayMs);
}

void mandelbrotgui::freezeViewportPreview(bool disableViewport,
    bool clearInteractionPreview) {
    if (disableViewport) {
        setViewportInteractive(false);
    }

    _previewImage = makeBlankViewportImage();
    _previewUsesBackendMemory = false;
    const QWidget *dprSource = _viewport ? _viewport : this;
    _previewImage.setDevicePixelRatio(effectiveDevicePixelRatio(dprSource));

    if (clearInteractionPreview) {
        _interactionPreviewForced.store(false, std::memory_order_release);
    }
    if (_viewport && clearInteractionPreview) {
        static_cast<ViewportWindow *>(_viewport)->clearPreviewOffset();
    }
    requestViewportRepaint();
}

void mandelbrotgui::setViewportInteractive(bool enabled) {
    if (!_viewport) return;
    _viewport->setEnabled(enabled);
}

void mandelbrotgui::updatePreview(const QImage &image,
    bool clearInteractionPreview,
    bool usesBackendMemory) {
    if (image.isNull()) return;

    _previewImage = image;
    _previewUsesBackendMemory = usesBackendMemory;
    const QWidget *dprSource = _viewport ? _viewport : this;
    _previewImage.setDevicePixelRatio(effectiveDevicePixelRatio(dprSource));
    if (clearInteractionPreview) {
        _interactionPreviewForced.store(false, std::memory_order_release);
    }
    if (_viewport && clearInteractionPreview) {
        static_cast<ViewportWindow *>(_viewport)->clearPreviewOffset();
    }
    updateViewport();
}

void mandelbrotgui::prepareInteractionPreview(bool forceFallbackPreview) {
    if (!_previewImage.isNull() && _previewUsesBackendMemory) {
        _previewImage = _previewImage.copy();
        _previewUsesBackendMemory = false;
        const QWidget *dprSource = _viewport ? _viewport : this;
        _previewImage.setDevicePixelRatio(effectiveDevicePixelRatio(dprSource));
    }

    if (forceFallbackPreview && !_previewImage.isNull()) {
        _interactionPreviewForced.store(true, std::memory_order_release);
    }
}

void mandelbrotgui::updateStatusRightEdgeAlignment() {
    if (!_saveButton || !_statusRightLabel) return;
    constexpr int statusRowLeftShift = 3;

    QWidget *statusPanel = _statusRightLabel->parentWidget();
    if (!statusPanel) return;

    auto *statusRow = qobject_cast<QHBoxLayout *>(statusPanel->layout());
    if (!statusRow) return;

    const QPoint saveTopLeft = _saveButton->mapTo(this, QPoint(0, 0));
    const int targetRight = saveTopLeft.x() + _saveButton->width() - statusRowLeftShift;
    const QPoint panelTopLeft = statusPanel->mapTo(this, QPoint(0, 0));
    const int panelRight = panelTopLeft.x() + statusPanel->width();
    const int rightInset = std::max(0, panelRight - targetRight);
    const int leftInset = -statusRowLeftShift;

    const QMargins margins = statusRow->contentsMargins();
    if (margins.left() == leftInset && margins.right() == rightInset) return;

    statusRow->setContentsMargins(
        leftInset,
        margins.top(),
        rightInset,
        margins.bottom());
}

void mandelbrotgui::setStatusMessage(const QString &message) {
    _statusText = message;
    _statusLinkPath.clear();
    _statusMarqueeSourceText.clear();
    _statusMarqueeOffset = 0;
    updateStatusLabels();
}

void mandelbrotgui::setStatusSavedPath(const QString &path) {
    const QString absolutePath = QFileInfo(path).absoluteFilePath();
    _statusText = QString("Saved: %1").arg(QDir::toNativeSeparators(absolutePath));
    _statusLinkPath = absolutePath;
    _statusMarqueeSourceText.clear();
    _statusMarqueeOffset = 0;
    updateStatusLabels();
}

void mandelbrotgui::updateStatusLabels(const QString &rightText) {
    const int shownProgressValue =
        (_progressActive || _progressCancelled) ? _progressValue : 0;
    if (_progressLabel) {
        _progressLabel->setText(QString("%1%").arg(shownProgressValue));
        _progressLabel->setStyleSheet(_progressCancelled ?
            "color: rgb(215, 80, 80);" :
            QString());
    }
    if (_progressBar) {
        _progressBar->setValue(shownProgressValue);
        _progressBar->setTextVisible(false);
        _progressBar->setStyleSheet(_progressCancelled ?
            "QProgressBar::chunk { background-color: rgb(215, 80, 80); }" :
            QString());
    }
    if (_statusRightLabel) {
        _statusRightLabel->setStyleSheet(_progressCancelled ?
            "color: rgb(215, 80, 80);" :
            QString());
    }
    const QString fullPixelsPerSecondText =
        ((_progressActive || _pixelsPerSecondText != "0 pixels/s") &&
            !_pixelsPerSecondText.isEmpty()) ?
        _pixelsPerSecondText :
        QString();
    QString pixelsPerSecondText = fullPixelsPerSecondText;
    if (_imageMemoryLabel) {
        _imageMemoryLabel->setText(_imageMemoryText);
    }
    if (!_statusRightLabel) {
        if (_pixelsPerSecondLabel) _pixelsPerSecondLabel->setText(pixelsPerSecondText);
        return;
    }

    refreshDirtyComboLabels();

    const QString baseText = rightText.isEmpty() ? _statusText : rightText;
    const QString sourceLink = rightText.isEmpty() ? _statusLinkPath : QString();
    const QString sourceText = baseText;
    if (_pixelsPerSecondLabel) _pixelsPerSecondLabel->setText(pixelsPerSecondText);
    if (_statusMarqueeSourceText != sourceText) {
        _statusMarqueeSourceText = sourceText;
        _statusMarqueeOffset = 0;
    }

    const auto computeShouldMarquee = [this, &sourceText]() {
        const int availableWidth = std::max(1, _statusRightLabel->contentsRect().width());
        const QFontMetrics metrics(_statusRightLabel->font());
        return metrics.horizontalAdvance(sourceText) > availableWidth;
        };

    bool shouldMarquee = computeShouldMarquee();
    if (shouldMarquee && _progressActive && _pixelsPerSecondLabel) {
        const int separatorIndex = fullPixelsPerSecondText.indexOf(' ');
        if (separatorIndex > 0) {
            pixelsPerSecondText = fullPixelsPerSecondText.left(separatorIndex);
            shouldMarquee = computeShouldMarquee();
        }
    }
    if (_pixelsPerSecondLabel) _pixelsPerSecondLabel->setText(pixelsPerSecondText);

    if (shouldMarquee) {
        if (!_statusMarqueeTimer.isActive()) {
            _statusMarqueeTimer.start();
        }
    } else if (_statusMarqueeTimer.isActive()) {
        _statusMarqueeTimer.stop();
        _statusMarqueeOffset = 0;
    }

    QString shownText = sourceText;
    if (shouldMarquee && !sourceText.isEmpty()) {
        const QString padded = sourceText + "     ";
        const int wrappedOffset = _statusMarqueeOffset % padded.size();
        shownText = padded.mid(wrappedOffset) + padded.left(wrappedOffset);
    }

    if (!sourceLink.isEmpty()) {
        const QString href = QUrl::fromLocalFile(sourceLink).toString();
        _statusRightLabel->setText(QString("<a href=\"%1\">%2</a>")
            .arg(href, shownText.toHtmlEscaped()));
    } else {
        _statusRightLabel->setText(shownText.toHtmlEscaped());
    }
}

void mandelbrotgui::updateLightColorButton() {
    if (!_lightColorButton) return;

    const QColor color = lightColorToQColor(_state.lightColor);
    _lightColorButton->setText(QString::fromStdString(
        FormatUtil::formatHexColor(
            _state.lightColor.R,
            _state.lightColor.G,
            _state.lightColor.B)));
    _lightColorButton->setStyleSheet(lightColorButtonStyle(color));
}

void mandelbrotgui::updateModeEnablement() {
    const bool paletteMode = _state.colorMethod == Backend::ColorMethod::palette;
    const bool sineMode = _state.colorMethod == Backend::ColorMethod::iterations ||
        _state.colorMethod == Backend::ColorMethod::smooth_iterations;
    const bool lightMode = _state.colorMethod == Backend::ColorMethod::light;
    if (_sineGroup) {
        static_cast<CollapsibleGroupBox *>(_sineGroup)->setContentEnabled(sineMode);
    }
    if (_paletteGroup) {
        static_cast<CollapsibleGroupBox *>(_paletteGroup)->setContentEnabled(paletteMode);
    }
    if (_lightGroup) {
        static_cast<CollapsibleGroupBox *>(_lightGroup)->setContentEnabled(lightMode);
    }
    updateViewport();
}

void mandelbrotgui::updateViewport() {
    if (_viewport) {
        resizeViewportWindowToImageSize();
        requestViewportRepaint();
    }

    updateSinePreview();
    refreshPalettePreview();
    updateStatusLabels();
}

void mandelbrotgui::requestViewportRepaint() {
    if (!_viewport) return;
    _viewport->update();
}

void mandelbrotgui::updateSinePreview() {
    if (!_sinePreview) return;

    auto *preview = static_cast<SinePreviewWidget *>(_sinePreview);
    const QSize widgetSize = preview->size();
    if (!widgetSize.isValid()) return;

    if (!_backend || !_previewSession) {
        preview->setPreviewImage({});
        return;
    }

    const auto [rangeMin, rangeMax] = preview->range();
    const Backend::Status colorStatus =
        _previewSession->setSinePalette(_state.sinePalette);
    if (!colorStatus) {
        preview->setPreviewImage({});
        return;
    }

    const int previewWidth = std::max(1, widgetSize.width() - 12);
    const int previewHeight = std::max(1, widgetSize.height() - 28);
    const QImage image = imageViewToImage(_previewSession->renderSinePreview(
        previewWidth,
        previewHeight,
        static_cast<float>(rangeMin),
        static_cast<float>(rangeMax)));
    preview->setPreviewImage(image);
}

void mandelbrotgui::refreshPalettePreview() {
    if (!_palettePreviewLabel) return;

    const QSize targetSize = _palettePreviewLabel->size();
    if (!targetSize.isValid()) return;
    auto *preview = static_cast<PalettePreviewWidget *>(_palettePreviewLabel);

    if (!_palettePreviewDirty &&
        !_palettePreviewPixmap.isNull() &&
        _palettePreviewSize == targetSize) {
        preview->setPreviewPixmap(_palettePreviewPixmap);
        return;
    }

    const QImage image = makePalettePreviewImage(_state.palette, 256, 20);
    if (image.isNull()) {
        preview->clearPreview();
        return;
    }

    _palettePreviewPixmap = QPixmap::fromImage(image).scaled(
        targetSize,
        Qt::IgnoreAspectRatio,
        Qt::SmoothTransformation
    );
    _palettePreviewSize = targetSize;
    _palettePreviewDirty = false;
    preview->setPreviewPixmap(_palettePreviewPixmap);
}

void mandelbrotgui::updateControlWindowSize() {
    if (layout()) layout()->activate();
    if (_controlScrollContent && _controlScrollContent->layout()) {
        _controlScrollContent->layout()->activate();
        _controlScrollContent->updateGeometry();
    }

    QSize target = sizeHint().expandedTo(minimumSizeHint());
    const QSize packedTarget = minimumSizeHint().expandedTo(QSize(1, 1));
    const QMargins outerMargins = layout() ? layout()->contentsMargins() : QMargins();
    int controlContentHeight = 0, defaultVisibleContentHeight = 0;
    int packedWidth = packedTarget.width();
    if (_controlScrollContent) {
        const QSize contentSize = _controlScrollContent->sizeHint()
            .expandedTo(_controlScrollContent->minimumSizeHint());
        const QSize contentMinSize = _controlScrollContent->minimumSizeHint()
            .expandedTo(QSize(1, 1));
        const int contentWidth = contentSize.width();
        const int contentMinWidth = contentMinSize.width();
        controlContentHeight = contentSize.height();
        if (_viewportGroup) {
            const QMargins contentMargins = _controlScrollContent->contentsMargins();
            defaultVisibleContentHeight = std::max(
                0,
                _viewportGroup->geometry().bottom() + 1 + contentMargins.bottom());
        }
        target.setWidth(std::max(
            target.width(),
            contentWidth +
            outerMargins.left() + outerMargins.right() +
            kControlScrollBarWidth));
        packedWidth = std::max(
            packedWidth,
            contentMinWidth +
            outerMargins.left() + outerMargins.right() +
            kControlScrollBarWidth);
    } else {
        target += QSize(kControlScrollBarWidth, 0);
        packedWidth = std::max(packedWidth, target.width());
    }

    int fixedPanelsMinWidth = 0;
    if (QLayout *outerLayout = layout()) {
        for (int i = 0; i < outerLayout->count(); ++i) {
            QLayoutItem *item = outerLayout->itemAt(i);
            if (!item) continue;
            QWidget *widget = item->widget();
            if (!widget || widget == _controlScrollArea) continue;

            fixedPanelsMinWidth = std::max(
                fixedPanelsMinWidth,
                item->minimumSize().width());
            fixedPanelsMinWidth = std::max(
                fixedPanelsMinWidth,
                widget->minimumSizeHint().width());
        }
    }
    packedWidth = std::max(
        packedWidth,
        fixedPanelsMinWidth + outerMargins.left() + outerMargins.right());

    const int minHeight = std::max(1, minimumSizeHint().height());

    setMinimumWidth(std::max(1, packedWidth));
    setMaximumWidth(QWIDGETSIZE_MAX);
    setMinimumHeight(minHeight);
    setMaximumHeight(QWIDGETSIZE_MAX);

    if (!_controlWindowSized) {
        int fixedPanelsHeight = 0;
        if (QLayout *outerLayout = layout()) {
            const QMargins margins = outerLayout->contentsMargins();
            fixedPanelsHeight += margins.top() + margins.bottom();

            int visibleWidgets = 0;
            for (int i = 0; i < outerLayout->count(); ++i) {
                QLayoutItem *item = outerLayout->itemAt(i);
                if (!item) continue;

                QWidget *widget = item->widget();
                if (!widget) continue;

                ++visibleWidgets;
                if (widget == _controlScrollArea) continue;
                fixedPanelsHeight += item->sizeHint().height();
            }

            fixedPanelsHeight += std::max(0, visibleWidgets - 1) * outerLayout->spacing();
        }

        const int desiredContentHeight = defaultVisibleContentHeight > 0 ?
            defaultVisibleContentHeight :
            controlContentHeight;
        int desiredHeight = std::max(minHeight, fixedPanelsHeight + desiredContentHeight);
        if (const QScreen *screen =
            (windowHandle() && windowHandle()->screen()) ?
            windowHandle()->screen() :
            (this->screen() ? this->screen() : QApplication::primaryScreen())) {
            const int maxOnScreenHeight = std::max(
                minHeight,
                screen->availableGeometry().height() - kControlWindowScreenPadding);
            desiredHeight = std::min(desiredHeight, maxOnScreenHeight);
        }

        resize(std::max(1, packedWidth), desiredHeight);
    } else if (width() < packedWidth) {
        resize(packedWidth, std::max(height(), minHeight));
    }

    if (layout()) layout()->activate();
    updateStatusRightEdgeAlignment();
}

void mandelbrotgui::positionWindowsForInitialShow() {
    if (!_viewport) return;

    const QScreen *screen =
        (_viewport->windowHandle() && _viewport->windowHandle()->screen()) ?
        _viewport->windowHandle()->screen() :
        ((windowHandle() && windowHandle()->screen()) ?
            windowHandle()->screen() :
            (this->screen() ? this->screen() : QApplication::primaryScreen()));
    if (!screen) return;

    const QRect available = screen->availableGeometry();
    const QSize controlFrameSize = frameGeometry().size();
    const QSize viewportFrameSize = _viewport->frameGeometry().size();
    constexpr int gap = 8;

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
    const int totalWidth = controlFrameSize.width() + gap + viewportFrameSize.width();
    if (totalWidth <= available.width()) {
        controlX = available.left() + (available.width() - totalWidth) / 2;
        viewportX = controlX + controlFrameSize.width() + gap;
    } else {
        viewportX = std::min(
            controlX + controlFrameSize.width() + gap,
            available.right() - viewportFrameSize.width() + 1);
        viewportX = std::max(controlX, viewportX);
    }

    move(controlX, controlY);
    _viewport->move(viewportX, viewportY);
}

void mandelbrotgui::showHelpDialog() {
    QMessageBox dialog(this);
    dialog.setWindowTitle("Help");
    dialog.setIcon(QMessageBox::Information);
    dialog.setText(
        "Controls\n"
        "\n"
        "Mouse\n"
        "Left click: use the current navigation mode\n"
        "Right click: zoom out in zoom modes, or pan in Pan mode\n"
        "Mouse wheel: zoom\n"
        "Middle drag: temporarily pan in any mode while held\n"
        "\n"
        "Navigation modes\n"
        "Realtime Zoom: hold left or right click for continuous zoom\n"
        "Box Zoom: drag a box with left click\n"
        "Pan: drag with left, right, or middle click\n"
        "\n"
        "Keyboard\n"
        "Most keys work from either window.\n"
        "Esc: cancel queued or active renders\n"
        "Z: cycle navigation mode\n"
        "H: return to the home view\n"
        "G: cycle viewport grid\n"
        "Arrow keys: pan in Pan mode\n"
        "Space: temporary pan while held in the viewport only\n"
        "Shift: precision mode for mouse pan, arrow pan, and realtime zoom\n"
        "Ctrl: double arrow-pan speed and realtime zoom auto-pan speed\n"
        "F1: toggle viewport overlays and cursor\n"
        "F2: quick-save to the default save folder with date suffix enabled\n"
        "F12: toggle true fullscreen for the viewport\n"
        "\n"
        "Buttons\n"
        "Calculate: render with the current settings\n"
        "Home: return to the default view\n"
        "Zoom: zoom toward the center\n"
        "Save: save the current image\n"
        "\n"
        "Files\n"
        "Views save and load the current location\n"
        "Palettes and sine colors can be saved and imported from their controls.");
    dialog.setStandardButtons(QMessageBox::Ok);
    dialog.exec();
}

void mandelbrotgui::showAboutDialog() {
    QDialog dialog(this);
    dialog.setWindowTitle("About Mandelbrot GUI");
    dialog.setModal(true);
    dialog.setMinimumWidth(500);
    dialog.setStyleSheet(
        "QDialog {"
        " background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
        "     stop:0 #f7f1e8,"
        "     stop:0.45 #efe5d6,"
        "     stop:1 #e5d7c0);"
        "}"
        "QLabel#aboutTitle {"
        " color: #1d2430;"
        " font-size: 28px;"
        " font-weight: 700;"
        "}"
        "QLabel#aboutSubtitle {"
        " color: #5b6673;"
        " font-size: 13px;"
        "}"
        "QLabel#aboutNote {"
        " color: #6a5440;"
        " font-size: 12px;"
        " letter-spacing: 0.5px;"
        "}"
        "QFrame#aboutAccent {"
        " background: rgba(82, 55, 29, 0.10);"
        " border: 1px solid rgba(82, 55, 29, 0.12);"
        " border-radius: 18px;"
        "}"
        "QPushButton {"
        " min-width: 88px;"
        " padding: 6px 18px;"
        " border-radius: 8px;"
        " border: 1px solid rgba(45, 34, 25, 0.18);"
        " background: rgba(255, 255, 255, 0.55);"
        "}"
        "QPushButton:hover {"
        " background: rgba(255, 255, 255, 0.82);"
        "}");

    auto *outerLayout = new QVBoxLayout(&dialog);
    outerLayout->setContentsMargins(26, 26, 26, 18);
    outerLayout->setSpacing(18);

    auto *heroLayout = new QHBoxLayout();
    heroLayout->setContentsMargins(0, 0, 0, 0);
    heroLayout->setSpacing(18);

    auto *accent = new QFrame(&dialog);
    accent->setObjectName("aboutAccent");
    accent->setFixedSize(96, 96);
    auto *accentLayout = new QVBoxLayout(accent);
    accentLayout->setContentsMargins(0, 0, 0, 0);

    auto *iconLabel = new QLabel(accent);
    const QPixmap icon = windowIcon().pixmap(56, 56);
    iconLabel->setPixmap(icon);
    iconLabel->setAlignment(Qt::AlignCenter);
    accentLayout->addWidget(iconLabel);
    heroLayout->addWidget(accent, 0, Qt::AlignTop);

    auto *titleColumn = new QVBoxLayout();
    titleColumn->setContentsMargins(0, 4, 0, 0);
    titleColumn->setSpacing(6);

    auto *titleLabel = new QLabel("Mandelbrot GUI", &dialog);
    titleLabel->setObjectName("aboutTitle");
    auto *subtitleLabel = new QLabel(
        "Interactive Mandelbrot explorer",
        &dialog);
    subtitleLabel->setObjectName("aboutSubtitle");
    auto *noteLabel = new QLabel("Desktop fractal renderer", &dialog);
    noteLabel->setObjectName("aboutNote");

    titleColumn->addWidget(titleLabel);
    titleColumn->addWidget(subtitleLabel);
    titleColumn->addSpacing(4);
    titleColumn->addWidget(noteLabel);
    titleColumn->addStretch(1);

    heroLayout->addLayout(titleColumn, 1);
    outerLayout->addLayout(heroLayout);
    outerLayout->addStretch(1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted,
        &dialog, &QDialog::accept);
    outerLayout->addWidget(buttons);

    dialog.exec();
}

void mandelbrotgui::handleRenderFailure(const QString &message) {
    if (message.isEmpty()) return;

    _interactionPreviewForced.store(false, std::memory_order_release);
    setStatusMessage(message);
    if (_lastRenderFailureMessage == message) {
        return;
    }

    _lastRenderFailureMessage = message;
    QMessageBox::warning(this, "Render", message);
}

void mandelbrotgui::syncStateFromControls() {
    _state.backend = _backendCombo->currentText();
    _state.iterations = _iterationsSpin->value();
    _state.useThreads = _useThreadsCheckBox->isChecked();
    _state.julia = _juliaCheck->isChecked();
    _state.inverse = _inverseCheck->isChecked();
    _state.aaPixels = _aaSpin->value();
    _state.preserveRatio = _preserveRatioCheck->isChecked();
    _state.panRate = _panRateSlider->value();
    _state.zoomRate = _zoomRateSlider->value();
    _state.exponent = std::max(1.01, _exponentSpin->value());
    if (_sineCombo) {
        QString sineName = _sineCombo->currentData().toString();
        if (sineName.isEmpty()) {
            sineName = undecoratedLabel(_sineCombo->currentText());
        }
        if (!sineName.isEmpty() && sineName != kNewEntryLabel) {
            _state.sineName = sineName;
        }
    }
    _state.sinePalette.freqR = static_cast<float>(_freqRSpin->value());
    _state.sinePalette.freqG = static_cast<float>(_freqGSpin->value());
    _state.sinePalette.freqB = static_cast<float>(_freqBSpin->value());
    _state.sinePalette.freqMult = static_cast<float>(_freqMultSpin->value());
    if (_paletteCombo) {
        QString paletteName = _paletteCombo->currentData().toString();
        if (paletteName.isEmpty()) {
            paletteName = undecoratedLabel(_paletteCombo->currentText());
        }
        if (!paletteName.isEmpty() && paletteName != kNewEntryLabel) {
            _state.paletteName = paletteName;
        }
    }
    _state.palette.totalLength = static_cast<float>(_paletteLengthSpin->value());
    _state.palette.offset = static_cast<float>(_paletteOffsetSpin->value());
    _state.outputWidth = _outputWidthSpin->value();
    _state.outputHeight = _outputHeightSpin->value();

    if (_infoRealEdit) {
        const QString text = _infoRealEdit->text().trimmed();
        if (!text.isEmpty()) _pointRealText = text;
    }
    if (_infoImagEdit) {
        const QString text = _infoImagEdit->text().trimmed();
        if (!text.isEmpty()) _pointImagText = text;
    }
    syncStatePointFromText();

    if (_infoZoomEdit) {
        const QString text = _infoZoomEdit->text().trimmed();
        if (!text.isEmpty()) _zoomText = text;
    }
    syncStateZoomFromText();

    if (_infoSeedRealEdit) {
        const QString text = _infoSeedRealEdit->text().trimmed();
        if (!text.isEmpty()) _seedRealText = text;
    }
    if (_infoSeedImagEdit) {
        const QString text = _infoSeedImagEdit->text().trimmed();
        if (!text.isEmpty()) _seedImagText = text;
    }
    syncStateSeedFromText();

    switch (_colorMethodCombo->currentIndex()) {
        case 0: _state.colorMethod = Backend::ColorMethod::iterations; break;
        case 1: _state.colorMethod = Backend::ColorMethod::smooth_iterations; break;
        case 2: _state.colorMethod = Backend::ColorMethod::palette; break;
        case 3: _state.colorMethod = Backend::ColorMethod::light; break;
    }

    switch (_fractalCombo->currentIndex()) {
        case 0: _state.fractalType = Backend::FractalType::mandelbrot; break;
        case 1: _state.fractalType = Backend::FractalType::perpendicular; break;
        case 2: _state.fractalType = Backend::FractalType::burningship; break;
    }
}

void mandelbrotgui::syncControlsFromState() {
    const QSignalBlocker backendBlocker(_backendCombo);
    const QSignalBlocker iterationsBlocker(_iterationsSpin);
    const QSignalBlocker threadsBlocker(_useThreadsCheckBox);
    const QSignalBlocker juliaBlocker(_juliaCheck);
    const QSignalBlocker inverseBlocker(_inverseCheck);
    const QSignalBlocker aaBlocker(_aaSpin);
    const QSignalBlocker preserveRatioBlocker(_preserveRatioCheck);
    const QSignalBlocker panSliderBlocker(_panRateSlider);
    const QSignalBlocker panSpinBlocker(_panRateSpin);
    const QSignalBlocker zoomSliderBlocker(_zoomRateSlider);
    const QSignalBlocker zoomSpinBlocker(_zoomRateSpin);
    const QSignalBlocker exponentSliderBlocker(_exponentSlider);
    const QSignalBlocker exponentSpinBlocker(_exponentSpin);
    const QSignalBlocker sineComboBlocker(_sineCombo);
    const QSignalBlocker freqRBlocker(_freqRSpin);
    const QSignalBlocker freqGBlocker(_freqGSpin);
    const QSignalBlocker freqBBlocker(_freqBSpin);
    const QSignalBlocker freqMultBlocker(_freqMultSpin);
    const QSignalBlocker paletteComboBlocker(_paletteCombo);
    const QSignalBlocker paletteLengthBlocker(_paletteLengthSpin);
    const QSignalBlocker paletteOffsetBlocker(_paletteOffsetSpin);
    const QSignalBlocker widthBlocker(_outputWidthSpin);
    const QSignalBlocker heightBlocker(_outputHeightSpin);
    const QSignalBlocker colorBlocker(_colorMethodCombo);
    const QSignalBlocker fractalBlocker(_fractalCombo);
    const QSignalBlocker navModeBlocker(_navModeCombo);

    _backendCombo->setCurrentText(_state.backend);
    _iterationsSpin->setValue(_state.iterations);
    _useThreadsCheckBox->setChecked(_state.useThreads);
    _juliaCheck->setChecked(_state.julia);
    _inverseCheck->setChecked(_state.inverse);
    _aaSpin->setValue(_state.aaPixels);
    _preserveRatioCheck->setChecked(_state.preserveRatio);
    _panRateSlider->setValue(_state.panRate);
    _panRateSpin->setValue(_state.panRate);
    _zoomRateSlider->setValue(_state.zoomRate);
    _zoomRateSpin->setValue(_state.zoomRate);
    _exponentSlider->setValue(
        static_cast<int>(std::round(std::max(1.01, _state.exponent) * 100.0)));
    setAdaptiveSpinValue(_exponentSpin, std::max(1.01, _state.exponent));
    if (_sineCombo && !_state.sineName.isEmpty()) {
        const int sineIndex = _sineCombo->findData(_state.sineName);
        if (sineIndex >= 0) {
            _sineCombo->setCurrentIndex(sineIndex);
        } else {
            _sineCombo->setCurrentText(_state.sineName);
        }
    }
    setAdaptiveSpinValue(_freqRSpin, _state.sinePalette.freqR);
    setAdaptiveSpinValue(_freqGSpin, _state.sinePalette.freqG);
    setAdaptiveSpinValue(_freqBSpin, _state.sinePalette.freqB);
    setAdaptiveSpinValue(_freqMultSpin, _state.sinePalette.freqMult);
    if (_paletteCombo && !_state.paletteName.isEmpty()) {
        const int paletteIndex = _paletteCombo->findData(_state.paletteName);
        if (paletteIndex >= 0) {
            _paletteCombo->setCurrentIndex(paletteIndex);
        } else {
            _paletteCombo->setCurrentText(_state.paletteName);
        }
    }
    setAdaptiveSpinValue(_paletteLengthSpin, _state.palette.totalLength);
    setAdaptiveSpinValue(_paletteOffsetSpin, _state.palette.offset);
    _outputWidthSpin->setValue(_state.outputWidth);
    _outputHeightSpin->setValue(_state.outputHeight);
    _colorMethodCombo->setCurrentIndex(static_cast<int>(_state.colorMethod));
    _fractalCombo->setCurrentIndex(static_cast<int>(_state.fractalType));
    _navModeCombo->setCurrentIndex(static_cast<int>(displayedNavMode()));

    updateLightColorButton();
    syncStateReadouts();
    refreshDirtyComboLabels();
    updateSinePreview();
}

void mandelbrotgui::syncStateReadouts() {
    setCommittedLineEditText(_infoRealEdit, _pointRealText);
    setCommittedLineEditText(_infoImagEdit, _pointImagText);
    setCommittedLineEditText(_infoZoomEdit, _zoomText);
    setCommittedLineEditText(_infoSeedRealEdit, _seedRealText);
    setCommittedLineEditText(_infoSeedImagEdit, _seedImagText);
    if (_lightRealEdit) _lightRealEdit->setText(stateToString(_state.light.x(), 12));
    if (_lightImagEdit) _lightImagEdit->setText(stateToString(_state.light.y(), 12));
}

void mandelbrotgui::refreshSineList(const QString &preferredName) {
    if (!_sineCombo) return;

    QStringList names = listSineNames();
    const QString currentName = preferredName.isEmpty() ?
        _state.sineName :
        preferredName;
    if (!currentName.isEmpty() &&
        currentName != kNewEntryLabel &&
        !names.contains(currentName, Qt::CaseInsensitive)) {
        names.push_front(currentName);
    }
    const QString selectedName = names.contains(currentName, Qt::CaseInsensitive) ?
        currentName :
        (names.isEmpty() ? QString() : names.front());

    const QSignalBlocker blocker(_sineCombo);
    _sineCombo->clear();
    _sineCombo->addItem(kNewEntryLabel);
    _sineCombo->setItemData(0, QBrush(QColor(0, 153, 0)), Qt::ForegroundRole);
    _sineCombo->insertSeparator(1);
    const bool markUnsaved = isSineDirty();
    for (const QString &name : names) {
        const bool unsavedItem =
            markUnsaved && name.compare(currentName, Qt::CaseInsensitive) == 0;
        _sineCombo->addItem(decorateUnsavedLabel(name, unsavedItem), name);
    }
    if (!selectedName.isEmpty()) {
        const int selectedIndex = _sineCombo->findData(selectedName);
        if (selectedIndex >= 0) {
            _sineCombo->setCurrentIndex(selectedIndex);
        } else {
            _sineCombo->setCurrentText(selectedName);
        }
    } else {
        _sineCombo->setCurrentIndex(0);
    }
}

void mandelbrotgui::refreshPaletteList(const QString &preferredName) {
    if (!_paletteCombo) return;

    QStringList names = listPaletteNames();
    const QString currentName = preferredName.isEmpty() ?
        _state.paletteName :
        preferredName;
    if (!currentName.isEmpty() &&
        currentName != kNewEntryLabel &&
        !names.contains(currentName, Qt::CaseInsensitive)) {
        names.push_front(currentName);
    }
    const QString selectedName = names.contains(currentName, Qt::CaseInsensitive) ?
        currentName :
        (names.isEmpty() ? QString() : names.front());

    const QSignalBlocker blocker(_paletteCombo);
    _paletteCombo->clear();
    _paletteCombo->addItem(kNewEntryLabel);
    _paletteCombo->setItemData(0, QBrush(QColor(0, 153, 0)), Qt::ForegroundRole);
    _paletteCombo->insertSeparator(1);
    const bool markUnsaved = isPaletteDirty();
    for (const QString &name : names) {
        const bool unsavedItem =
            markUnsaved && name.compare(currentName, Qt::CaseInsensitive) == 0;
        _paletteCombo->addItem(decorateUnsavedLabel(name, unsavedItem), name);
    }
    if (!selectedName.isEmpty()) {
        const int selectedIndex = _paletteCombo->findData(selectedName);
        if (selectedIndex >= 0) {
            _paletteCombo->setCurrentIndex(selectedIndex);
        } else {
            _paletteCombo->setCurrentText(selectedName);
        }
    } else {
        _paletteCombo->setCurrentIndex(0);
    }
}

void mandelbrotgui::createNewSinePalette(bool requestRenderOnSuccess) {
    const QStringList existingNames = listSineNames();
    const QString newName = uniqueIndexedNameFromList(kNewSinePaletteBaseName, existingNames);
    const Backend::SinePaletteConfig config = makeNewSinePaletteConfig();

    _state.sineName = newName;
    _state.sinePalette = config;
    refreshSineList(_state.sineName);
    syncControlsFromState();
    updateViewport();
    if (requestRenderOnSuccess) {
        requestRender();
    }
}

void mandelbrotgui::createNewColorPalette(bool requestRenderOnSuccess) {
    const QStringList existingNames = listPaletteNames();
    const QString newName = uniqueIndexedNameFromList(kNewColorPaletteBaseName, existingNames);
    const Backend::PaletteHexConfig config = makeNewColorPaletteConfig();

    _state.paletteName = newName;
    _state.palette = config;
    _palettePreviewDirty = true;
    refreshPaletteList(_state.paletteName);
    syncControlsFromState();
    updateViewport();
    if (requestRenderOnSuccess) {
        requestRender();
    }
}

void mandelbrotgui::saveSine() {
    syncStateFromControls();

    QString targetName = normalizePaletteName(_state.sineName);
    if (!isValidPaletteName(targetName) || targetName == kNewEntryLabel) {
        const QString suggested = sanitizePaletteName(_state.sineName);
        bool accepted = false;
        const QString enteredName = QInputDialog::getText(this,
            "Save Sine Color",
            "Name",
            QLineEdit::Normal,
            suggested.isEmpty() ? "sine" : suggested,
            &accepted);
        if (!accepted) return;

        targetName = normalizePaletteName(enteredName);
        if (!isValidPaletteName(targetName)) {
            QMessageBox::warning(this, "Save Sine Color",
                "Use an ASCII name with letters, numbers, spaces, ., _, or -.");
            return;
        }
    }

    QString errorMessage;
    if (!ensureSineDirectory(errorMessage)) {
        QMessageBox::warning(this, "Save Sine Color", errorMessage);
        return;
    }

    std::filesystem::path destinationPath = sineFilePath(targetName);
    std::error_code existsError;
    const bool destinationExists =
        std::filesystem::exists(destinationPath, existsError) && !existsError;
    if (destinationExists) {
        const QMessageBox::StandardButton choice = QMessageBox::question(
            this,
            "Save Sine Color",
            QString("Sine \"%1\" already exists. Overwrite it?").arg(targetName),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
            QMessageBox::Yes);
        if (choice == QMessageBox::Cancel) return;
        if (choice == QMessageBox::No) {
            const QString savePath = showNativeSaveFileDialog(
                this,
                "Save Sine Color As",
                sineDirectoryPath(),
                QFileInfo(uniqueIndexedPathWithExtension(
                    sineDirectoryPath(),
                    targetName,
                    "txt")).fileName(),
                "Sine Files (*.txt);;All Files (*.*)");
            if (savePath.isEmpty()) return;

            const QString savePathWithExtension = QString::fromStdString(
                PathUtil::appendExtension(
                    savePath.toStdString(),
                    "txt"));
            const QString saveName = normalizePaletteName(
                QFileInfo(savePathWithExtension).completeBaseName());
            if (!isValidPaletteName(saveName)) {
                QMessageBox::warning(this, "Save Sine Color",
                    "Use an ASCII name with letters, numbers, spaces, ., _, or -.");
                return;
            }

            targetName = saveName;
            destinationPath = sineFilePath(targetName);
        }
    }

    if (!saveSineToPath(destinationPath, _state.sinePalette, errorMessage)) {
        QMessageBox::warning(this, "Save Sine Color", errorMessage);
        return;
    }

    _state.sineName = targetName;
    markSineSavedState(_state.sineName, _state.sinePalette);
    setStatusSavedPath(QString::fromStdWString(destinationPath.wstring()));
    refreshSineList(_state.sineName);
    syncControlsFromState();
    updateStatusLabels();
}

void mandelbrotgui::savePointView() {
    syncStateFromControls();
    _state.zoom = clampGUIZoom(_state.zoom);

    QString errorMessage;
    if (!ensureViewDirectory(errorMessage)) {
        QMessageBox::warning(this, "Save View", errorMessage);
        return;
    }

    const QString defaultPath = uniqueIndexedPathWithExtension(
        viewDirectoryPath(),
        "View",
        "txt");
    const QString selectedPath = showNativeSaveFileDialog(
        this,
        "Save View",
        viewDirectoryPath(),
        QFileInfo(defaultPath).fileName(),
        "View Files (*.txt);;All Files (*.*)");
    if (selectedPath.isEmpty()) return;

    PointConfig point = {
        .fractal = fractalTypeToConfigString(_state.fractalType).toStdString(),
        .inverse = _state.inverse,
        .julia = _state.julia,
        .iterations = _state.iterations,
        .real = _pointRealText.toStdString(),
        .imag = _pointImagText.toStdString(),
        .zoom = _zoomText.toStdString(),
        .seedReal = _seedRealText.toStdString(),
        .seedImag = _seedImagText.toStdString()
    };

    const QString outputPathWithExtension = QString::fromStdString(
        PathUtil::appendExtension(
            selectedPath.toStdString(),
            "txt"));
    if (!savePointToPath(outputPathWithExtension.toStdString(), point, errorMessage)) {
        QMessageBox::warning(this, "Save View", errorMessage);
        return;
    }
    markPointViewSavedState();
    setStatusSavedPath(outputPathWithExtension);
    updateStatusLabels();
}

void mandelbrotgui::loadPointView() {
    QString errorMessage;
    if (!ensureViewDirectory(errorMessage)) {
        QMessageBox::warning(this, "Load View", errorMessage);
        return;
    }

    const QString defaultPath = QString::fromStdWString(
        viewDirectoryPath().wstring());
    const QString selectedPath = showNativeOpenFileDialog(
        this,
        "Load View",
        viewDirectoryPath(),
        "View Files (*.txt);;All Files (*.*)");
    if (selectedPath.isEmpty()) return;

    PointConfig point;
    if (!loadPointFromPath(selectedPath.toStdString(), point, errorMessage)) {
        QMessageBox::warning(this, "Load View", errorMessage);
        return;
    }

    _state.fractalType = fractalTypeFromConfigString(QString::fromStdString(point.fractal));
    _state.inverse = point.inverse;
    _state.julia = point.julia;
    _state.iterations = std::max(0, point.iterations);
    _zoomText = QString::fromStdString(point.zoom);
    _pointRealText = QString::fromStdString(point.real);
    _pointImagText = QString::fromStdString(point.imag);
    _seedRealText = QString::fromStdString(point.seedReal);
    _seedImagText = QString::fromStdString(point.seedImag);
    syncStateZoomFromText();
    syncStatePointFromText();
    syncStateSeedFromText();
    markPointViewSavedState();
    syncControlsFromState();
    requestRender();
}

void mandelbrotgui::importSine() {
    const QString sourcePath = showNativeOpenFileDialog(
        this,
        "Import Sine Color",
        sineDirectoryPath(),
        "Sine Files (*.txt);;All Files (*.*)"
    );
    if (sourcePath.isEmpty()) return;

    Backend::SinePaletteConfig loaded;
    QString errorMessage;
    if (!loadSineFromPath(sourcePath.toStdString(), loaded, errorMessage)) {
        QMessageBox::warning(this, "Sine Color", errorMessage);
        return;
    }

    if (!ensureSineDirectory(errorMessage)) {
        QMessageBox::warning(this, "Sine Color", errorMessage);
        return;
    }

    const QStringList existingNames = listSineNames();
    const QString importedName = uniquePaletteName(
        QFileInfo(sourcePath).completeBaseName(),
        existingNames);
    const std::filesystem::path destinationPath = sineFilePath(importedName);
    if (!saveSineToPath(destinationPath, loaded, errorMessage)) {
        QMessageBox::warning(this, "Sine Color", errorMessage);
        return;
    }

    _state.sineName = importedName;
    _state.sinePalette = loaded;
    markSineSavedState(_state.sineName, _state.sinePalette);
    setStatusSavedPath(QString::fromStdWString(destinationPath.wstring()));
    refreshSineList(_state.sineName);
    syncControlsFromState();
    updateStatusLabels();
    requestRender();
}

bool mandelbrotgui::loadSineByName(const QString &name,
    bool requestRenderOnSuccess,
    QString *errorMessage) {
    if (errorMessage) errorMessage->clear();

    const QString normalizedName = normalizePaletteName(name);
    if (normalizedName.isEmpty()) return false;

    Backend::SinePaletteConfig loaded;
    const std::filesystem::path sourcePath = sineFilePath(normalizedName);
    QString localError;
    if (std::filesystem::exists(sourcePath)) {
        if (!loadSineFromPath(sourcePath, loaded, localError)) {
            if (errorMessage) *errorMessage = localError;
            return false;
        }
    } else {
        localError = QString("Sine file not found: %1").arg(normalizedName);
        if (errorMessage) *errorMessage = localError;
        return false;
    }

    _state.sineName = normalizedName;
    _state.sinePalette = loaded;
    markSineSavedState(_state.sineName, _state.sinePalette);
    syncControlsFromState();
    updateViewport();
    if (requestRenderOnSuccess) {
        requestRender();
    }
    return true;
}

bool mandelbrotgui::loadPaletteByName(const QString &name,
    bool requestRenderOnSuccess,
    QString *errorMessage) {
    if (errorMessage) errorMessage->clear();

    const QString normalizedName = normalizePaletteName(name);
    if (normalizedName.isEmpty()) return false;

    Backend::PaletteHexConfig loaded;
    const std::filesystem::path sourcePath = paletteFilePath(normalizedName);
    QString localError;
    if (std::filesystem::exists(sourcePath)) {
        if (!loadPaletteFromPath(sourcePath, loaded, localError)) {
            if (errorMessage) *errorMessage = localError;
            return false;
        }
    } else {
        localError = QString("Palette not found: %1").arg(normalizedName);
        if (errorMessage) *errorMessage = localError;
        return false;
    }

    _state.paletteName = normalizedName;
    _state.palette = loaded;
    markPaletteSavedState(_state.paletteName, _state.palette);
    _palettePreviewDirty = true;
    syncControlsFromState();
    updateViewport();
    if (requestRenderOnSuccess) {
        requestRender();
    }
    return true;
}

void mandelbrotgui::openPaletteEditor() {
    const Backend::PaletteHexConfig original = _state.palette;
    const QString originalName = _state.paletteName;
    PaletteEditorDialog dialog(_state.palette,
        _state.paletteName,
        [this](const QString &path) { setStatusSavedPath(path); },
        this);

    if (dialog.exec() == QDialog::Accepted) {
        _state.palette = dialog.palette();
        _state.palette.totalLength = static_cast<float>(_paletteLengthSpin->value());
        _state.palette.offset = static_cast<float>(_paletteOffsetSpin->value());
        if (!dialog.savedPaletteName().isEmpty() && !dialog.savedStateDirty()) {
            _state.paletteName = dialog.savedPaletteName();
            markPaletteSavedState(_state.paletteName, _state.palette);
            refreshPaletteList(_state.paletteName);
        }
        _palettePreviewDirty = true;
        updateViewport();
        requestRender();
    } else {
        _state.palette = original;
        _state.paletteName = originalName;
        refreshPaletteList(_state.paletteName);
        _palettePreviewDirty = true;
        updateViewport();
    }
}

bool mandelbrotgui::commitImageSave(
    const QString &path, bool appendDate, const QString &type) {
    QString error;
    if (!ensureBackendReady(error)) {
        QMessageBox::warning(this, "Save", error);
        return false;
    }

    if (_previewImage.isNull()) {
        QMessageBox::warning(this, "Save", "No image is available.");
        return false;
    }

    Backend::Status status;
    {
        status = _renderSession->saveImage(
            path.toStdString(),
            appendDate,
            type.toStdString());
    }
    if (!status) {
        QMessageBox::warning(this, "Save",
            QString::fromStdString(status.message));
        return false;
    }

    return true;
}

void mandelbrotgui::saveImage() {
    auto result = runSaveDialog(this, "mandelbrot.png");
    if (!result) return;

    (void)commitImageSave(result->path, result->appendDate, result->type);
}

void mandelbrotgui::quickSaveImage() {
    std::error_code ec;
    std::filesystem::create_directories(savesDirectoryPath(), ec);
    const QString path = QString::fromStdWString(
        (savesDirectoryPath() / "mandelbrot.png").wstring());
    (void)commitImageSave(path, true, "png");
}

void mandelbrotgui::resizeViewportImage() {
    syncStateFromControls();
    resizeViewportWindowToImageSize();
    requestRender(true);
}

void mandelbrotgui::updateSelectionTarget(int index) {
    _selectionTarget = static_cast<SelectionTarget>(index);
}

void mandelbrotgui::updateNavMode(int index) {
    _navMode = static_cast<NavMode>(index);
}

void mandelbrotgui::updateAspectLinkedSizes(bool widthChanged) {
    if (!_preserveRatioCheck || !_preserveRatioCheck->isChecked()) return;
    if (!_outputWidthSpin || !_outputHeightSpin) return;
    if (_state.outputWidth <= 0 || _state.outputHeight <= 0) return;

    if (widthChanged) {
        const int newWidth = _outputWidthSpin->value();
        const int newHeight = std::max(1,
            static_cast<int>(std::lround(
                static_cast<double>(newWidth) * _state.outputHeight /
                std::max(1, _state.outputWidth))));
        const QSignalBlocker blocker(_outputHeightSpin);
        _outputHeightSpin->setValue(newHeight);
        return;
    }

    const int newHeight = _outputHeightSpin->value();
    const int newWidth = std::max(1,
        static_cast<int>(std::lround(
            static_cast<double>(newHeight) * _state.outputWidth /
            std::max(1, _state.outputHeight))));
    const QSignalBlocker blocker(_outputWidthSpin);
    _outputWidthSpin->setValue(newWidth);
}

void mandelbrotgui::markSineSavedState(const QString &name,
    const Backend::SinePaletteConfig &palette) {
    _savedSineName = normalizePaletteName(name);
    _savedSinePalette = palette;
    _hasSavedSineState = true;
}

void mandelbrotgui::markPaletteSavedState(const QString &name,
    const Backend::PaletteHexConfig &palette) {
    _savedPaletteName = normalizePaletteName(name);
    _savedPalette = palette;
    _hasSavedPaletteState = true;
}

mandelbrotgui::SavedPointViewState mandelbrotgui::capturePointViewState() const {
    return SavedPointViewState{
        .fractalType = _state.fractalType,
        .inverse = _state.inverse,
        .julia = _state.julia,
        .iterations = _state.iterations,
        .real = _pointRealText,
        .imag = _pointImagText,
        .zoom = _zoomText,
        .seedReal = _seedRealText,
        .seedImag = _seedImagText
    };
}

void mandelbrotgui::markPointViewSavedState() {
    _savedPointViewState = capturePointViewState();
    _hasSavedPointViewState = true;
}

bool mandelbrotgui::isSineDirty() const {
    if (!_hasSavedSineState) return true;

    if (normalizePaletteName(_state.sineName).compare(
        _savedSineName, Qt::CaseInsensitive) != 0) {
        return true;
    }

    return !sameSinePalette(_state.sinePalette, _savedSinePalette);
}

bool mandelbrotgui::isPaletteDirty() const {
    if (!_hasSavedPaletteState) return true;

    if (normalizePaletteName(_state.paletteName).compare(
        _savedPaletteName, Qt::CaseInsensitive) != 0) {
        return true;
    }

    return !samePalette(_state.palette, _savedPalette);
}

bool mandelbrotgui::isPointViewDirty() const {
    if (!_hasSavedPointViewState) return true;

    const SavedPointViewState current = capturePointViewState();
    const SavedPointViewState &saved = _savedPointViewState;
    if (current.fractalType != saved.fractalType ||
        current.inverse != saved.inverse ||
        current.julia != saved.julia ||
        current.iterations != saved.iterations ||
        !NumberUtil::equalParsedDoubleText(
            current.real.toStdString(), saved.real.toStdString()) ||
        !NumberUtil::equalParsedDoubleText(
            current.imag.toStdString(), saved.imag.toStdString()) ||
        !NumberUtil::equalParsedDoubleText(
            current.zoom.toStdString(), saved.zoom.toStdString()) ||
        !NumberUtil::equalParsedDoubleText(
            current.seedReal.toStdString(), saved.seedReal.toStdString()) ||
        !NumberUtil::equalParsedDoubleText(
            current.seedImag.toStdString(), saved.seedImag.toStdString())) {
        return true;
    }

    return false;
}

void mandelbrotgui::refreshDirtyComboLabels() {
    auto refreshCombo = [](QComboBox *combo,
        const QString &currentName,
        bool isDirty) {
            if (!combo) return;
            if (currentName.isEmpty()) return;

            const QSignalBlocker blocker(combo);
            for (int i = 0; i < combo->count(); ++i) {
                const QString itemData = combo->itemData(i).toString();
                const QString baseName = itemData.isEmpty() ?
                    undecoratedLabel(combo->itemText(i)) :
                    itemData;
                if (baseName.isEmpty() || baseName == kNewEntryLabel) continue;

                const bool unsavedItem =
                    isDirty && baseName.compare(currentName, Qt::CaseInsensitive) == 0;
                const QString desiredLabel = decorateUnsavedLabel(baseName, unsavedItem);
                if (combo->itemText(i) != desiredLabel) {
                    combo->setItemText(i, desiredLabel);
                }
            }
        };

    refreshCombo(_sineCombo, _state.sineName, isSineDirty());
    refreshCombo(_paletteCombo, _state.paletteName, isPaletteDirty());
}

QString mandelbrotgui::stateToString(double value, int precision) const {
    return QString::number(value, 'g', precision);
}

void mandelbrotgui::syncZoomTextFromState() {
    _zoomText = stateToString(_state.zoom, 17);
}

void mandelbrotgui::syncStateZoomFromText() {
    bool ok = false;
    const double zoom = _zoomText.toDouble(&ok);
    if (ok) {
        _state.zoom = clampGUIZoom(zoom);
        if (!NumberUtil::almostEqual(_state.zoom, zoom)) {
            _zoomText = stateToString(_state.zoom, 17);
        }
    }
}

void mandelbrotgui::syncPointTextFromState() {
    _pointRealText = stateToString(_state.point.x());
    _pointImagText = stateToString(_state.point.y());
}

void mandelbrotgui::syncStatePointFromText() {
    bool okReal = false, okImag = false;
    const double real = _pointRealText.toDouble(&okReal);
    const double imag = _pointImagText.toDouble(&okImag);
    if (okReal) _state.point.setX(real);
    if (okImag) _state.point.setY(imag);
}

void mandelbrotgui::syncSeedTextFromState() {
    _seedRealText = stateToString(_state.seed.x(), 17);
    _seedImagText = stateToString(_state.seed.y(), 17);
}

void mandelbrotgui::syncStateSeedFromText() {
    bool okReal = false, okImag = false;
    const double real = _seedRealText.toDouble(&okReal);
    const double imag = _seedImagText.toDouble(&okImag);
    if (okReal) _state.seed.setX(real);
    if (okImag) _state.seed.setY(imag);
}

QPoint mandelbrotgui::clampPixelToOutput(const QPoint &pixel) const {
    const QSize size = outputSize();
    return {
        std::clamp(pixel.x(), 0, std::max(0, size.width() - 1)),
        std::clamp(pixel.y(), 0, std::max(0, size.height() - 1))
    };
}

int mandelbrotgui::resolveCurrentIterationCount() const {
    if (_backend && _renderSession) {
        return std::max(1, _renderSession->currentIterationCount());
    }

    return std::max(1, _state.iterations);
}

void mandelbrotgui::resizeViewportWindowToImageSize() {
    auto *viewport = _viewport ? static_cast<ViewportWindow *>(_viewport) : nullptr;
    if (viewport && viewport->isFullScreen()) {
        return;
    }

    resizeViewportToImageSize(_viewport, outputSize());
}

int mandelbrotgui::interactionFrameIntervalMs() const {
    const int fps = std::max(1, _state.interactionTargetFPS);
    return std::max(
        1,
        static_cast<int>(std::lround(1000.0 / static_cast<double>(fps))));
}

bool mandelbrotgui::canRenderAtTargetFPS() const {
    const int renderMs = _lastRenderDurationMs.load(std::memory_order_acquire);
    if (renderMs <= 0) return false;
    return !_interactionPreviewFallbackLatched.load(std::memory_order_acquire);
}

bool mandelbrotgui::shouldUseInteractionPreviewFallback() const {
    return
        _interactionPreviewForced.load(std::memory_order_acquire) ||
        _interactionPreviewFallbackLatched.load(std::memory_order_acquire);
}

double mandelbrotgui::zoomRateFactor() const {
    return currentZoomFactor(_state.zoomRate);
}

double mandelbrotgui::panRateFactor() const {
    constexpr double panRateBase = 1.125;
    const int clamped = clampSliderValue(_state.panRate);
    return std::pow(panRateBase, static_cast<double>(clamped - 8));
}

double mandelbrotgui::currentZoomFactor(int sliderValue) const {
    return kZoomStepTable[clampSliderValue(sliderValue)];
}
