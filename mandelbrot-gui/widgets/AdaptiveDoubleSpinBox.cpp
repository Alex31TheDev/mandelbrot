#include "AdaptiveDoubleSpinBox.h"

#include <algorithm>

#include <QLineEdit>
#include <QLocale>

AdaptiveDoubleSpinBox::AdaptiveDoubleSpinBox(QWidget *parent)
    : QDoubleSpinBox(parent) {
    QDoubleSpinBox::setDecimals(17);
    setKeyboardTracking(false);
    if (QLineEdit *edit = lineEdit()) {
        QObject::connect(edit, &QLineEdit::textEdited, this,
            [this](const QString &text) { _updateDisplayDecimals(text); });
    }
}

AdaptiveDoubleSpinBox::AdaptiveDoubleSpinBox(
    int defaultDisplayDecimals, QWidget *parent)
    : QDoubleSpinBox(parent)
    , _defaultDisplayDecimals(std::max(0, defaultDisplayDecimals))
    , _displayDecimals(_defaultDisplayDecimals) {
    QDoubleSpinBox::setDecimals(17);
    setKeyboardTracking(false);
    if (QLineEdit *edit = lineEdit()) {
        QObject::connect(edit, &QLineEdit::textEdited, this,
            [this](const QString &text) { _updateDisplayDecimals(text); });
    }
}

void AdaptiveDoubleSpinBox::setDefaultDisplayDecimals(int decimals) {
    _defaultDisplayDecimals = std::max(0, decimals);
    resetDisplayDecimals();
}

void AdaptiveDoubleSpinBox::resetDisplayDecimals() {
    _displayDecimals = _defaultDisplayDecimals;
}

QString AdaptiveDoubleSpinBox::textFromValue(double value) const {
    return locale().toString(value, 'f', _displayDecimals);
}

QValidator::State AdaptiveDoubleSpinBox::validate(
    QString &text, int &pos) const {
    return QDoubleSpinBox::validate(text, pos);
}

double AdaptiveDoubleSpinBox::valueFromText(const QString &text) const {
    return QDoubleSpinBox::valueFromText(text);
}

void AdaptiveDoubleSpinBox::stepBy(int steps) {
    resetDisplayDecimals();
    QDoubleSpinBox::stepBy(steps);
}

void AdaptiveDoubleSpinBox::_updateDisplayDecimals(const QString &text) {
    const QString decimalPoint = locale().decimalPoint();
    const int decimalIndex = text.indexOf(decimalPoint);
    if (decimalIndex < 0) {
        resetDisplayDecimals();
        return;
    }

    int typedDecimals = 0;
    for (int i = decimalIndex + 1; i < text.size(); i++) {
        if (!text[i].isDigit()) break;
        typedDecimals++;
    }

    _displayDecimals = std::max(_defaultDisplayDecimals, typedDecimals);
}