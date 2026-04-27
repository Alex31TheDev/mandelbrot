#pragma once

#include <cstdint>
#include <array>
#include <optional>

#include <QPoint>
#include <QPointF>
#include <QSize>
#include <QString>

#include "BackendAPI.h"

inline constexpr auto kDefaultBackendType = "FloatAVX2";
inline constexpr int kDefaultInteractionTargetFPS = 60;
inline constexpr int kPrecisionInteractionDelayMs = 200;
inline constexpr double kBoostedPanSpeedFactor = 2.0;
inline constexpr std::array<double, 21> kZoomStepTable
    = { 1.000625, 1.00125, 1.0025, 1.005, 1.01, 1.015, 1.02, 1.025, 1.03, 1.04,
          1.05, 1.06, 1.07, 1.08, 1.09, 1.10, 1.11, 1.12, 1.14, 1.17, 1.20 };
inline constexpr int kGridModes[] = { 0, 2, 3, 5, 9 };
inline constexpr int kGridModeCount
    = sizeof(kGridModes) / sizeof(kGridModes[0]);

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
    int outputWidth = 1920;
    int outputHeight = 1080;
    int panRate = 8;
    int zoomRate = 8;
    int interactionTargetFPS = kDefaultInteractionTargetFPS;
    double zoom = -0.65;
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
