#include "CollapsibleGroupBox.h"

#include <QApplication>
#include <QLayout>
#include <QPalette>

static void setLayoutItemsEnabled(QLayout *layout, bool enabled) {
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

static void setLayoutItemsVisible(QLayout *layout, bool visible) {
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
        setLayoutItemsVisible(boxLayout, expanded);
    }

    QPalette pal = palette();
    const QPalette appPalette = QApplication::palette(this);
    pal.setColor(QPalette::WindowText,
        expanded ? appPalette.color(QPalette::WindowText)
        : appPalette.color(QPalette::Disabled, QPalette::WindowText));
    setPalette(pal);
}

void CollapsibleGroupBox::setContentEnabled(bool enabled) {
    setLayoutItemsEnabled(layout(), enabled);
    setEnabled(true);

    QPalette pal = palette();
    const QPalette appPalette = QApplication::palette(this);
    pal.setColor(QPalette::WindowText,
        appPalette.color(enabled ? QPalette::WindowText : QPalette::Mid));
    setPalette(pal);
}