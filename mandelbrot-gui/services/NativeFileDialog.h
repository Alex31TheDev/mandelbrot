#pragma once

#include <filesystem>
#include <optional>
#include <vector>

#include <QString>
#include <QWidget>

namespace GUI {
    struct SaveDialogResult {
        QString path;
        QString type;
        bool appendDate = false;
    };

    struct NativeDialogFilter {
        QString displayText;
        QString patternText;
    };

    [[nodiscard]] std::vector<NativeDialogFilter> parseNativeDialogFilters(
        const QString &filters
    );
    [[nodiscard]] QString showNativeSaveFileDialog(QWidget *parent,
        const QString &title, const std::filesystem::path &directory,
        const QString &suggestedFileName, const QString &filters,
        QString *selectedFilter = nullptr);
    [[nodiscard]] QString showNativeOpenFileDialog(QWidget *parent,
        const QString &title, const std::filesystem::path &directory,
        const QString &filters);
    [[nodiscard]] std::optional<SaveDialogResult> runSaveImageDialog(
        QWidget *parent, const QString &suggestedFile
    );
}
