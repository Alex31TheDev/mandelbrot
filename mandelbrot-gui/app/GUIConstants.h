#pragma once

#include <array>

namespace GUI::Constants {
inline const char* const defaultBackendType = "FloatAVX2";

inline const int defaultInteractionTargetFPS = 30;
inline const int previewStillMs = 5000;
inline const int precisionInteractionDelayMs = 200;

inline const int defaultOutputWidth = 1920;
inline const int defaultOutputHeight = 1080;
inline const int renderShutdownTimeoutMs = 2000;

inline const double homeZoom = -0.65;
inline const double minimumZoom = -3.2499;
inline const double boostedPanSpeedFactor = 2.0;
inline const double panRateBase = 1.125;

inline constexpr std::array<double, 21> zoomStepTable
    = { 1.000625, 1.00125, 1.0025, 1.005, 1.01, 1.015, 1.02, 1.025, 1.03, 1.04,
          1.05, 1.06, 1.07, 1.08, 1.09, 1.10, 1.11, 1.12, 1.14, 1.17, 1.20 };

inline constexpr std::array<int, 5> gridModes = { 0, 2, 3, 5, 9 };
}
