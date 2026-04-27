#pragma once

#include <memory>

#include <QDialog>

namespace Ui {
class HelpDialog;
}

class HelpDialog final : public QDialog {
    Q_OBJECT

public:
    explicit HelpDialog(QWidget* parent = nullptr);
    ~HelpDialog() override;

private:
    std::unique_ptr<Ui::HelpDialog> _ui;
};
