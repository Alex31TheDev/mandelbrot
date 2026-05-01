#include "CollapsibleGroupBox.h"

#include <QApplication>
#include <QPalette>

#include "util/GUIUtil.h"

using namespace GUI;

CollapsibleGroupBox::CollapsibleGroupBox(QWidget *parent)
    : QGroupBox(parent) {
    _init();
}

CollapsibleGroupBox::CollapsibleGroupBox(const QString &title, QWidget *parent)
    : QGroupBox(title, parent) {
    _init();
}

void CollapsibleGroupBox::_init() {
    setCheckable(true);
    setChecked(true);
    setFlat(true);
    QObject::connect(this, &QGroupBox::toggled, this, [this](bool checked) {
        applyExpandedState(checked);
        emit expandedChanged(checked);
        });
}

void CollapsibleGroupBox::applyExpandedState(bool expanded) {
    if (QLayout *boxLayout = layout()) {
        Util::setLayoutItemsVisible(boxLayout, expanded);
    }

    QPalette pal = palette();
    const QPalette appPalette = QApplication::palette(this);
    pal.setColor(QPalette::WindowText,
        expanded ? appPalette.color(QPalette::WindowText)
        : appPalette.color(QPalette::Disabled, QPalette::WindowText));
    setPalette(pal);
}

void CollapsibleGroupBox::setContentEnabled(bool enabled) {
    Util::setLayoutItemsEnabled(layout(), enabled);
    setEnabled(true);

    QPalette pal = palette();
    const QPalette appPalette = QApplication::palette(this);
    pal.setColor(QPalette::WindowText,
        appPalette.color(enabled ? QPalette::WindowText : QPalette::Mid));
    setPalette(pal);
}