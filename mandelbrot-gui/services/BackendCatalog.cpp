#include "BackendCatalog.h"

#include <algorithm>
#include <filesystem>
#include <system_error>

#include <QCoreApplication>

#include "util/FileUtil.h"

namespace GUI {
    int BackendCatalog::typeRank(const QString &name) {
        const QString normalized = name.toLower();
        if (normalized.contains("scalar")) return 0;
        if (normalized.contains("avx") || normalized.contains("sse")
            || normalized.contains("vector")) {
            return 1;
        }
        if (normalized.contains("mpfr")) return 2;
        return 3;
    }

    int BackendCatalog::precisionRank(const QString &name) {
        const QString normalized = name.toLower();
        if (normalized.contains("float")) return 0;
        if (normalized.contains("double")) return 1;
        if (normalized.contains("qd")) return 2;
        if (normalized.contains("mpfr")) return 3;
        return 4;
    }

    QStringList BackendCatalog::listNames(QString &errorMessage) {
        errorMessage.clear();
        QStringList names;

        std::error_code ec;
        const std::filesystem::path dir = FileUtil::executableDir() / "backends";
        if (!std::filesystem::exists(dir, ec) || ec) {
            errorMessage = QCoreApplication::translate(
                "BackendCatalog", "Backend directory was not found.");
            return names;
        }

        for (const auto &entry : std::filesystem::directory_iterator(dir, ec)) {
            if (ec) break;
            if (!entry.is_regular_file()) continue;

            const std::filesystem::path path = entry.path();
            if (path.extension() != ".dll") continue;
            const QString stem = QString::fromStdWString(path.stem().wstring());
            if (!stem.contains(" - ")) continue;
            names.push_back(stem);
        }

        names.removeDuplicates();
        std::sort(
            names.begin(), names.end(), [](const QString &a, const QString &b) {
                const int typeA = typeRank(a);
                const int typeB = typeRank(b);
                if (typeA != typeB) return typeA < typeB;

                const int precisionA = precisionRank(a);
                const int precisionB = precisionRank(b);
                if (precisionA != precisionB) return precisionA < precisionB;

                return a.compare(b, Qt::CaseInsensitive) < 0;
            }
        );
        if (names.isEmpty()) {
            errorMessage = QCoreApplication::translate(
                "BackendCatalog",
                "No backend DLLs were found in the backends directory.");
        }

        return names;
    }
}