#pragma once

#include <cstdint>
#include <optional>

#include <QMetaType>
#include <QPoint>
#include <QPointF>
#include <QSize>
#include <QString>
#include <QtTypes>

#include "BackendAPI.h"

#include "GUIConstants.h"

#include "options/ColorMethods.h"
#include "options/FractalTypes.h"

namespace GUI {
    using namespace Backend;

    enum class NavMode { realtimeZoom, zoom, pan };

    enum class SelectionTarget { zoomPoint, seedPoint, lightPoint };

    struct ViewTextState {
        QString pointReal = "0";
        QString pointImag = "0";
        QString zoomText = "0";
        QSize outputSize;
        bool valid = false;
    };

    struct ParsedViewState {
        double pointReal = 0.0;
        double pointImag = 0.0;
        double zoom = 0.0;
        QSize outputSize;
        bool valid = false;
    };

    struct PresentedFrame {
        uint64_t stateId = 0;
        ViewTextState view;
        ParsedViewState parsedView;
        qint64 renderMs = 0;
        double renderFPS = 0.0;
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
        int outputWidth = Constants::defaultOutputWidth;
        int outputHeight = Constants::defaultOutputHeight;
        float viewportScalePercent = 100.0f;
        int panRate = 8;
        int zoomRate = 8;
        int interactionTargetFPS = Constants::defaultInteractionTargetFPS;
        double zoom = Constants::homeZoom;
        double exponent = 2.0;
        QPointF point = { 0.0, 0.0 };
        QPointF seed = { 0.0, 0.0 };
        QPointF light = { 1.0, 1.0 };
        LightColor lightColor;
        FractalType fractalType = FractalType::mandelbrot;
        ColorMethod colorMethod = ColorMethod::smooth_iterations;
        QString sineName = "default";
        SinePaletteConfig sinePalette;
        QString paletteName = "default";
        PaletteRGBConfig palette;
    };

    struct PendingPickAction {
        SelectionTarget target = SelectionTarget::zoomPoint;
        QPoint pixel;
    };

    struct SavedPointViewState {
        FractalType fractalType = FractalType::mandelbrot;
        bool inverse = false;
        bool julia = false;
        int iterations = 0;
        QString real = "0";
        QString imag = "0";
        QString zoom = "0";
        QString seedReal = "0";
        QString seedImag = "0";
    };

    struct GUIRenderSnapshot {
        GUIState state;
        QString pointRealText;
        QString pointImagText;
        QString zoomText;
        QString seedRealText;
        QString seedImagText;
    };
}

Q_DECLARE_METATYPE(GUI::ViewTextState)
Q_DECLARE_METATYPE(GUI::PresentedFrame)
