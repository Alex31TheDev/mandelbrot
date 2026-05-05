#pragma once

#include <QWidget>

#include "windows/viewport/ViewportHost.h"

class ViewportStatusOverlayWidget final : public QWidget {
public:
    explicit ViewportStatusOverlayWidget(QWidget *parent = nullptr);

    void setHost(ViewportHost *host);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    ViewportHost *_host = nullptr;

    [[nodiscard]] QRect _statusOverlayRect(const QString &statusText) const;
};
