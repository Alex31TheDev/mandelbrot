#pragma once

#include <memory>

#include <QByteArray>
#include <QSettings>
#include <QString>

class AppSettings {
public:
    AppSettings();

    [[nodiscard]] QString language() const;
    void setLanguage(const QString &language);

    [[nodiscard]] QString shortcut(
        const QString &id, const QString &fallback
    ) const;
    void setShortcut(const QString &id, const QString &sequence);

    [[nodiscard]] QByteArray controlWindowGeometry() const;
    void setControlWindowGeometry(const QByteArray &geometry);

    [[nodiscard]] QByteArray controlWindowState() const;
    void setControlWindowState(const QByteArray &state);

    [[nodiscard]] QString preferredBackend() const;
    void setPreferredBackend(const QString &backend);

    [[nodiscard]] int previewFallbackFPS() const;
    void setPreviewFallbackFPS(int fps);

    [[nodiscard]] int selectedOutputWidth() const;
    void setSelectedOutputWidth(int width);

    [[nodiscard]] int selectedOutputHeight() const;
    void setSelectedOutputHeight(int height);

    [[nodiscard]] float viewportScalePercent() const;
    void setViewportScalePercent(float percent);

    [[nodiscard]] QString paletteRecoveryName() const;
    void setPaletteRecoveryName(const QString &name);
    void clearPaletteRecoveryName();

    [[nodiscard]] QString sineRecoveryName() const;
    void setSineRecoveryName(const QString &name);
    void clearSineRecoveryName();

    void sync();
    void clearPersistedSettings();

private:
    QSettings _settings;
};
