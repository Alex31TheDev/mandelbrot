#include "HelpDialog.h"
#include "ui_HelpDialog.h"

#include <filesystem>

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QHelpContentModel>
#include <QHelpContentWidget>
#include <QHelpEngine>
#include <QHelpIndexWidget>
#include <QHelpLink>
#include <QStandardPaths>
#include <QUrl>

#include "util/FileUtil.h"

static constexpr auto helpNamespace = "com.alex31thedev.mandelbrot.gui";
static constexpr auto helpVirtualFolder = "doc";
static constexpr auto helpHomePage = "index.html";

static std::filesystem::path helpFilePath(const char *fileName) {
    return FileUtil::executableDir() / "help" / fileName;
}

static QString helpCollectionPath() {
    QString writableDir = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation
    );
    if (writableDir.isEmpty()) {
        writableDir = QString::fromStdWString(FileUtil::executableDir().wstring());
    }

    QDir().mkpath(writableDir);
    return QDir(writableDir).filePath(QStringLiteral("mandelbrot-gui-help.qhc"));
}

static QString helpQchPath() {
    return QString::fromStdWString(
        helpFilePath("mandelbrot-gui-help.qch").wstring()
    );
}

static QUrl helpHomeUrl() {
    return QUrl(QStringLiteral("qthelp://%1/%2/%3")
        .arg(QLatin1String(helpNamespace),
            QLatin1String(helpVirtualFolder),
            QLatin1String(helpHomePage)));
}

HelpBrowser::HelpBrowser(QWidget *parent)
    : QTextBrowser(parent) {
    setOpenExternalLinks(false);
    setOpenLinks(false);
}

void HelpBrowser::setHelpEngine(QHelpEngine *helpEngine) {
    _helpEngine = helpEngine;
}

QVariant HelpBrowser::loadResource(int type, const QUrl &name) {
    if (_helpEngine && name.scheme() == QStringLiteral("qthelp")) {
        const QByteArray data = _helpEngine->fileData(name);
        if (!data.isEmpty()) {
            return data;
        }
    }

    return QTextBrowser::loadResource(type, name);
}

HelpDialog::HelpDialog(QWidget *parent)
    : QDialog(parent)
    , _ui(std::make_unique<Ui::HelpDialog>())
    , _helpEngine(std::make_unique<QHelpEngine>(helpCollectionPath(), this)) {
    _ui->setupUi(this);
    _ui->helpBrowser->setHelpEngine(_helpEngine.get());

    connect(_ui->actionBack, &QAction::triggered,
        _ui->helpBrowser, &QTextBrowser::backward);
    connect(_ui->actionForward, &QAction::triggered,
        _ui->helpBrowser, &QTextBrowser::forward);
    connect(_ui->actionHome, &QAction::triggered, this, &HelpDialog::_showHomePage);
    connect(_ui->helpBrowser, &QTextBrowser::backwardAvailable,
        _ui->actionBack, &QAction::setEnabled);
    connect(_ui->helpBrowser, &QTextBrowser::forwardAvailable,
        _ui->actionForward, &QAction::setEnabled);
    connect(_ui->helpBrowser, &QTextBrowser::anchorClicked, this,
        [this](const QUrl &url) {
            const QUrl resolvedUrl = _ui->helpBrowser->source().resolved(url);
            if (resolvedUrl.scheme() == QStringLiteral("qthelp")) {
                _showPage(resolvedUrl);
                return;
            }

            QDesktopServices::openUrl(resolvedUrl);
        });

    _ui->actionBack->setEnabled(false);
    _ui->actionForward->setEnabled(false);

    _helpEngine->setReadOnly(false);
    if (!_helpEngine->setupData()) {
        _showStatusMessage(tr("Help setup failed: %1").arg(_helpEngine->error()));
        return;
    }

    const QString qchPath = QFileInfo(helpQchPath()).absoluteFilePath();
    if (!QFileInfo::exists(qchPath)) {
        _showStatusMessage(
            tr("Help file not found: %1").arg(QDir::toNativeSeparators(qchPath))
        );
        return;
    }

    const QString registeredPath
        = QFileInfo(_helpEngine->documentationFileName(
            QLatin1String(helpNamespace)
        )).absoluteFilePath();
    if (!registeredPath.isEmpty()
        && registeredPath.compare(qchPath, Qt::CaseInsensitive) != 0) {
        _helpEngine->unregisterDocumentation(QLatin1String(helpNamespace));
    }
    if (_helpEngine->documentationFileName(QLatin1String(helpNamespace)).isEmpty()) {
        if (!_helpEngine->registerDocumentation(qchPath)) {
            _showStatusMessage(
                tr("Help registration failed: %1").arg(_helpEngine->error())
            );
            return;
        }
        _helpEngine->setupData();
    }

    auto *contentWidget = _helpEngine->contentWidget();
    auto *indexWidget = _helpEngine->indexWidget();
    _ui->contentsLayout->addWidget(contentWidget);
    _ui->indexWidgetLayout->addWidget(indexWidget);

    connect(contentWidget, &QHelpContentWidget::linkActivated,
        this, &HelpDialog::_showPage);
    connect(contentWidget, &QHelpContentWidget::clicked, this,
        [this, contentWidget](const QModelIndex &index) {
            if (!index.isValid()) {
                return;
            }

            if (auto *model = contentWidget->model()) {
                auto *contentModel = qobject_cast<QHelpContentModel *>(model);
                if (!contentModel) {
                    return;
                }

                if (auto *item = contentModel->contentItemAt(index)) {
                    _showPage(item->url());
                }
            }
        });
    connect(indexWidget, &QHelpIndexWidget::documentActivated, this,
        [this](const QHelpLink &document, const QString &) {
            _showPage(document.url);
        });
    connect(indexWidget, &QHelpIndexWidget::clicked, this,
        [indexWidget](const QModelIndex &) { indexWidget->activateCurrentItem(); });
    connect(_ui->indexFilterEdit, &QLineEdit::textChanged, this,
        [indexWidget](const QString &text) { indexWidget->filterIndices(text); });
    connect(_ui->indexFilterEdit, &QLineEdit::returnPressed,
        indexWidget, &QHelpIndexWidget::activateCurrentItem);

    _ui->splitter->setSizes({ 230, 560 });
    _showHomePage();
}

HelpDialog::~HelpDialog() = default;

void HelpDialog::_showPage(const QUrl &url) {
    if (!url.isValid()) {
        return;
    }

    _ui->helpBrowser->setSource(url);
}

void HelpDialog::_showHomePage() {
    _showPage(helpHomeUrl());
}

void HelpDialog::_showStatusMessage(const QString &message) {
    _ui->navTabs->setEnabled(false);
    _ui->indexFilterEdit->clear();
    _ui->indexFilterEdit->setEnabled(false);
    _ui->helpBrowser->setHtml(QStringLiteral("<p>%1</p>").arg(message.toHtmlEscaped()));
}