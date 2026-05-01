#pragma once

#include <cstdint>
#include <optional>

#include <QPoint>
#include <QPointF>
#include <QSize>
#include <QString>

#include "BackendAPI.h"
#include "app/GUIConstants.h"

enum class NavMode { realtimeZoom, zoom, pan };

enum class SelectionTarget { zoomPoint, seedPoint, lightPoint };

struct ViewTextState {
    QString pointReal = "0";
    QString pointImag = "0";
    QString zoomText = "0";
    QSize outputSize;
    bool valid = false;
};

struct GUIState {
    QString backend;
    bool manualBackendSelection = false;
    int iterations = 0;
    int aaPixels = 1;
    bool useThreads = true;
    bool preserveRatio = true;
    bool julia = false;
    bool inverse = false;
    int outputWidth = GUI::Constants::defaultOutputWidth;
    int outputHeight = GUI::Constants::defaultOutputHeight;
    int panRate = 8;
    int zoomRate = 8;
    int interactionTargetFPS = GUI::Constants::defaultInteractionTargetFPS;
    double zoom = GUI::Constants::homeZoom;
    double exponent = 2.0;
    QPointF point = { 0.0, 0.0 };
    QPointF seed = { 0.0, 0.0 };
    QPointF light = { 1.0, 1.0 };
    Backend::LightColor lightColor;
    Backend::FractalType fractalType = Backend::FractalType::mandelbrot;
    Backend::ColorMethod colorMethod = Backend::ColorMethod::smooth_iterations;
    QString sineName = "default";
    Backend::SinePaletteConfig sinePalette;
    QString paletteName = "default";
    Backend::PaletteHexConfig palette;
};

struct PendingPickAction {
    SelectionTarget target = SelectionTarget::zoomPoint;
    QPoint pixel;
};

struct SavedPointViewState {
    Backend::FractalType fractalType = Backend::FractalType::mandelbrot;
    bool inverse = false;
    bool julia = false;
    int iterations = 0;
    QString real = "0";
    QString imag = "0";
    QString zoom = "0";
    QString seedReal = "0";
    QString seedImag = "0";
};

struct RenderRequest {
    GUIState state;
    QString pointRealText;
    QString pointImagText;
    QString zoomText;
    QString seedRealText;
    QString seedImagText;
    std::optional<PendingPickAction> pickAction;
    uint64_t id = 0;
};

struct GUIRenderSnapshot {
    GUIState state;
    QString pointRealText;
    QString pointImagText;
    QString zoomText;
    QString seedRealText;
    QString seedImagText;
};
