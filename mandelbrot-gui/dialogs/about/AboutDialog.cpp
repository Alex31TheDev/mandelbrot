#include "AboutDialog.h"
#include "ui_AboutDialog.h"

#include <QLabel>


AboutDialog::AboutDialog(const QIcon &icon, QWidget *parent)
    : QDialog(parent)
    , _ui(std::make_unique<Ui::AboutDialog>()) {
    _ui->setupUi(this);
    _ui->iconLabel->setPixmap(icon.pixmap(56, 56));
}

AboutDialog::~AboutDialog() = default;