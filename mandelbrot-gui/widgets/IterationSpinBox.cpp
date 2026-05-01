#include "IterationSpinBox.h"

#include <algorithm>

#include <QLineEdit>
#include <QWheelEvent>

IterationSpinBox::IterationSpinBox(QWidget *parent)
    : QSpinBox(parent) {
    setKeyboardTracking(false);
    setCorrectionMode(QAbstractSpinBox::CorrectToNearestValue);
}

QValidator::State IterationSpinBox::validate(QString &input, int &pos) const {
    const QString trimmed = input.trimmed();
    if (trimmed.isEmpty()) return QValidator::Intermediate;
    if (trimmed.compare(specialValueText(), Qt::CaseInsensitive) == 0) {
        return QValidator::Acceptable;
    }
    if (specialValueText().startsWith(trimmed, Qt::CaseInsensitive)) {
        return QValidator::Intermediate;
    }

    return QSpinBox::validate(input, pos);
}

int IterationSpinBox::valueFromText(const QString &text) const {
    if (text.trimmed().compare(specialValueText(), Qt::CaseInsensitive) == 0) {
        return minimum();
    }
    return QSpinBox::valueFromText(text);
}

QString IterationSpinBox::textFromValue(int value) const {
    if (value <= minimum()) {
        return specialValueText();
    }
    return QSpinBox::textFromValue(value);
}

void IterationSpinBox::fixup(QString &input) const {
    if (input.trimmed().compare(specialValueText(), Qt::CaseInsensitive) == 0) {
        input = specialValueText();
        return;
    }

    QSpinBox::fixup(input);
}

void IterationSpinBox::wheelEvent(QWheelEvent *event) {
    const int steps = event->angleDelta().y() / 120;
    if (steps == 0) {
        QSpinBox::wheelEvent(event);
        return;
    }

    int nextValue = value();
    if (nextValue <= minimum()) {
        nextValue = std::clamp(
            resolveAutoIterations ? resolveAutoIterations() : 1, 1, maximum());
        setValue(nextValue);
        event->accept();
        return;
    }

    for (int i = 0; i < std::abs(steps); i++) {
        if (steps > 0) {
            nextValue = std::clamp(
                nextValue > maximum() / 2 ? maximum() : nextValue * 2,
                minimum(), maximum());
        } else {
            nextValue = std::clamp(nextValue / 2, 1, maximum());
        }
    }

    setValue(nextValue);
    event->accept();
}