#include "settings/Shortcuts.h"

#include <QCoreApplication>
#include <QMap>
#include <QObject>

const std::vector<ShortcutDef> &Shortcuts::defs() {
    static const std::vector<ShortcutDef> defs = {
        { "cancel", QT_TRANSLATE_NOOP("Shortcuts", "Cancel render"),
            QKeySequence(Qt::Key_Escape), true },
        { "home", QT_TRANSLATE_NOOP("Shortcuts", "Home view"),
            QKeySequence(Qt::Key_H) },
        { "cycleMode", QT_TRANSLATE_NOOP("Shortcuts", "Cycle navigation mode"),
            QKeySequence(Qt::Key_Z) },
        { "cycleGrid", QT_TRANSLATE_NOOP("Shortcuts", "Cycle grid"),
            QKeySequence(Qt::Key_G) },
        { "overlay", QT_TRANSLATE_NOOP("Shortcuts", "Toggle viewport overlay"),
            QKeySequence(Qt::Key_F1) },
        { "quickSave", QT_TRANSLATE_NOOP("Shortcuts", "Quick save image"),
            QKeySequence(Qt::Key_F2) },
        { "fullscreen", QT_TRANSLATE_NOOP("Shortcuts", "Toggle fullscreen"),
            QKeySequence(Qt::Key_F12) },
        { "calculate", QT_TRANSLATE_NOOP("Shortcuts", "Calculate"),
            QKeySequence(Qt::CTRL | Qt::Key_R) },
        { "saveImage", QT_TRANSLATE_NOOP("Shortcuts", "Save image"),
            QKeySequence::Save },
        { "openView", QT_TRANSLATE_NOOP("Shortcuts", "Open view"),
            QKeySequence::Open },
        { "saveView", QT_TRANSLATE_NOOP("Shortcuts", "Save view"),
            QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S) },
        { "settings", QT_TRANSLATE_NOOP("Shortcuts", "Settings"),
            QKeySequence(Qt::CTRL | Qt::Key_Comma) },
        { "exit", QT_TRANSLATE_NOOP("Shortcuts", "Exit"), QKeySequence::Quit }
    };
    return defs;
}

QString Shortcuts::label(const ShortcutDef &def) {
    return QCoreApplication::translate("Shortcuts", def.label);
}

Shortcuts::Shortcuts(AppSettings &settings)
    : _settings(settings) {
    reload();
}

void Shortcuts::reload() {
    _sequences.clear();
    for (const ShortcutDef &def : defs()) {
        const QString stored = _settings.shortcut(
            def.id, def.defaultSequence.toString(QKeySequence::PortableText));
        _sequences.push_back(
            QKeySequence::fromString(stored, QKeySequence::PortableText));
    }
}

QKeySequence Shortcuts::sequence(const QString &id) const {
    const auto &items = defs();
    for (size_t i = 0; i < items.size(); ++i) {
        if (items[i].id == id) return _sequences[i];
    }
    return {};
}

void Shortcuts::setSequence(const QString &id, const QKeySequence &sequence) {
    const auto &items = defs();
    for (size_t i = 0; i < items.size(); ++i) {
        if (items[i].id == id) {
            _sequences[i] = sequence;
            return;
        }
    }
}

void Shortcuts::save() {
    const auto &items = defs();
    for (size_t i = 0; i < items.size(); ++i) {
        _settings.setShortcut(
            items[i].id, _sequences[i].toString(QKeySequence::PortableText));
    }
    _settings.sync();
}

QString Shortcuts::duplicateError() const {
    QMap<QString, QString> seen;
    const auto &items = defs();
    for (size_t i = 0; i < items.size(); ++i) {
        const QKeySequence sequence = _sequences[i];
        if (sequence.isEmpty()) {
            if (items[i].required) {
                return QObject::tr("%1 cannot be empty.").arg(label(items[i]));
            }
            continue;
        }

        const QString key = sequence.toString(QKeySequence::PortableText);
        const auto it = seen.constFind(key);
        if (it != seen.constEnd()) {
            return QObject::tr("%1 and %2 both use %3.")
                .arg(it.value(), label(items[i]), key);
        }
        seen.insert(key, label(items[i]));
    }
    return {};
}
