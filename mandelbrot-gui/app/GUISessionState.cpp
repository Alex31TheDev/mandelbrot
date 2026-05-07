#include "GUISessionState.h"

#include <algorithm>

#include "BackendAPI.h"
using namespace Backend;

#include "services/PaletteStore.h"
#include "services/SineStore.h"

#include "util/GUIUtil.h"
#include "util/NumberUtil.h"

using namespace GUI;

GUISessionState::GUISessionState(QObject *parent)
    : QObject(parent) {
    syncZoomTextFromState();
    syncPointTextFromState();
    syncSeedTextFromState();
}

void GUISessionState::setPointRealText(const QString &text) {
    _pointRealText = text;
}

void GUISessionState::setPointImagText(const QString &text) {
    _pointImagText = text;
}

void GUISessionState::setZoomText(const QString &text) {
    _zoomText = text;
}

void GUISessionState::setSeedRealText(const QString &text) {
    _seedRealText = text;
}

void GUISessionState::setSeedImagText(const QString &text) {
    _seedImagText = text;
}

void GUISessionState::setPointTexts(const QString &real, const QString &imag) {
    _pointRealText = real;
    _pointImagText = imag;
}

void GUISessionState::setSeedTexts(const QString &real, const QString &imag) {
    _seedRealText = real;
    _seedImagText = imag;
}

QSize GUISessionState::outputSize() const {
    return { _state.outputWidth, _state.outputHeight };
}

ViewTextState GUISessionState::currentViewTextState() const {
    const QSize size = outputSize();
    return { .pointReal = _pointRealText,
        .pointImag = _pointImagText,
        .zoomText = _zoomText,
        .outputSize = size,
        .valid = size.width() > 0 && size.height() > 0 };
}

GUIRenderSnapshot GUISessionState::snapshot() const {
    return { .state = _state,
        .pointRealText = _pointRealText,
        .pointImagText = _pointImagText,
        .zoomText = _zoomText,
        .seedRealText = _seedRealText,
        .seedImagText = _seedImagText };
}

void GUISessionState::syncPointTextFromState() {
    _pointRealText = stateToString(_state.point.x());
    _pointImagText = stateToString(_state.point.y());
}

void GUISessionState::syncSeedTextFromState() {
    _seedRealText = stateToString(_state.seed.x(), 17);
    _seedImagText = stateToString(_state.seed.y(), 17);
}

void GUISessionState::syncZoomTextFromState() {
    _zoomText = stateToString(_state.zoom, 17);
}

void GUISessionState::syncStatePointFromText() {
    bool okReal = false;
    bool okImag = false;
    const double real = _pointRealText.toDouble(&okReal);
    const double imag = _pointImagText.toDouble(&okImag);
    if (okReal) _state.point.setX(real);
    if (okImag) _state.point.setY(imag);
}

void GUISessionState::syncStateSeedFromText() {
    bool okReal = false;
    bool okImag = false;
    const double real = _seedRealText.toDouble(&okReal);
    const double imag = _seedImagText.toDouble(&okImag);
    if (okReal) _state.seed.setX(real);
    if (okImag) _state.seed.setY(imag);
}

void GUISessionState::syncStateZoomFromText() {
    bool ok = false;
    const double zoom = _zoomText.toDouble(&ok);
    if (!ok) return;

    _state.zoom = Util::clampGUIZoom(zoom);
    if (!NumberUtil::almostEqual(_state.zoom, zoom)) {
        _zoomText = stateToString(_state.zoom, 17);
    }
}

void GUISessionState::applyHomeView() {
    _state.iterations = 0;
    _state.zoom = Constants::homeZoom;
    _state.point = QPointF(0.0, 0.0);
    _state.seed = QPointF(0.0, 0.0);
    _state.light = QPointF(1.0, 1.0);
    syncZoomTextFromState();
    syncPointTextFromState();
    syncSeedTextFromState();
}

bool GUISessionState::isHomeView() const {
    const SavedPointViewState current = capturePointViewState();
    return current.fractalType == FractalType::mandelbrot
        && !current.inverse && !current.julia && current.iterations == 0
        && NumberUtil::equalParsedDoubleText(current.real.toStdString(), "0")
        && NumberUtil::equalParsedDoubleText(current.imag.toStdString(), "0")
        && NumberUtil::equalParsedDoubleText(current.zoom.toStdString(),
            stateToString(Constants::homeZoom, 17).toStdString())
        && NumberUtil::equalParsedDoubleText(current.seedReal.toStdString(), "0")
        && NumberUtil::equalParsedDoubleText(current.seedImag.toStdString(), "0");
}

void GUISessionState::markSineSavedState() {
    _savedSineName = PaletteStore::normalizeName(_state.sineName);
    _savedSinePalette = _state.sinePalette;
    _hasSavedSineState = true;
}

void GUISessionState::markPaletteSavedState() {
    _savedPaletteName = PaletteStore::normalizeName(_state.paletteName);
    _savedPalette = _state.palette;
    _hasSavedPaletteState = true;
}

void GUISessionState::markPointViewSavedState() {
    _savedPointViewState = capturePointViewState();
    _hasSavedPointViewState = true;
}

bool GUISessionState::isSineDirty() const {
    if (!_hasSavedSineState) return true;
    if (PaletteStore::normalizeName(_state.sineName)
        .compare(_savedSineName, Qt::CaseInsensitive)
        != 0) {
        return true;
    }
    return !SineStore::sameConfig(_state.sinePalette, _savedSinePalette);
}

bool GUISessionState::isPaletteDirty() const {
    if (!_hasSavedPaletteState) return true;
    if (PaletteStore::normalizeName(_state.paletteName)
        .compare(_savedPaletteName, Qt::CaseInsensitive)
        != 0) {
        return true;
    }
    return !PaletteStore::sameConfig(_state.palette, _savedPalette);
}

bool GUISessionState::isPointViewDirty() const {
    if (!_hasSavedPointViewState) return true;

    const SavedPointViewState current = capturePointViewState();
    return current.fractalType != _savedPointViewState.fractalType
        || current.inverse != _savedPointViewState.inverse
        || current.julia != _savedPointViewState.julia
        || current.iterations != _savedPointViewState.iterations
        || !NumberUtil::equalParsedDoubleText(current.real.toStdString(),
            _savedPointViewState.real.toStdString())
        || !NumberUtil::equalParsedDoubleText(current.imag.toStdString(),
            _savedPointViewState.imag.toStdString())
        || !NumberUtil::equalParsedDoubleText(current.zoom.toStdString(),
            _savedPointViewState.zoom.toStdString())
        || !NumberUtil::equalParsedDoubleText(current.seedReal.toStdString(),
            _savedPointViewState.seedReal.toStdString())
        || !NumberUtil::equalParsedDoubleText(current.seedImag.toStdString(),
            _savedPointViewState.seedImag.toStdString());
}

QString GUISessionState::stateToString(double value, int precision) const {
    return QString::number(value, 'g', precision);
}

SavedPointViewState GUISessionState::capturePointViewState() const {
    return SavedPointViewState{ .fractalType = _state.fractalType,
        .inverse = _state.inverse,
        .julia = _state.julia,
        .iterations = _state.iterations,
        .real = _pointRealText,
        .imag = _pointImagText,
        .zoom = _zoomText,
        .seedReal = _seedRealText,
        .seedImag = _seedImagText };
}