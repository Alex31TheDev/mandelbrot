#pragma once

#include <QGroupBox>

class CollapsibleGroupBox final : public QGroupBox {
    Q_OBJECT

public:
    explicit CollapsibleGroupBox(QWidget *parent = nullptr);
    explicit CollapsibleGroupBox(
        const QString &title, QWidget *parent = nullptr
    );

    void applyExpandedState(bool expanded);
    void setContentEnabled(bool enabled);

signals:
    void expandedChanged(bool expanded);

private:
    void _init();
};
