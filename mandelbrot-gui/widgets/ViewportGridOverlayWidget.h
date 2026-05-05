#pragma once

#include <QWidget>

class ViewportGridOverlayWidget final : public QWidget {
public:
    explicit ViewportGridOverlayWidget(QWidget *parent = nullptr);

    void setGridDivisions(int divisions);
    [[nodiscard]] int gridDivisions() const { return _gridDivisions; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int _gridDivisions = 0;
};
