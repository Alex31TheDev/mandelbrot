#pragma once

#include <QPixmap>
#include <QWidget>

class PalettePreviewWidget final : public QWidget {
    Q_OBJECT

public:
    explicit PalettePreviewWidget(QWidget *parent = nullptr);

    void setPreviewPixmap(const QPixmap &pixmap);
    void clearPreview();

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QPixmap _previewPixmap;
};
