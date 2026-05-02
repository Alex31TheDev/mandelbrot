#pragma once

#include <filesystem>

#include <QColor>
#include <QDoubleSpinBox>
#include <QImage>
#include <QLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QWidget>

#include "BackendAPI.h"

namespace GUI::Util {
    using namespace Backend;

    [[nodiscard]] int clampSliderValue(int value);
    [[nodiscard]] QString committedLineEditText(const QLineEdit *edit);
    void markLineEditTextCommitted(QLineEdit *edit);
    void setCommittedLineEditText(QLineEdit *edit, const QString &text);
    [[nodiscard]] bool hasUncommittedLineEditChange(const QLineEdit *edit);

    [[nodiscard]] QString translatedNewEntryLabel();
    [[nodiscard]] QString translatedUnsavedLabelSuffix();
    [[nodiscard]] QString decorateUnsavedLabel(const QString &name,
        bool unsaved);
    [[nodiscard]] QString undecoratedLabel(const QString &name);
    [[nodiscard]] QString uniqueIndexedNameFromList(
        const QString &baseName, const QStringList &existingNames
    );
    [[nodiscard]] QString uniqueIndexedPathWithExtension(
        const std::filesystem::path &directory, const QString &baseName,
        const QString &extension
    );

    [[nodiscard]] QString fractalTypeToConfigString(FractalType fractalType);
    [[nodiscard]] FractalType fractalTypeFromConfigString(const QString &value);
    [[nodiscard]] double clampGUIZoom(double zoom);
    [[nodiscard]] QString defaultPixelsPerSecondText();
    [[nodiscard]] QString defaultImageMemoryText();
    [[nodiscard]] QString defaultViewportFPSText();
    [[nodiscard]] QString formatViewportFPSText(double fps);
    [[nodiscard]] QString formatPixelsPerSecondText(
        const QString &pixelsPerSecond
    );
    [[nodiscard]] QString formatImageMemoryText(const ImageEvent &event);

    [[nodiscard]] QColor lightColorToQColor(const LightColor &color);
    [[nodiscard]] LightColor lightColorFromQColor(const QColor &color);
    [[nodiscard]] QString lightColorButtonStyle(const QColor &color);

    [[nodiscard]] std::filesystem::path savesDirectoryPath();
    void stabilizePushButton(QPushButton *button);
    void setLayoutItemsVisible(QLayout *layout, bool visible);
    void setLayoutItemsEnabled(QLayout *layout, bool enabled);

    [[nodiscard]] QImage wrapImageViewToImage(const ImageView &view);
    [[nodiscard]] QImage imageViewToImage(const ImageView &view);
    [[nodiscard]] QImage makeBlankViewportImage();
    void setAdaptiveSpinValue(QDoubleSpinBox *spinBox, double value);

}
