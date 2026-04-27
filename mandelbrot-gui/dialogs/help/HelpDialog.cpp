#include "dialogs/help/HelpDialog.h"

#include "ui_HelpDialog.h"

HelpDialog::HelpDialog(QWidget* parent)
    : QDialog(parent)
    , _ui(std::make_unique<Ui::HelpDialog>()) {
    _ui->setupUi(this);
}

HelpDialog::~HelpDialog() = default;
