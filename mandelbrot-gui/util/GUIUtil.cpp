#include "util/GUIUtil.h"

#include <algorithm>
#include <cmath>
#include <system_error>

#include <QDoubleSpinBox>
#include <QLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QScreen>
#include <QWidget>
#include <QWindow>

#include "widgets/AdaptiveDoubleSpinBox.h"
#include "util/FormatUtil.h"
#include "util/PathUtil.h"
#include "util/StringUtil.h"

static constexpr auto kCommittedLineEditTextProperty
    = "_mandelbrotCommittedText";
static constexpr double kMinGUIZoom = -3.2499;

int GUI::Util::clampSliderValue(int value) {
    return std::clamp(value, 0, 20);
}

QString GUI::Util::committedLineEditText(const QLineEdit* edit) {
    return edit ? edit->text().trimmed() : QString();
}

void GUI::Util::markLineEditTextCommitted(QLineEdit* edit) {
    if (!edit) return;
    edit->setProperty(
        kCommittedLineEditTextProperty, committedLineEditText(edit));
}

void GUI::Util::setCommittedLineEditText(QLineEdit* edit, const QString& text) {
    if (!edit) return;
    edit->setText(text);
    markLineEditTextCommitted(edit);
}

bool GUI::Util::hasUncommittedLineEditChange(const QLineEdit* edit) {
    if (!edit) return false;
    return committedLineEditText(edit)
        != edit->property(kCommittedLineEditTextProperty).toString();
}

QString GUI::Util::decorateUnsavedLabel(const QString& name, bool unsaved) {
    if (!unsaved) return name;
    return QString::fromStdString(
        StringUtil::appendSuffix(name.toStdString(), " (unsaved)"));
}

QString GUI::Util::undecoratedLabel(const QString& name) {
    return QString::fromStdString(
        StringUtil::stripSuffix(name.toStdString(), " (unsaved)"));
}

QString GUI::Util::uniqueIndexedNameFromList(
    const QString& baseName, const QStringList& existingNames) {
    return QString::fromStdString(
        FormatUtil::uniqueIndexedName(baseName.toStdString(),
            [&existingNames](const std::string_view candidate) {
                return existingNames.contains(
                    QString::fromUtf8(
                        candidate.data(), static_cast<int>(candidate.size())),
                    Qt::CaseInsensitive);
            }));
}

QString GUI::Util::uniqueIndexedPathWithExtension(
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

QString GUI::Util::fractalTypeToConfigString(Backend::FractalType fractalType) {
    switch (fractalType) {
    case Backend::FractalType::mandelbrot:
        return "mandelbrot";
    case Backend::FractalType::perpendicular:
        return "perpendicular";
    case Backend::FractalType::burningship:
        return "burningship";
    }
    return "mandelbrot";
}

Backend::FractalType GUI::Util::fractalTypeFromConfigString(
    const QString& value) {
    const QString normalized = value.trimmed().toLower();
    if (normalized == "perpendicular")
        return Backend::FractalType::perpendicular;
    if (normalized == "burningship" || normalized == "burning_ship"
        || normalized == "burning ship") {
        return Backend::FractalType::burningship;
    }
    return Backend::FractalType::mandelbrot;
}

double GUI::Util::clampGUIZoom(double zoom) {
    return std::max(zoom, kMinGUIZoom);
}

QString GUI::Util::formatImageMemoryText(const Backend::ImageEvent& event) {
    const QString renderBytes = QString::fromStdString(
        FormatUtil::formatBufferSize(event.primaryBytes));
    if (!event.downscaling) {
        return QString("Render: %1").arg(renderBytes);
    }

    const QString outputBytesText = QString::fromStdString(
        FormatUtil::formatBufferSize(event.secondaryBytes));
    return QString("Render: %1  Output: %2").arg(renderBytes, outputBytesText);
}

QColor GUI::Util::lightColorToQColor(const Backend::LightColor& color) {
    return QColor::fromRgbF(std::clamp(static_cast<double>(color.R), 0.0, 1.0),
        std::clamp(static_cast<double>(color.G), 0.0, 1.0),
        std::clamp(static_cast<double>(color.B), 0.0, 1.0));
}

Backend::LightColor GUI::Util::lightColorFromQColor(const QColor& color) {
    return { .R = static_cast<float>(color.redF()),
        .G = static_cast<float>(color.greenF()),
        .B = static_cast<float>(color.blueF()) };
}

QString GUI::Util::lightColorButtonStyle(const QColor& color) {
    const int luminance
        = (color.red() * 299 + color.green() * 587 + color.blue() * 114) / 1000;
    const QColor fg = luminance >= 160 ? QColor(Qt::black) : QColor(Qt::white);

    return QString("QPushButton {"
                   " background-color: %1;"
                   " color: %2;"
                   "}")
        .arg(color.name(QColor::HexRgb), fg.name(QColor::HexRgb));
}

std::filesystem::path GUI::Util::savesDirectoryPath() {
    return PathUtil::executableDir() / "saves";
}

void GUI::Util::stabilizePushButton(QPushButton* button) {
    if (!button) return;
    button->setAutoDefault(false);
    button->setDefault(false);
}

void GUI::Util::setLayoutItemsVisible(QLayout* layout, bool visible) {
    if (!layout) return;

    for (int i = 0; i < layout->count(); ++i) {
        QLayoutItem* item = layout->itemAt(i);
        if (!item) continue;

        if (QWidget* widget = item->widget()) {
            widget->setVisible(visible);
        } else if (QLayout* childLayout = item->layout()) {
            setLayoutItemsVisible(childLayout, visible);
        }
    }
}

void GUI::Util::setLayoutItemsEnabled(QLayout* layout, bool enabled) {
    if (!layout) return;

    for (int i = 0; i < layout->count(); ++i) {
        QLayoutItem* item = layout->itemAt(i);
        if (!item) continue;

        if (QWidget* widget = item->widget()) {
            widget->setEnabled(enabled);
        } else if (QLayout* childLayout = item->layout()) {
            setLayoutItemsEnabled(childLayout, enabled);
        }
    }
}

QImage GUI::Util::wrapImageViewToImage(const Backend::ImageView& view) {
    if (!view.outputPixels || view.outputWidth <= 0 || view.outputHeight <= 0) {
        return {};
    }

    const int bytesPerLine
        = view.downscaling ? view.outputWidth * 3 : view.strideWidth;
    return QImage(view.outputPixels, view.outputWidth, view.outputHeight,
        bytesPerLine, QImage::Format_RGB888);
}

QImage GUI::Util::imageViewToImage(const Backend::ImageView& view) {
    const QImage wrapped = wrapImageViewToImage(view);
    return wrapped.copy();
}

QImage GUI::Util::makeBlankViewportImage() {
    QImage image(1, 1, QImage::Format_RGB888);
    image.fill(Qt::black);
    return image;
}

double GUI::Util::effectiveDevicePixelRatio(const QWidget* widget) {
    if (!widget) return 1.0;

    if (const QWindow* window = widget->windowHandle()) {
        return std::max(1.0, window->devicePixelRatio());
    }
    if (const QScreen* screen = widget->screen()) {
        return std::max(1.0, screen->devicePixelRatio());
    }
    return std::max(1.0, widget->devicePixelRatioF());
}

void GUI::Util::setAdaptiveSpinValue(QDoubleSpinBox* spinBox, double value) {
    if (!spinBox) return;

    if (auto* adaptiveSpinBox
        = dynamic_cast<::AdaptiveDoubleSpinBox*>(spinBox)) {
        adaptiveSpinBox->resetDisplayDecimals();
    }

    spinBox->setValue(value);
}

void GUI::Util::resizeViewportToImageSize(
    QWidget* viewport, const QSize& imageSize) {
    if (!viewport || !imageSize.isValid()) return;

    const double dpr = effectiveDevicePixelRatio(viewport);
    const QSize logicalSize(
        std::max(1,
            static_cast<int>(
                std::lround(static_cast<double>(imageSize.width()) / dpr))),
        std::max(1,
            static_cast<int>(
                std::lround(static_cast<double>(imageSize.height()) / dpr))));

    viewport->showNormal();
    viewport->setMinimumSize(logicalSize);
    viewport->setMaximumSize(logicalSize);
    viewport->resize(logicalSize);
}
