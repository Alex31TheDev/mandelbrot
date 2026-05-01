#pragma once

#include <memory>

#include <QDialog>
#include <QIcon>

namespace Ui {
    class AboutDialog;
}

class AboutDialog final : public QDialog {
    Q_OBJECT

public:
    explicit AboutDialog(const QIcon &icon, QWidget *parent = nullptr);
    ~AboutDialog() override;

private:
    std::unique_ptr<Ui::AboutDialog> _ui;
};
