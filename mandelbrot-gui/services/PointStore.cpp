#include "PointStore.h"

#include <system_error>

#include <QCoreApplication>

#include "parsers/point/PointParser.h"
#include "parsers/point/PointWriter.h"

#include "util/FileUtil.h"

namespace GUI {
    std::filesystem::path PointStore::directoryPath() {
        return FileUtil::executableDir() / "views";
    }

    bool PointStore::ensureDirectory(QString &errorMessage) {
        errorMessage.clear();

        std::error_code ec;
        std::filesystem::create_directories(directoryPath(), ec);
        if (!ec) return true;

        errorMessage = QCoreApplication::translate("PointStore",
            "Failed to create views directory: %1")
            .arg(QString::fromStdString(ec.message()));
        return false;
    }

    bool PointStore::loadFromPath(
        const std::filesystem::path &path,
        PointConfig &point, QString &errorMessage
    ) {
        PointParser parser;
        std::string err;
        if (parser.parse(path.string(), point, err)) {
            errorMessage.clear();
            return true;
        }

        errorMessage = QString::fromStdString(err);
        return false;
    }

    bool PointStore::saveToPath(
        const std::filesystem::path &path,
        const PointConfig &point, QString &errorMessage
    ) {
        PointWriter writer(point);
        std::string err;
        if (writer.write(path.string(), err)) {
            errorMessage.clear();
            return true;
        }

        errorMessage = QString::fromStdString(err);
        return false;
    }
}