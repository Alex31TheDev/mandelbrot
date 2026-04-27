#pragma once

#include <memory>

#include <QApplication>
#include <QObject>
#include <QString>
#include <QTranslator>

class GUILocale final : public QObject {
    Q_OBJECT

public:
    explicit GUILocale(QApplication &app);

    [[nodiscard]] QString language() const { return _language; }
    bool setLanguage(const QString &language);

private:
    QApplication &_app;
    QString _language = "en";
    std::unique_ptr<QTranslator> _translator;
};
