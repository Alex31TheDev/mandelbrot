#pragma once

#include <vector>

#include <QKeySequence>
#include <QString>

#include "AppSettings.h"

struct ShortcutDef {
    QString id;
    const char *label;
    QKeySequence defaultSequence;
    bool required = false;
};

class Shortcuts {
public:
    static const std::vector<ShortcutDef> &defs();
    static QString label(const ShortcutDef &def);

    explicit Shortcuts(AppSettings &settings);

    [[nodiscard]] QKeySequence sequence(const QString &id) const;
    void setSequence(const QString &id, const QKeySequence &sequence);
    void reload();
    void save();
    [[nodiscard]] QString duplicateError() const;

private:
    AppSettings &_settings;
    std::vector<QKeySequence> _sequences;
};
