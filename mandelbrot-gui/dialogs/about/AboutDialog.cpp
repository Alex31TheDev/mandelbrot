#include "AboutDialog.h"
#include "ui_AboutDialog.h"

#include <QCoreApplication>
#include <QLabel>
#include <QtVersion>


AboutDialog::AboutDialog(const QIcon &icon, QWidget *parent)
    : QDialog(parent)
    , _ui(std::make_unique<Ui::AboutDialog>()) {
    _ui->setupUi(this);
    _ui->iconLabel->setPixmap(icon.pixmap(56, 56));
    _ui->aboutVersion->setText(tr("Version %1")
        .arg(QCoreApplication::applicationVersion()));
    _ui->aboutQtVersion->setText(tr("Qt runtime %1")
        .arg(QString::fromLatin1(qVersion())));
}

AboutDialog::~AboutDialog() = default;
