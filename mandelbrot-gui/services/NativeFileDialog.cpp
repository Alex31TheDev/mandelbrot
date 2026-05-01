#include "services/NativeFileDialog.h"

#include "util/IncludeWin32.h"

#ifdef _WIN32
#include <shobjidl.h>
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Shell32.lib")
#endif

#include <QDialog>
#include <QCoreApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QStringList>

#include "util/PathUtil.h"

static QString defaultExtensionFromPattern(const QString &patternText) {
    const QStringList patterns = patternText.split(' ', Qt::SkipEmptyParts);
    for (QString pattern : patterns) {
        pattern = pattern.trimmed();
        if (!pattern.startsWith("*.")) continue;
        pattern.remove(0, 2);
        if (!pattern.isEmpty()) return pattern;
    }
    return QString();
}

namespace GUI {
    std::vector<NativeDialogFilter> parseNativeDialogFilters(
        const QString &filters
    ) {
        std::vector<NativeDialogFilter> parsed;
        const QStringList entries = filters.split(";;", Qt::SkipEmptyParts);
        for (const QString &entry : entries) {
            const int openParen = entry.indexOf('(');
            const int closeParen = entry.lastIndexOf(')');
            if (openParen < 0 || closeParen <= openParen) {
                parsed.push_back({ entry.trimmed(), "*" });
                continue;
            }

            parsed.push_back({ entry.left(openParen).trimmed(),
                entry.mid(openParen + 1, closeParen - openParen - 1).trimmed() });
        }
        return parsed;
    }

    QString showNativeSaveFileDialog(
        QWidget *parent, const QString &title,
        const std::filesystem::path &directory, const QString &suggestedFileName,
        const QString &filters, QString *selectedFilter
    ) {
#ifdef _WIN32
        const std::vector<NativeDialogFilter> parsedFilters
            = parseNativeDialogFilters(filters);

        HRESULT initHr = CoInitializeEx(
            nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        const bool shouldUninitialize = SUCCEEDED(initHr);

        IFileSaveDialog *dialog = nullptr;
        const HRESULT createHr = CoCreateInstance(CLSID_FileSaveDialog, nullptr,
            CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
        if (FAILED(createHr) || !dialog) {
            if (shouldUninitialize) CoUninitialize();
            return QString();
        }

        dialog->SetTitle(reinterpret_cast<LPCWSTR>(title.utf16()));
        if (!suggestedFileName.isEmpty()) {
            dialog->SetFileName(
                reinterpret_cast<LPCWSTR>(suggestedFileName.utf16()));
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
            filterSpecs.push_back(
                { filterNames.back().c_str(), filterPatterns.back().c_str() });
        }

        if (!filterSpecs.empty()) {
            dialog->SetFileTypes(
                static_cast<UINT>(filterSpecs.size()), filterSpecs.data());
            dialog->SetFileTypeIndex(1);
            const QString defaultExt
                = defaultExtensionFromPattern(parsedFilters.front().patternText);
            if (!defaultExt.isEmpty()) {
                const std::wstring ext = defaultExt.toStdWString();
                dialog->SetDefaultExtension(ext.c_str());
            }
        }

        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST
            | FOS_OVERWRITEPROMPT);

        std::error_code ec;
        std::filesystem::create_directories(directory, ec);
        IShellItem *folder = nullptr;
        const std::wstring dir = directory.wstring();
        if (SUCCEEDED(SHCreateItemFromParsingName(
            dir.c_str(), nullptr, IID_PPV_ARGS(&folder)))) {
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

        UINT filterIndex = 1;
        dialog->GetFileTypeIndex(&filterIndex);
        if (selectedFilter && filterIndex > 0
            && filterIndex <= static_cast<UINT>(parsedFilters.size())) {
            *selectedFilter = parsedFilters[filterIndex - 1].patternText;
        }

        IShellItem *resultItem = nullptr;
        if (FAILED(dialog->GetResult(&resultItem)) || !resultItem) {
            dialog->Release();
            if (shouldUninitialize) CoUninitialize();
            return QString();
        }

        PWSTR selectedPath = nullptr;
        if (FAILED(resultItem->GetDisplayName(SIGDN_FILESYSPATH, &selectedPath))
            || !selectedPath) {
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
        QFileDialog dialog(parent, title,
            QString::fromStdWString(directory.wstring()), filters);
        dialog.setAcceptMode(QFileDialog::AcceptSave);
        dialog.setOption(QFileDialog::DontUseNativeDialog, false);
        if (!suggestedFileName.isEmpty()) {
            dialog.selectFile(suggestedFileName);
        }

        if (dialog.exec() != QDialog::Accepted) return QString();
        if (selectedFilter) *selectedFilter = dialog.selectedNameFilter();
        return dialog.selectedFiles().value(0);
#endif
    }

    QString showNativeOpenFileDialog(
        QWidget *parent, const QString &title,
        const std::filesystem::path &directory, const QString &filters
    ) {
#ifdef _WIN32
        const std::vector<NativeDialogFilter> parsedFilters
            = parseNativeDialogFilters(filters);

        HRESULT initHr = CoInitializeEx(
            nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        const bool shouldUninitialize = SUCCEEDED(initHr);

        IFileOpenDialog *dialog = nullptr;
        const HRESULT createHr = CoCreateInstance(CLSID_FileOpenDialog, nullptr,
            CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
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
            filterSpecs.push_back(
                { filterNames.back().c_str(), filterPatterns.back().c_str() });
        }
        if (!filterSpecs.empty()) {
            dialog->SetFileTypes(
                static_cast<UINT>(filterSpecs.size()), filterSpecs.data());
            dialog->SetFileTypeIndex(1);
        }

        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST
            | FOS_PATHMUSTEXIST);

        IShellItem *folder = nullptr;
        const std::wstring dir = directory.wstring();
        if (SUCCEEDED(SHCreateItemFromParsingName(
            dir.c_str(), nullptr, IID_PPV_ARGS(&folder)))) {
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
        if (FAILED(resultItem->GetDisplayName(SIGDN_FILESYSPATH, &selectedPath))
            || !selectedPath) {
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
        QFileDialog dialog(parent, title,
            QString::fromStdWString(directory.wstring()), filters);
        dialog.setAcceptMode(QFileDialog::AcceptOpen);
        dialog.setFileMode(QFileDialog::ExistingFile);
        dialog.setOption(QFileDialog::DontUseNativeDialog, false);

        if (dialog.exec() != QDialog::Accepted) return QString();
        return dialog.selectedFiles().value(0);
#endif
    }

    std::optional<SaveDialogResult> runSaveImageDialog(
        QWidget *parent, const QString &suggestedFile
    ) {
        const std::filesystem::path savesDir = PathUtil::executableDir() / "saves";
        std::error_code ec;
        std::filesystem::create_directories(savesDir, ec);

        SaveDialogResult result;
#ifdef _WIN32
        HRESULT initHr = CoInitializeEx(
            nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        const bool shouldUninitialize = SUCCEEDED(initHr);

        IFileSaveDialog *dialog = nullptr;
        const HRESULT createHr = CoCreateInstance(CLSID_FileSaveDialog, nullptr,
            CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
        if (FAILED(createHr) || !dialog) {
            if (shouldUninitialize) CoUninitialize();
            return std::nullopt;
        }

        const QString saveImageTitle
            = QCoreApplication::translate("NativeFileDialog", "Save Image");
        const QString pngFilterLabel
            = QCoreApplication::translate("NativeFileDialog", "PNG Files (*.png)");
        const QString jpegFilterLabel = QCoreApplication::translate(
            "NativeFileDialog", "JPEG Files (*.jpg;*.jpeg)");
        const QString bmpFilterLabel = QCoreApplication::translate(
            "NativeFileDialog", "Bitmap Files (*.bmp)");
        const QString appendDateLabel
            = QCoreApplication::translate("NativeFileDialog", "Append Date");
        const std::wstring saveImageTitleW = saveImageTitle.toStdWString();
        const std::wstring pngFilterLabelW = pngFilterLabel.toStdWString();
        const std::wstring jpegFilterLabelW = jpegFilterLabel.toStdWString();
        const std::wstring bmpFilterLabelW = bmpFilterLabel.toStdWString();
        const std::wstring appendDateLabelW = appendDateLabel.toStdWString();

        dialog->SetTitle(saveImageTitleW.c_str());
        dialog->SetFileName(reinterpret_cast<LPCWSTR>(suggestedFile.utf16()));

        const COMDLG_FILTERSPEC filters[]
            = { { pngFilterLabelW.c_str(), L"*.png" },
                  { jpegFilterLabelW.c_str(), L"*.jpg;*.jpeg" },
                  { bmpFilterLabelW.c_str(), L"*.bmp" } };
        dialog->SetFileTypes(static_cast<UINT>(std::size(filters)), filters);
        dialog->SetFileTypeIndex(1);
        dialog->SetDefaultExtension(L"png");

        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST
            | FOS_OVERWRITEPROMPT);

        IShellItem *folder = nullptr;
        const std::wstring savesDirText = savesDir.wstring();
        if (SUCCEEDED(SHCreateItemFromParsingName(
            savesDirText.c_str(), nullptr, IID_PPV_ARGS(&folder)))) {
            dialog->SetFolder(folder);
            folder->Release();
        }

        IFileDialogCustomize *customize = nullptr;
        bool appendDate = true;
        if (SUCCEEDED(dialog->QueryInterface(IID_PPV_ARGS(&customize)))
            && customize) {
            customize->AddCheckButton(1, appendDateLabelW.c_str(), TRUE);
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
        if (FAILED(selectedItem->GetDisplayName(SIGDN_FILESYSPATH, &selectedPath))
            || !selectedPath) {
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
        const QString saveImageTitle
            = QCoreApplication::translate("NativeFileDialog", "Save Image");
        const QString saveImageFilters = QCoreApplication::translate(
            "NativeFileDialog",
            "PNG Files (*.png);;JPEG Files (*.jpg *.jpeg);;Bitmap Files (*.bmp)");
        result.path = showNativeSaveFileDialog(parent, saveImageTitle, savesDir,
            suggestedFile, saveImageFilters, &selectedFilter);
        if (result.path.isEmpty()) return std::nullopt;

        const QMessageBox::StandardButton appendDateChoice
            = QMessageBox::question(parent, saveImageTitle,
                QCoreApplication::translate(
                    "NativeFileDialog", "Append date to filename?"),
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
            result.path.toStdString(), result.type.toStdString()));
        return result;
    }
}