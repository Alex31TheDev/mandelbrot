#pragma once

#include <QObject>
#include <QSize>
#include <QString>

#include "BackendAPI.h"
#include "GUITypes.h"

class GUISessionState final : public QObject {
    Q_OBJECT

public:
    explicit GUISessionState(QObject *parent = nullptr);

    [[nodiscard]] const GUI::GUIState &state() const { return _state; }
    [[nodiscard]] GUI::GUIState &mutableState() { return _state; }

    [[nodiscard]] const QString &pointRealText() const { return _pointRealText; }
    [[nodiscard]] const QString &pointImagText() const { return _pointImagText; }
    [[nodiscard]] const QString &zoomText() const { return _zoomText; }
    [[nodiscard]] const QString &seedRealText() const { return _seedRealText; }
    [[nodiscard]] const QString &seedImagText() const { return _seedImagText; }

    void setPointRealText(const QString &text);
    void setPointImagText(const QString &text);
    void setZoomText(const QString &text);
    void setSeedRealText(const QString &text);
    void setSeedImagText(const QString &text);

    void setPointTexts(const QString &real, const QString &imag);
    void setSeedTexts(const QString &real, const QString &imag);
    [[nodiscard]] QSize outputSize() const;
    [[nodiscard]] GUI::ViewTextState currentViewTextState() const;
    [[nodiscard]] GUI::GUIRenderSnapshot snapshot() const;

    void syncPointTextFromState();
    void syncSeedTextFromState();
    void syncZoomTextFromState();
    void syncStatePointFromText();
    void syncStateSeedFromText();
    void syncStateZoomFromText();

    void applyHomeView();
    [[nodiscard]] bool isHomeView() const;

    void markSineSavedState();
    void markPaletteSavedState();
    void markPointViewSavedState();
    [[nodiscard]] bool isSineDirty() const;
    [[nodiscard]] bool isPaletteDirty() const;
    [[nodiscard]] bool isPointViewDirty() const;

    [[nodiscard]] QString stateToString(double value, int precision = 17) const;
    [[nodiscard]] GUI::SavedPointViewState capturePointViewState() const;

private:
    GUI::GUIState _state;

    QString _pointRealText = "0";
    QString _pointImagText = "0";
    QString _zoomText;
    QString _seedRealText = "0";
    QString _seedImagText = "0";

    bool _hasSavedSineState = false;
    bool _hasSavedPaletteState = false;
    bool _hasSavedPointViewState = false;
    QString _savedSineName;
    QString _savedPaletteName;
    Backend::SinePaletteConfig _savedSinePalette;
    Backend::PaletteHexConfig _savedPalette;
    GUI::SavedPointViewState _savedPointViewState;
};
