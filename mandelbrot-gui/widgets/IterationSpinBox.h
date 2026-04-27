#pragma once

#include <functional>

#include <QSpinBox>

class IterationSpinBox final : public QSpinBox {
    Q_OBJECT

public:
    explicit IterationSpinBox(QWidget *parent = nullptr);

    std::function<int()> resolveAutoIterations;

protected:
    QValidator::State validate(QString &input, int &pos) const override;
    int valueFromText(const QString &text) const override;
    QString textFromValue(int value) const override;
    void fixup(QString &input) const override;
    void wheelEvent(QWheelEvent *event) override;
};
