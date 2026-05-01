#pragma once

#include <QDoubleSpinBox>

class AdaptiveDoubleSpinBox final : public QDoubleSpinBox {
    Q_OBJECT

public:
    explicit AdaptiveDoubleSpinBox(QWidget *parent = nullptr);
    explicit AdaptiveDoubleSpinBox(
        int defaultDisplayDecimals, QWidget *parent = nullptr
    );

    void setDefaultDisplayDecimals(int decimals);
    void resetDisplayDecimals();

protected:
    QString textFromValue(double value) const override;
    QValidator::State validate(QString &text, int &pos) const override;
    double valueFromText(const QString &text) const override;
    void stepBy(int steps) override;

private:
    int _defaultDisplayDecimals = 2;
    int _displayDecimals = 2;

    void _updateDisplayDecimals(const QString &text);
};
