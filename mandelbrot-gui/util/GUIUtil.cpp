#include "util/GUIUtil.h"

#include <algorithm>
#include <cmath>
#include <system_error>

#include <QDoubleSpinBox>
#include <QLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QApplication>
#include <QScreen>
#include <QWidget>
#include <QWindow>

#include "widgets/AdaptiveDoubleSpinBox.h"
#include "app/GUIConstants.h"
#include "util/FormatUtil.h"
#include "util/PathUtil.h"
#include "util/StringUtil.h"

#include "BackendAPI.h"
using namespace Backend;

using namespace GUI;

const char committedLineEditTextProperty[] = "_mandelbrotCommittedText";

namespace GUI::Util {
    int clampSliderValue(int value) {
        return std::clamp(value, 0, 20);
    }

    QString translatedNewEntryLabel() {
        return QApplication::translate("GUI::Util", "+ New");
    }

    QString translatedUnsavedLabelSuffix() {
        return QApplication::translate("GUI::Util", " (unsaved)");
    }

    QString committedLineEditText(const QLineEdit *edit) {
        return edit ? edit->text().trimmed() : QString();
    }

    void markLineEditTextCommitted(QLineEdit *edit) {
        if (!edit) return;
        edit->setProperty(
            committedLineEditTextProperty, committedLineEditText(edit));
    }

    void setCommittedLineEditText(QLineEdit *edit, const QString &text) {
        if (!edit) return;
        edit->setText(text);
        markLineEditTextCommitted(edit);
    }

    bool hasUncommittedLineEditChange(const QLineEdit *edit) {
        if (!edit) return false;
        return committedLineEditText(edit)
            != edit->property(committedLineEditTextProperty).toString();
    }

    QString decorateUnsavedLabel(const QString &name, bool unsaved) {
        if (!unsaved) return name;
        return QString::fromStdString(
            StringUtil::appendSuffix(
                name.toStdString(), translatedUnsavedLabelSuffix().toStdString()));
    }

    QString undecoratedLabel(const QString &name) {
        return QString::fromStdString(
            StringUtil::stripSuffix(
                name.toStdString(), translatedUnsavedLabelSuffix().toStdString()));
    }

    QString uniqueIndexedNameFromList(
        const QString &baseName, const QStringList &existingNames) {
        return QString::fromStdString(
            FormatUtil::uniqueIndexedName(baseName.toStdString(),
                [&existingNames](const std::string_view candidate) {
                    return existingNames.contains(
                        QString::fromUtf8(candidate.data(),
                            static_cast<int>(candidate.size())),
                        Qt::CaseInsensitive);
                }));
    }

    QString uniqueIndexedPathWithExtension(
        const std::filesystem::path &directory, const QString &baseName,
        const QString &extension) {
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

    QString fractalTypeToConfigString(FractalType fractalType) {
        switch (fractalType) {
            case FractalType::mandelbrot:
                return "mandelbrot";
            case FractalType::perpendicular:
                return "perpendicular";
            case FractalType::burningship:
                return "burningship";
        }
        return "mandelbrot";
    }

    FractalType fractalTypeFromConfigString(const QString &value) {
        const QString normalized = value.trimmed().toLower();
        if (normalized == "perpendicular")
            return FractalType::perpendicular;
        if (normalized == "burningship" || normalized == "burning_ship"
            || normalized == "burning ship") {
            return FractalType::burningship;
        }
        return FractalType::mandelbrot;
    }

    double clampGUIZoom(double zoom) {
        return std::max(zoom, Constants::minimumZoom);
    }

    QString defaultPixelsPerSecondText() {
        return QApplication::translate("GUI::Util", "0 pixels/s");
    }

    QString defaultImageMemoryText() {
        return QApplication::translate("GUI::Util", "Render: -  Output: -");
    }

    QString defaultViewportFPSText() {
        return QApplication::translate("GUI::Util", "- FPS");
    }

    QString formatViewportFPSText(double fps) {
        return QApplication::translate("GUI::Util", "%1 FPS")
            .arg(QString::number(fps, 'f', 1));
    }

    QString formatPixelsPerSecondText(const QString &pixelsPerSecond) {
        return QApplication::translate("GUI::Util", "%1 pixels/s")
            .arg(pixelsPerSecond);
    }

    QString formatImageMemoryText(const ImageEvent &event) {
        const QString renderBytes = QString::fromStdString(
            FormatUtil::formatBufferSize(event.primaryBytes));
        if (!event.downscaling) {
            return QApplication::translate("GUI::Util", "Render: %1").arg(renderBytes);
        }

        const QString outputBytesText = QString::fromStdString(
            FormatUtil::formatBufferSize(event.secondaryBytes));
        return QApplication::translate("GUI::Util", "Render: %1  Output: %2")
            .arg(renderBytes, outputBytesText);
    }

    QColor lightColorToQColor(const LightColor &color) {
        return QColor::fromRgbF(
            std::clamp(static_cast<double>(color.R), 0.0, 1.0),
            std::clamp(static_cast<double>(color.G), 0.0, 1.0),
            std::clamp(static_cast<double>(color.B), 0.0, 1.0));
    }

    LightColor lightColorFromQColor(const QColor &color) {
        return { .R = static_cast<float>(color.redF()),
            .G = static_cast<float>(color.greenF()),
            .B = static_cast<float>(color.blueF()) };
    }

    QString lightColorButtonStyle(const QColor &color) {
        const int luminance = (color.red() * 299 + color.green() * 587
            + color.blue() * 114)
            / 1000;
        const QColor fg = luminance >= 160 ? QColor(Qt::black) : QColor(Qt::white);

        return QString("QPushButton {"
            " background-color: %1;"
            " color: %2;"
            "}")
            .arg(color.name(QColor::HexRgb), fg.name(QColor::HexRgb));
    }

    std::filesystem::path savesDirectoryPath() {
        return PathUtil::executableDir() / "saves";
    }

    void stabilizePushButton(QPushButton *button) {
        if (!button) return;
        button->setAutoDefault(false);
        button->setDefault(false);
    }

    void setLayoutItemsVisible(QLayout *layout, bool visible) {
        if (!layout) return;

        for (int i = 0; i < layout->count(); i++) {
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

        for (int i = 0; i < layout->count(); i++) {
            QLayoutItem *item = layout->itemAt(i);
            if (!item) continue;

            if (QWidget *widget = item->widget()) {
                widget->setEnabled(enabled);
            } else if (QLayout *childLayout = item->layout()) {
                setLayoutItemsEnabled(childLayout, enabled);
            }
        }
    }

    QImage wrapImageViewToImage(const ImageView &view) {
        if (!view.outputPixels || view.outputWidth <= 0 || view.outputHeight <= 0) {
            return {};
        }

        const int bytesPerLine
            = view.downscaling ? view.outputWidth * 3 : view.strideWidth;
        return QImage(view.outputPixels, view.outputWidth, view.outputHeight,
            bytesPerLine, QImage::Format_RGB888);
    }

    QImage imageViewToImage(const ImageView &view) {
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

    void setAdaptiveSpinValue(QDoubleSpinBox *spinBox, double value) {
        if (!spinBox) return;

        if (auto *adaptiveSpinBox
            = dynamic_cast<::AdaptiveDoubleSpinBox *>(spinBox)) {
            adaptiveSpinBox->resetDisplayDecimals();
        }

        spinBox->setValue(value);
    }

    void resizeViewportToImageSize(QWidget *viewport, const QSize &imageSize) {
        if (!viewport || !imageSize.isValid()) return;

        const double dpr = effectiveDevicePixelRatio(viewport);
        const QSize logicalSize(
            std::max(1,
                static_cast<int>(
                    std::lround(static_cast<double>(imageSize.width()) / dpr))),
            std::max(1,
                static_cast<int>(std::lround(
                    static_cast<double>(imageSize.height()) / dpr))));

        viewport->showNormal();
        viewport->setMinimumSize(logicalSize);
        viewport->setMaximumSize(logicalSize);
        viewport->resize(logicalSize);
    }
}