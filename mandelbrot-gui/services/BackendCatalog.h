#pragma once

#include <QString>
#include <QStringList>

namespace GUI::BackendCatalog {
    [[nodiscard]] int typeRank(const QString &name);
    [[nodiscard]] int precisionRank(const QString &name);
    [[nodiscard]] QStringList listNames(QString &errorMessage);
}
