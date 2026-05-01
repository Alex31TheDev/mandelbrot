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
    [[nodiscard]] QString _buildHelpHtml() const;

    std::unique_ptr<Ui::HelpDialog> _ui;
};
