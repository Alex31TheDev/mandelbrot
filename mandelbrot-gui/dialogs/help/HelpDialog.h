#pragma once

#include <memory>

#include <QDialog>
#include <QString>
#include <QTextBrowser>
#include <QVariant>

class QHelpEngine;
class QUrl;

namespace Ui {
    class HelpDialog;
}

class HelpBrowser final : public QTextBrowser {
    Q_OBJECT

public:
    explicit HelpBrowser(QWidget *parent = nullptr);

    void setHelpEngine(QHelpEngine *helpEngine);

protected:
    QVariant loadResource(int type, const QUrl &name) override;

private:
    QHelpEngine *_helpEngine = nullptr;
};

class HelpDialog final : public QDialog {
    Q_OBJECT

public:
    explicit HelpDialog(QWidget *parent = nullptr);
    ~HelpDialog() override;

private:
    void _showPage(const QUrl &url);
    void _showHomePage();
    void _showStatusMessage(const QString &message);

    std::unique_ptr<Ui::HelpDialog> _ui;
    std::unique_ptr<QHelpEngine> _helpEngine;
};
