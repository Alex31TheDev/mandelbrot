#pragma once

#include <QObject>
#include <QSize>
#include <QString>

#include "app/GUITypes.h"

class GUISessionState final : public QObject {
    Q_OBJECT

public:
    explicit GUISessionState(QObject* parent = nullptr);

    [[nodiscard]] const GUIState& state() const { return _state; }
    [[nodiscard]] GUIState& mutableState() { return _state; }

    [[nodiscard]] const QString& pointRealText() const { return _pointRealText; }
    [[nodiscard]] const QString& pointImagText() const { return _pointImagText; }
    [[nodiscard]] const QString& zoomText() const { return _zoomText; }
    [[nodiscard]] const QString& seedRealText() const { return _seedRealText; }
    [[nodiscard]] const QString& seedImagText() const { return _seedImagText; }

    void setPointRealText(const QString& text);
    void setPointImagText(const QString& text);
    void setZoomText(const QString& text);
    void setSeedRealText(const QString& text);
    void setSeedImagText(const QString& text);

    void setPointTexts(const QString& real, const QString& imag);
    void setSeedTexts(const QString& real, const QString& imag);
    void setDisplayedViewState(const QString& pointReal, const QString& pointImag,
        const QString& zoomText, const QSize& outputSize);
    void clearDisplayedViewState();

    [[nodiscard]] bool hasDisplayedViewState() const {
        return _hasDisplayedViewState;
    }
    [[nodiscard]] QSize outputSize() const;
    [[nodiscard]] ViewTextState currentViewTextState() const;
    [[nodiscard]] ViewTextState displayedViewTextState() const;
    [[nodiscard]] GUIRenderSnapshot snapshot() const;

    void syncPointTextFromState();
    void syncSeedTextFromState();
    void syncZoomTextFromState();
    void syncStatePointFromText();
    void syncStateSeedFromText();
    void syncStateZoomFromText();
    void applyHomeView();

    void markSineSavedState();
    void markPaletteSavedState();
    void markPointViewSavedState();
    [[nodiscard]] bool isSineDirty() const;
    [[nodiscard]] bool isPaletteDirty() const;
    [[nodiscard]] bool isPointViewDirty() const;

    [[nodiscard]] QString stateToString(double value, int precision = 17) const;
    [[nodiscard]] SavedPointViewState capturePointViewState() const;

private:
    GUIState _state;
    QString _pointRealText = "0";
    QString _pointImagText = "0";
    QString _zoomText;
    QString _seedRealText = "0";
    QString _seedImagText = "0";

    QString _displayedPointRealText = "0";
    QString _displayedPointImagText = "0";
    QString _displayedZoomText;
    QSize _displayedOutputSize {
        GUI::Constants::defaultOutputWidth, GUI::Constants::defaultOutputHeight
    };
    bool _hasDisplayedViewState = false;

    bool _hasSavedSineState = false;
    bool _hasSavedPaletteState = false;
    bool _hasSavedPointViewState = false;
    QString _savedSineName;
    QString _savedPaletteName;
    Backend::SinePaletteConfig _savedSinePalette;
    Backend::PaletteHexConfig _savedPalette;
    SavedPointViewState _savedPointViewState;
};
