#include "dialogs/help/HelpDialog.h"

#include "ui_HelpDialog.h"

HelpDialog::HelpDialog(QWidget *parent)
    : QDialog(parent)
    , _ui(std::make_unique<Ui::HelpDialog>()) {
    _ui->setupUi(this);
    _ui->helpText->setHtml(_buildHelpHtml());
}

HelpDialog::~HelpDialog() = default;

QString HelpDialog::_buildHelpHtml() const {
    const QString overview = tr(
        "Use the control window to edit render parameters, colors, output settings, "
        "and saved views. Use the viewport window to inspect the image and navigate "
        "through the fractal.");
    const QString mouse = tr(
        "Left click uses the current navigation mode.<br/>"
        "Right click zooms out in zoom modes or pans in Pan mode.<br/>"
        "Mouse wheel zooms around the pointer.<br/>"
        "Middle-button drag temporarily pans in any mode while held.");
    const QString navigationModes = tr(
        "Realtime Zoom continuously zooms while the mouse button is held.<br/>"
        "Box Zoom draws a rectangle and fits that area into the viewport.<br/>"
        "Pan drags the current view without changing the zoom target.");
    const QString buttons = tr(
        "Render starts a new image with the current settings.<br/>"
        "Home restores the default view.<br/>"
        "Zoom moves toward the center of the current viewport.<br/>"
        "Save writes the current image to disk.");
    const QString settings = tr(
        "Open Settings to review or change shortcuts, save locations, language, "
        "viewport overlays, output sizing defaults, and interaction tuning.");
    const QString files = tr(
        "Views save and load the current location and zoom. Palette and sine-color "
        "presets can be saved, imported, and reused from their editors.");

    const auto section = [](const QString &title, const QString &body) {
        return QStringLiteral("<h3>%1</h3><p>%2</p>").arg(title, body);
        };

    return QStringLiteral(
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta charset=\"utf-8\"/>"
        "<style>"
        "body { font-family: 'Segoe UI'; font-size: 9pt; }"
        "h2 { margin: 0 0 12px 0; }"
        "h3 { margin: 14px 0 8px 0; }"
        "p { margin: 0 0 8px 0; white-space: normal; }"
        "</style>"
        "</head>"
        "<body>"
        "<h2>%1</h2>"
        "%2%3%4%5%6%7"
        "</body>"
        "</html>")
        .arg(tr("Using the interface"),
            section(tr("Overview"), overview),
            section(tr("Mouse navigation"), mouse),
            section(tr("Navigation modes"), navigationModes),
            section(tr("Main actions"), buttons),
            section(tr("Settings"), settings),
            section(tr("Files and presets"), files));
}