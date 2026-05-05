#include "D3DWidget.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <string>
#include <string_view>

#include "util/IncludeWin32.h"

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#include <d3d11.h>
#include <d3d11_4.h>
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <QDebug>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QShowEvent>

#include "util/NumberUtil.h"

#include "windows/viewport/ViewportHost.h"

using namespace GUI;
using Microsoft::WRL::ComPtr;

static constexpr int hudDigitWidth = 3;
static constexpr int hudDigitHeight = 5;
static constexpr int hudDigitStride = 4;
static constexpr int hudAtlasWidth = 10 * hudDigitStride;
static constexpr int hudAtlasHeight = hudDigitHeight;

static const char *vertexShaderSource = R"(
cbuffer TransformBuffer : register(b0) {
    float2 center;
    float2 translation;
    float2 scale;
    float2 viewportSize;
};

struct VSInput {
    float2 position : POSITION;
    float2 texcoord : TEXCOORD0;
};

struct PSInput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

PSInput main(VSInput input) {
    PSInput output;

    float2 pixel;
    pixel.x = (input.position.x * 0.5 + 0.5) * viewportSize.x;
    pixel.y = (1.0 - (input.position.y * 0.5 + 0.5)) * viewportSize.y;
    pixel = translation + center + (pixel - center) * scale;

    float2 ndc;
    ndc.x = (pixel.x / max(viewportSize.x, 1.0)) * 2.0 - 1.0;
    ndc.y = 1.0 - (pixel.y / max(viewportSize.y, 1.0)) * 2.0;

    output.position = float4(ndc, 0.0, 1.0);
    output.texcoord = input.texcoord;
    return output;
}
)";

static const char *framePixelShaderSource = R"(
Texture2D frameTexture : register(t0);
SamplerState frameSampler : register(s0);

float4 main(float4 position : SV_POSITION, float2 texcoord : TEXCOORD0)
    : SV_TARGET {
    return frameTexture.Sample(frameSampler, texcoord);
}
)";

static const char *hudPixelShaderSource = R"(
Texture2D hudTexture : register(t0);
SamplerState hudSampler : register(s0);

float4 main(float4 position : SV_POSITION, float2 texcoord : TEXCOORD0)
    : SV_TARGET {
    const float alpha = hudTexture.Sample(hudSampler, texcoord).a;
    clip(alpha - 0.5);
    return float4(0.0, 0.0, 0.0, 1.0);
}
)";

static const std::array<std::array<std::string_view, hudDigitHeight>, 10>
    hudDigitPatterns{ {
        { "111", "101", "101", "101", "111" },
        { "010", "110", "010", "010", "111" },
        { "111", "001", "111", "100", "111" },
        { "111", "001", "111", "001", "111" },
        { "101", "101", "111", "001", "001" },
        { "111", "100", "111", "001", "111" },
        { "111", "100", "111", "101", "111" },
        { "111", "001", "001", "001", "001" },
        { "111", "101", "111", "101", "111" },
        { "111", "101", "111", "001", "111" }
    } };

static bool hrSucceeded(HRESULT hr) {
    return SUCCEEDED(hr);
}

static QPoint mapToOutputDelta(const QSize &logicalSize, const QSize &outputSize,
    const QPoint &logicalDelta) {
    if (logicalSize.width() <= 0 || logicalSize.height() <= 0) {
        return {};
    }

    return { static_cast<int>(std::lround(static_cast<double>(logicalDelta.x())
                 * outputSize.width() / std::max(1, logicalSize.width()))),
        static_cast<int>(std::lround(static_cast<double>(logicalDelta.y())
            * outputSize.height() / std::max(1, logicalSize.height()))) };
}

static QPoint mapToOutputPixel(const QSize &logicalSize, const QSize &outputSize,
    const QPoint &logicalPoint) {
    if (logicalSize.width() <= 0 || logicalSize.height() <= 0) {
        return {};
    }

    const auto mapAxis = [](double logical, int logicalExtent, int outputExtent) {
        if (outputExtent <= 1 || logicalExtent <= 1) {
            return 0.0;
        }

        return logical * static_cast<double>(outputExtent - 1)
            / static_cast<double>(logicalExtent - 1);
    };

    const double x = mapAxis(logicalPoint.x(), logicalSize.width(),
        outputSize.width());
    const double y = mapAxis(logicalPoint.y(), logicalSize.height(),
        outputSize.height());

    return { std::clamp(static_cast<int>(std::lround(x)), 0,
                 std::max(0, outputSize.width() - 1)),
        std::clamp(static_cast<int>(std::lround(y)), 0,
            std::max(0, outputSize.height() - 1)) };
}

static D3DWidget::Vertex makeVertex(float x, float y, float u, float v) {
    return D3DWidget::Vertex{ { x, y }, { u, v } };
}

static float pixelToNdcX(float pixelX, int width) {
    return static_cast<float>((pixelX / static_cast<float>(std::max(1, width)))
        * 2.0f - 1.0f);
}

static float pixelToNdcY(float pixelY, int height) {
    return static_cast<float>(1.0f
        - (pixelY / static_cast<float>(std::max(1, height))) * 2.0f);
}

D3DWidget::D3DWidget(QWidget *parent)
    : D3DWidget(nullptr, parent) {}

D3DWidget::D3DWidget(ViewportHost *host, QWidget *parent)
    : QWidget(parent)
    , _host(host) {
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAutoFillBackground(false);
    setFocusPolicy(Qt::NoFocus);

    _presentTimer.setTimerType(Qt::PreciseTimer);
    _presentTimer.setInterval(0);
    QObject::connect(&_presentTimer, &QTimer::timeout, this, [this]() {
        _render();
    });
    _presentTimer.start();
}

D3DWidget::~D3DWidget() = default;

void D3DWidget::setHost(ViewportHost *host) {
    if (_host == host) {
        return;
    }

    _host = host;
    refreshPreviewTransform();
    if (_host && _frameTextureReady) {
        _host->setPresentationSurface(_presentationSurface());
    }
}

void D3DWidget::setPresentationSize(const QSize &size) {
    const QSize nextSize = size.isValid() ? size : QSize();
    if (_presentationSize == nextSize) {
        return;
    }

    _presentationSize = nextSize;
    if (_frameTextureReady && _frameTextureSize != _presentationSize) {
        _frameTextureReady = false;
        _frameTextureSize = {};
        _frameTexture.Reset();
        _frameTextureSRV.Reset();
        if (_host) {
            _host->setPresentationSurface({});
        }
    }
    refreshPreviewTransform();
}

void D3DWidget::presentFrame(const PresentedFrame &frame) {
    if (_displayedFrame.valid && frame.valid
        && frame.stateId < _displayedFrame.stateId) {
        return;
    }

    _displayedFrame = frame;
    refreshPreviewTransform();
}

void D3DWidget::clearFrame() {
    _displayedFrame = {};
    _cachedPreviewTransform.reset();
}

void D3DWidget::refreshPreviewTransform() {
    _cachedPreviewTransform = _calculatePreviewTransform();
}

void D3DWidget::setPanPreviewOffset(const QPoint &logicalOffset) {
    if (_panPreviewOffset == logicalOffset) {
        return;
    }

    _panPreviewOffset = logicalOffset;
    refreshPreviewTransform();
}

void D3DWidget::setZoomPreviewScale(double scale) {
    if (NumberUtil::almostEqual(_zoomPreviewScale, scale)) {
        return;
    }

    _zoomPreviewScale = scale;
    refreshPreviewTransform();
}

QPaintEngine *D3DWidget::paintEngine() const {
    return nullptr;
}

void D3DWidget::paintEvent(QPaintEvent *event) {
    if (event) {
        event->accept();
    }
}

void D3DWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    refreshPreviewTransform();
    _resetPresentationResources();
    if (_host) {
        _host->setPresentationSurface({});
    }
}

void D3DWidget::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    if (!_presentTimer.isActive()) {
        _presentTimer.start();
    }
}

bool D3DWidget::_ensureD3D() {
    if (_d3dReady) {
        return true;
    }

    return _createDeviceResources();
}

bool D3DWidget::_ensureSwapChain() {
    if (!_ensureD3D()) {
        return false;
    }
    if (_swapChainReady) {
        return true;
    }

    return _createSwapChainResources();
}

bool D3DWidget::_ensureFrameTexture(const QSize &textureSize) {
    if (!textureSize.isValid()) {
        return false;
    }
    if (_frameTextureReady && _frameTextureSize == textureSize) {
        return true;
    }

    return _createFrameTextureResources(textureSize);
}

bool D3DWidget::_createDeviceResources() {
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    static const std::array<D3D_FEATURE_LEVEL, 2> featureLevels{
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0
    };

    D3D_FEATURE_LEVEL createdLevel = D3D_FEATURE_LEVEL_11_0;
    HRESULT deviceHr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE,
        nullptr, flags, featureLevels.data(),
        static_cast<UINT>(featureLevels.size()), D3D11_SDK_VERSION,
        _device.GetAddressOf(), &createdLevel, _deviceContext.GetAddressOf());
    if (!hrSucceeded(deviceHr) && (flags & D3D11_CREATE_DEVICE_DEBUG) != 0) {
        flags &= ~D3D11_CREATE_DEVICE_DEBUG;
        deviceHr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE,
            nullptr, flags, featureLevels.data(),
            static_cast<UINT>(featureLevels.size()), D3D11_SDK_VERSION,
            _device.ReleaseAndGetAddressOf(), &createdLevel,
            _deviceContext.ReleaseAndGetAddressOf());
    }
    if (!hrSucceeded(deviceHr)) {
        qWarning() << "Failed to create D3D11 device:" << Qt::hex << deviceHr;
        return false;
    }

    (void)createdLevel;

    ComPtr<ID3DBlob> vertexBlob;
    ComPtr<ID3DBlob> framePixelBlob;
    ComPtr<ID3DBlob> hudPixelBlob;
    ComPtr<ID3DBlob> errorBlob;
    const HRESULT vertexCompileHr = D3DCompile(vertexShaderSource,
        std::strlen(vertexShaderSource), nullptr, nullptr, nullptr, "main",
        "vs_4_0", 0, 0, vertexBlob.GetAddressOf(), errorBlob.GetAddressOf());
    if (!hrSucceeded(vertexCompileHr)) {
        qWarning() << "Failed to compile viewport vertex shader:" << Qt::hex
            << vertexCompileHr;
        return false;
    }

    errorBlob.Reset();
    const HRESULT framePixelCompileHr = D3DCompile(framePixelShaderSource,
        std::strlen(framePixelShaderSource), nullptr, nullptr, nullptr, "main",
        "ps_4_0", 0, 0, framePixelBlob.GetAddressOf(),
        errorBlob.GetAddressOf());
    if (!hrSucceeded(framePixelCompileHr)) {
        qWarning() << "Failed to compile viewport pixel shader:" << Qt::hex
            << framePixelCompileHr;
        return false;
    }

    errorBlob.Reset();
    const HRESULT hudPixelCompileHr = D3DCompile(hudPixelShaderSource,
        std::strlen(hudPixelShaderSource), nullptr, nullptr, nullptr, "main",
        "ps_4_0", 0, 0, hudPixelBlob.GetAddressOf(),
        errorBlob.GetAddressOf());
    if (!hrSucceeded(hudPixelCompileHr)) {
        qWarning() << "Failed to compile HUD pixel shader:" << Qt::hex
            << hudPixelCompileHr;
        return false;
    }

    const HRESULT vertexShaderHr = _device->CreateVertexShader(
        vertexBlob->GetBufferPointer(), vertexBlob->GetBufferSize(), nullptr,
        _vertexShader.GetAddressOf());
    if (!hrSucceeded(vertexShaderHr)) {
        qWarning() << "Failed to create vertex shader:" << Qt::hex
            << vertexShaderHr;
        return false;
    }

    const HRESULT framePixelShaderHr = _device->CreatePixelShader(
        framePixelBlob->GetBufferPointer(), framePixelBlob->GetBufferSize(),
        nullptr, _framePixelShader.GetAddressOf());
    if (!hrSucceeded(framePixelShaderHr)) {
        qWarning() << "Failed to create frame pixel shader:" << Qt::hex
            << framePixelShaderHr;
        return false;
    }

    const HRESULT hudPixelShaderHr = _device->CreatePixelShader(
        hudPixelBlob->GetBufferPointer(), hudPixelBlob->GetBufferSize(),
        nullptr, _hudPixelShader.GetAddressOf());
    if (!hrSucceeded(hudPixelShaderHr)) {
        qWarning() << "Failed to create HUD pixel shader:" << Qt::hex
            << hudPixelShaderHr;
        return false;
    }

    static const std::array<D3D11_INPUT_ELEMENT_DESC, 2> inputLayout{
        D3D11_INPUT_ELEMENT_DESC{
            "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
            D3D11_INPUT_PER_VERTEX_DATA, 0
        },
        D3D11_INPUT_ELEMENT_DESC{
            "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8,
            D3D11_INPUT_PER_VERTEX_DATA, 0
        }
    };
    const HRESULT layoutHr = _device->CreateInputLayout(inputLayout.data(),
        static_cast<UINT>(inputLayout.size()), vertexBlob->GetBufferPointer(),
        vertexBlob->GetBufferSize(), _inputLayout.GetAddressOf());
    if (!hrSucceeded(layoutHr)) {
        qWarning() << "Failed to create input layout:" << Qt::hex << layoutHr;
        return false;
    }

    const std::array<Vertex, 6> quadVertices{
        makeVertex(-1.0f, 1.0f, 0.0f, 0.0f),
        makeVertex(1.0f, 1.0f, 1.0f, 0.0f),
        makeVertex(-1.0f, -1.0f, 0.0f, 1.0f),
        makeVertex(-1.0f, -1.0f, 0.0f, 1.0f),
        makeVertex(1.0f, 1.0f, 1.0f, 0.0f),
        makeVertex(1.0f, -1.0f, 1.0f, 1.0f)
    };
    D3D11_SUBRESOURCE_DATA quadData{};
    quadData.pSysMem = quadVertices.data();
    D3D11_BUFFER_DESC quadBufferDesc{};
    quadBufferDesc.ByteWidth = sizeof(Vertex) * static_cast<UINT>(quadVertices.size());
    quadBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
    quadBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    const HRESULT quadBufferHr = _device->CreateBuffer(&quadBufferDesc,
        &quadData, _quadVertexBuffer.GetAddressOf());
    if (!hrSucceeded(quadBufferHr)) {
        qWarning() << "Failed to create quad vertex buffer:" << Qt::hex
            << quadBufferHr;
        return false;
    }

    D3D11_BUFFER_DESC hudBufferDesc{};
    hudBufferDesc.ByteWidth = sizeof(Vertex)
        * static_cast<UINT>(HUDVertexCapacity);
    hudBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    hudBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    hudBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    const HRESULT hudBufferHr = _device->CreateBuffer(&hudBufferDesc, nullptr,
        _hudVertexBuffer.GetAddressOf());
    if (!hrSucceeded(hudBufferHr)) {
        qWarning() << "Failed to create HUD vertex buffer:" << Qt::hex
            << hudBufferHr;
        return false;
    }

    D3D11_BUFFER_DESC constantBufferDesc{};
    constantBufferDesc.ByteWidth = sizeof(TransformConstants);
    constantBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    const HRESULT constantBufferHr = _device->CreateBuffer(&constantBufferDesc,
        nullptr, _transformConstantBuffer.GetAddressOf());
    if (!hrSucceeded(constantBufferHr)) {
        qWarning() << "Failed to create transform constant buffer:" << Qt::hex
            << constantBufferHr;
        return false;
    }

    D3D11_SAMPLER_DESC frameSamplerDesc{};
    frameSamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    frameSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    frameSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    frameSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    frameSamplerDesc.MinLOD = 0.0f;
    frameSamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    const HRESULT frameSamplerHr = _device->CreateSamplerState(&frameSamplerDesc,
        _frameSamplerState.GetAddressOf());
    if (!hrSucceeded(frameSamplerHr)) {
        qWarning() << "Failed to create frame sampler state:" << Qt::hex
            << frameSamplerHr;
        return false;
    }

    D3D11_SAMPLER_DESC hudSamplerDesc = frameSamplerDesc;
    hudSamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    const HRESULT hudSamplerHr = _device->CreateSamplerState(&hudSamplerDesc,
        _hudSamplerState.GetAddressOf());
    if (!hrSucceeded(hudSamplerHr)) {
        qWarning() << "Failed to create HUD sampler state:" << Qt::hex
            << hudSamplerHr;
        return false;
    }

    std::vector<uint8_t> atlasPixels(
        static_cast<size_t>(hudAtlasWidth * hudAtlasHeight * 4), 0);
    for (size_t digit = 0; digit < hudDigitPatterns.size(); digit++) {
        const int baseX = static_cast<int>(digit) * hudDigitStride;
        for (int y = 0; y < hudDigitHeight; y++) {
            for (int x = 0; x < hudDigitWidth; x++) {
                if (hudDigitPatterns[digit][static_cast<size_t>(y)][
                    static_cast<size_t>(x)] != '1') {
                    continue;
                }

                const size_t index = static_cast<size_t>(
                    (y * hudAtlasWidth + (baseX + x)) * 4
                );
                atlasPixels[index] = 255;
                atlasPixels[index + 1] = 255;
                atlasPixels[index + 2] = 255;
                atlasPixels[index + 3] = 255;
            }
        }
    }

    D3D11_TEXTURE2D_DESC hudTextureDesc{};
    hudTextureDesc.Width = hudAtlasWidth;
    hudTextureDesc.Height = hudAtlasHeight;
    hudTextureDesc.MipLevels = 1;
    hudTextureDesc.ArraySize = 1;
    hudTextureDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    hudTextureDesc.SampleDesc.Count = 1;
    hudTextureDesc.Usage = D3D11_USAGE_IMMUTABLE;
    hudTextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA hudTextureData{};
    hudTextureData.pSysMem = atlasPixels.data();
    hudTextureData.SysMemPitch = hudAtlasWidth * 4;
    const HRESULT hudTextureHr = _device->CreateTexture2D(&hudTextureDesc,
        &hudTextureData, _hudTexture.GetAddressOf());
    if (!hrSucceeded(hudTextureHr)) {
        qWarning() << "Failed to create HUD texture:" << Qt::hex
            << hudTextureHr;
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC hudSRVDesc{};
    hudSRVDesc.Format = hudTextureDesc.Format;
    hudSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    hudSRVDesc.Texture2D.MipLevels = 1;
    const HRESULT hudSRVHr = _device->CreateShaderResourceView(_hudTexture.Get(),
        &hudSRVDesc, _hudTextureSRV.GetAddressOf());
    if (!hrSucceeded(hudSRVHr)) {
        qWarning() << "Failed to create HUD texture SRV:" << Qt::hex
            << hudSRVHr;
        return false;
    }

    ComPtr<ID3D11Multithread> multithread;
    if (hrSucceeded(_deviceContext.As(&multithread)) && multithread) {
        multithread->SetMultithreadProtected(TRUE);
    }

    _presentFPSWindowStartedAt = std::chrono::steady_clock::now();
    _presentFrameCounter = 0;
    _presentFPS = 0.0;
    _d3dReady = true;
    return true;
}

bool D3DWidget::_createSwapChainResources() {
    const QSize targetSize = _swapChainSize();
    if (!targetSize.isValid()) {
        return false;
    }
    _swapChainReady = false;

    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (!hwnd) {
        return false;
    }

    if (_deviceContext) {
        ID3D11RenderTargetView *nullRTV = nullptr;
        _deviceContext->OMSetRenderTargets(1, &nullRTV, nullptr);
        _deviceContext->Flush();
    }
    _renderTargetView.Reset();

    if (!_swapChain) {
        ComPtr<IDXGIDevice> dxgiDevice;
        ComPtr<IDXGIAdapter> adapter;
        ComPtr<IDXGIFactory2> factory;
        if (!hrSucceeded(_device.As(&dxgiDevice))
            || !hrSucceeded(dxgiDevice->GetAdapter(adapter.GetAddressOf()))
            || !hrSucceeded(adapter->GetParent(
                IID_PPV_ARGS(factory.GetAddressOf())
            ))) {
            qWarning() << "Failed to resolve DXGI factory for swap chain creation.";
            return false;
        }

        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.Width = static_cast<UINT>(targetSize.width());
        desc.Height = static_cast<UINT>(targetSize.height());
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.Stereo = FALSE;
        desc.SampleDesc.Count = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 2;
        desc.Scaling = DXGI_SCALING_STRETCH;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

        const HRESULT swapChainHr = factory->CreateSwapChainForHwnd(_device.Get(),
            hwnd, &desc, nullptr, nullptr, _swapChain.GetAddressOf());
        if (!hrSucceeded(swapChainHr)) {
            qWarning() << "Failed to create swap chain:" << Qt::hex
                << swapChainHr;
            return false;
        }

        factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
    } else {
        _deviceContext->ClearState();
        _deviceContext->Flush();
        const HRESULT resizeHr = _swapChain->ResizeBuffers(0,
            static_cast<UINT>(targetSize.width()),
            static_cast<UINT>(targetSize.height()), DXGI_FORMAT_UNKNOWN, 0);
        if (!hrSucceeded(resizeHr)) {
            qWarning() << "Failed to resize swap chain:" << Qt::hex << resizeHr;
            return false;
        }
    }

    ComPtr<ID3D11Texture2D> backBuffer;
    const HRESULT bufferHr = _swapChain->GetBuffer(0,
        IID_PPV_ARGS(backBuffer.GetAddressOf()));
    if (!hrSucceeded(bufferHr)) {
        qWarning() << "Failed to acquire swap chain back buffer:" << Qt::hex
            << bufferHr;
        return false;
    }

    const HRESULT rtvHr = _device->CreateRenderTargetView(backBuffer.Get(),
        nullptr, _renderTargetView.GetAddressOf());
    if (!hrSucceeded(rtvHr)) {
        qWarning() << "Failed to create render target view:" << Qt::hex << rtvHr;
        return false;
    }

    _swapChainReady = true;
    return true;
}

bool D3DWidget::_createFrameTextureResources(const QSize &textureSize) {
    if (!_ensureD3D()) {
        return false;
    }

    _frameTextureReady = false;
    _frameTexture.Reset();
    _frameTextureSRV.Reset();

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = static_cast<UINT>(std::max(1, textureSize.width()));
    desc.Height = static_cast<UINT>(std::max(1, textureSize.height()));
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    const HRESULT textureHr = _device->CreateTexture2D(&desc, nullptr,
        _frameTexture.GetAddressOf());
    if (!hrSucceeded(textureHr)) {
        qWarning() << "Failed to create frame texture:" << Qt::hex << textureHr;
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    const HRESULT srvHr = _device->CreateShaderResourceView(_frameTexture.Get(),
        &srvDesc, _frameTextureSRV.GetAddressOf());
    if (!hrSucceeded(srvHr)) {
        qWarning() << "Failed to create frame texture SRV:" << Qt::hex << srvHr;
        return false;
    }

    _frameTextureSize = textureSize;
    _frameTextureReady = true;
    if (_host) {
        _host->setPresentationSurface(_presentationSurface());
    }
    return true;
}

void D3DWidget::_render() {
    if (!isVisible() || width() <= 0 || height() <= 0) {
        return;
    }
    if (!_ensureSwapChain()) {
        return;
    }

    if (_presentationSize.isValid()) {
        (void)_ensureFrameTexture(_presentationSize);
    }

    static const std::array<float, 4> clearColor{ 0.0f, 0.0f, 0.0f, 1.0f };
    _deviceContext->OMSetRenderTargets(1, _renderTargetView.GetAddressOf(), nullptr);
    _deviceContext->ClearRenderTargetView(_renderTargetView.Get(),
        clearColor.data());

    _renderFrameTexture();
    _renderPresentFPS();
    const HRESULT presentHr = _swapChain->Present(0, DXGI_PRESENT_DO_NOT_WAIT);
    if (presentHr == S_OK || presentHr == DXGI_STATUS_OCCLUDED) {
        _updatePresentFPS();
    }
}

void D3DWidget::_renderFrameTexture() {
    if (!_frameTextureReady || !_frameTextureSRV || !_quadVertexBuffer) {
        return;
    }

    const QSize swapChainSize = _swapChainSize();
    D3D11_VIEWPORT viewport{};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(swapChainSize.width());
    viewport.Height = static_cast<float>(swapChainSize.height());
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    _deviceContext->RSSetViewports(1, &viewport);

    if (!_updateTransformBuffer(_cachedPreviewTransform)) {
        return;
    }

    const UINT stride = sizeof(Vertex);
    const UINT offset = 0;
    _deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    _deviceContext->IASetInputLayout(_inputLayout.Get());
    _deviceContext->IASetVertexBuffers(0, 1, _quadVertexBuffer.GetAddressOf(),
        &stride, &offset);
    _deviceContext->VSSetShader(_vertexShader.Get(), nullptr, 0);
    _deviceContext->VSSetConstantBuffers(0, 1,
        _transformConstantBuffer.GetAddressOf());
    _deviceContext->PSSetShader(_framePixelShader.Get(), nullptr, 0);
    _deviceContext->PSSetShaderResources(0, 1, _frameTextureSRV.GetAddressOf());
    _deviceContext->PSSetSamplers(0, 1, _frameSamplerState.GetAddressOf());
    _deviceContext->Draw(6, 0);

    ID3D11ShaderResourceView *nullSRV = nullptr;
    _deviceContext->PSSetShaderResources(0, 1, &nullSRV);
}

void D3DWidget::_renderPresentFPS() {
    if (!_hudTextureSRV || !_hudVertexBuffer) {
        return;
    }

    std::vector<Vertex> vertices;
    _buildHUDVertices(vertices);
    if (vertices.empty()) {
        return;
    }

    D3D11_MAPPED_SUBRESOURCE mapped{};
    const HRESULT mapHr = _deviceContext->Map(_hudVertexBuffer.Get(), 0,
        D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (!hrSucceeded(mapHr)) {
        qWarning() << "Failed to map HUD vertex buffer:" << Qt::hex << mapHr;
        return;
    }
    std::memcpy(mapped.pData, vertices.data(), sizeof(Vertex) * vertices.size());
    _deviceContext->Unmap(_hudVertexBuffer.Get(), 0);

    if (!_updateTransformBuffer(std::nullopt)) {
        return;
    }

    const UINT stride = sizeof(Vertex);
    const UINT offset = 0;
    _deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    _deviceContext->IASetInputLayout(_inputLayout.Get());
    _deviceContext->IASetVertexBuffers(0, 1, _hudVertexBuffer.GetAddressOf(),
        &stride, &offset);
    _deviceContext->VSSetShader(_vertexShader.Get(), nullptr, 0);
    _deviceContext->VSSetConstantBuffers(0, 1,
        _transformConstantBuffer.GetAddressOf());
    _deviceContext->PSSetShader(_hudPixelShader.Get(), nullptr, 0);
    _deviceContext->PSSetShaderResources(0, 1, _hudTextureSRV.GetAddressOf());
    _deviceContext->PSSetSamplers(0, 1, _hudSamplerState.GetAddressOf());
    _deviceContext->Draw(static_cast<UINT>(vertices.size()), 0);

    ID3D11ShaderResourceView *nullSRV = nullptr;
    _deviceContext->PSSetShaderResources(0, 1, &nullSRV);
}

void D3DWidget::_updatePresentFPS() {
    const auto now = std::chrono::steady_clock::now();
    if (_presentFPSWindowStartedAt.time_since_epoch().count() == 0) {
        _presentFPSWindowStartedAt = now;
    }

    _presentFrameCounter++;
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - _presentFPSWindowStartedAt
    );
    if (elapsed.count() < 250) {
        return;
    }

    const double elapsedSeconds = static_cast<double>(elapsed.count()) / 1000.0;
    if (elapsedSeconds > 0.0) {
        _presentFPS = static_cast<double>(_presentFrameCounter) / elapsedSeconds;
    }
    _presentFPSWindowStartedAt = now;
    _presentFrameCounter = 0;
}

bool D3DWidget::_updateTransformBuffer(
    const std::optional<PreviewTransform> &transform
) {
    if (!_transformConstantBuffer) {
        return false;
    }

    const QSize renderSize = _swapChainSize();
    const qreal deviceScale = std::max<qreal>(1.0, devicePixelRatioF());
    const QPointF center = transform.has_value()
        ? transform->center * deviceScale
        : QPointF(static_cast<double>(renderSize.width()) / 2.0,
            static_cast<double>(renderSize.height()) / 2.0);
    const QPointF translation = transform.has_value()
        ? transform->translation * deviceScale
        : QPointF();
    const double scaleX = transform.has_value() ? transform->scaleX : 1.0;
    const double scaleY = transform.has_value() ? transform->scaleY : 1.0;

    const TransformConstants constants{
        { static_cast<float>(center.x()), static_cast<float>(center.y()) },
        { static_cast<float>(translation.x()), static_cast<float>(translation.y()) },
        { static_cast<float>(scaleX), static_cast<float>(scaleY) },
        { static_cast<float>(std::max(1, renderSize.width())),
            static_cast<float>(std::max(1, renderSize.height())) }
    };
    _deviceContext->UpdateSubresource(_transformConstantBuffer.Get(), 0, nullptr,
        &constants, 0, 0);
    return true;
}

void D3DWidget::_resetPresentationResources() {
    _swapChainReady = false;
    if (_deviceContext) {
        _deviceContext->ClearState();
        _deviceContext->Flush();
    }
    _renderTargetView.Reset();
    _frameTextureReady = false;
    _frameTextureSize = {};
    _frameTexture.Reset();
    _frameTextureSRV.Reset();
}

QSize D3DWidget::_swapChainSize() const {
    const qreal dpr = std::max<qreal>(1.0, devicePixelRatioF());
    return QSize(std::max(1,
            static_cast<int>(std::lround(static_cast<double>(width()) * dpr))),
        std::max(1,
            static_cast<int>(std::lround(static_cast<double>(height()) * dpr))));
}

D3DPresentationSurface D3DWidget::_presentationSurface() const {
    D3DPresentationSurface surface;
    if (!_frameTextureReady || !_frameTexture) {
        return surface;
    }

    surface.device = _device.Get();
    surface.deviceContext = _deviceContext.Get();
    surface.texture = _frameTexture.Get();
    surface.width = _frameTextureSize.width();
    surface.height = _frameTextureSize.height();
    return surface;
}

std::optional<ViewTextState> D3DWidget::_targetPreviewView() const {
    if (!_host) {
        return std::nullopt;
    }

    ViewTextState view = _host->currentViewTextState();
    if (!view.valid) {
        return view;
    }

    if (!_panPreviewOffset.isNull()) {
        QString error;
        ViewTextState pannedView;
        if (_host->previewPannedViewState(
                mapToOutputDelta(size(), view.outputSize, _panPreviewOffset),
                pannedView, error)) {
            view = pannedView;
        } else {
            view.valid = false;
            return view;
        }
    }

    if (!NumberUtil::almostEqual(_zoomPreviewScale, 1.0)) {
        QString error;
        ViewTextState scaledView;
        const QPoint outputCenter = mapToOutputPixel(size(), view.outputSize,
            QPoint(width() / 2, height() / 2));
        if (_host->previewScaledViewState(outputCenter, _zoomPreviewScale,
                scaledView, error)) {
            view = scaledView;
        } else {
            view.valid = false;
            return view;
        }
    }

    return view;
}

std::optional<D3DWidget::PreviewTransform>
D3DWidget::_calculatePreviewTransform() const {
    if (!_host || !_host->shouldUseInteractionPreviewFallback()) {
        return std::nullopt;
    }

    const QSize logicalSize = size();
    if (logicalSize.width() <= 0 || logicalSize.height() <= 0) {
        return std::nullopt;
    }

    const ViewTextState sourceView = _displayedFrame.view;
    const std::optional<ViewTextState> targetView = _targetPreviewView();
    if (!sourceView.valid || !targetView.has_value() || !targetView->valid) {
        return std::nullopt;
    }

    if (sourceView.outputSize.width() <= 0
        || sourceView.outputSize.height() <= 0
        || targetView->outputSize.width() <= 0
        || targetView->outputSize.height() <= 0) {
        return std::nullopt;
    }

    const int targetMidX = std::clamp(targetView->outputSize.width() / 2, 0,
        std::max(0, targetView->outputSize.width() - 1));
    const int targetMidY = std::clamp(targetView->outputSize.height() / 2, 0,
        std::max(0, targetView->outputSize.height() - 1));

    QPointF sourceLeft;
    QPointF sourceRight;
    QPointF sourceTop;
    QPointF sourceBottom;
    QString error;
    if (!_host->mapViewPixelToViewPixel(sourceView, *targetView,
            QPoint(0, targetMidY), sourceLeft, error)
        || !_host->mapViewPixelToViewPixel(sourceView, *targetView,
            QPoint(std::max(0, targetView->outputSize.width() - 1), targetMidY),
            sourceRight, error)
        || !_host->mapViewPixelToViewPixel(sourceView, *targetView,
            QPoint(targetMidX, 0), sourceTop, error)
        || !_host->mapViewPixelToViewPixel(sourceView, *targetView,
            QPoint(targetMidX, std::max(0, targetView->outputSize.height() - 1)),
            sourceBottom, error)) {
        return std::nullopt;
    }

    const auto sourceToLogical = [&](const QPointF &pixel) {
        return QPointF(pixel.x() * logicalSize.width()
            / std::max(1, sourceView.outputSize.width()),
            pixel.y() * logicalSize.height()
                / std::max(1, sourceView.outputSize.height()));
    };
    const auto targetToLogical = [&](const QPoint &pixel) {
        return QPointF(static_cast<double>(pixel.x()) * logicalSize.width()
            / std::max(1, targetView->outputSize.width()),
            static_cast<double>(pixel.y()) * logicalSize.height()
                / std::max(1, targetView->outputSize.height()));
    };

    const QPointF sourceLeftLogical = sourceToLogical(sourceLeft);
    const QPointF sourceRightLogical = sourceToLogical(sourceRight);
    const QPointF sourceTopLogical = sourceToLogical(sourceTop);
    const QPointF sourceBottomLogical = sourceToLogical(sourceBottom);
    const QPointF targetLeftLogical = targetToLogical(QPoint(0, targetMidY));
    const QPointF targetRightLogical = targetToLogical(
        QPoint(std::max(0, targetView->outputSize.width() - 1), targetMidY)
    );
    const QPointF targetTopLogical = targetToLogical(QPoint(targetMidX, 0));
    const QPointF targetBottomLogical = targetToLogical(
        QPoint(targetMidX, std::max(0, targetView->outputSize.height() - 1))
    );

    const double sourceSpanX = sourceRightLogical.x() - sourceLeftLogical.x();
    const double sourceSpanY = sourceBottomLogical.y() - sourceTopLogical.y();
    if (NumberUtil::almostEqual(sourceSpanX, 0.0, 1e-9)
        || NumberUtil::almostEqual(sourceSpanY, 0.0, 1e-9)) {
        return std::nullopt;
    }

    const QPointF center(static_cast<double>(logicalSize.width()) / 2.0,
        static_cast<double>(logicalSize.height()) / 2.0);
    const double scaleX
        = (targetRightLogical.x() - targetLeftLogical.x()) / sourceSpanX;
    const double scaleY
        = (targetBottomLogical.y() - targetTopLogical.y()) / sourceSpanY;
    const double translationXLeft = targetLeftLogical.x() - center.x()
        - scaleX * (sourceLeftLogical.x() - center.x());
    const double translationXRight = targetRightLogical.x() - center.x()
        - scaleX * (sourceRightLogical.x() - center.x());
    const double translationYTop = targetTopLogical.y() - center.y()
        - scaleY * (sourceTopLogical.y() - center.y());
    const double translationYBottom = targetBottomLogical.y() - center.y()
        - scaleY * (sourceBottomLogical.y() - center.y());
    const QPointF translation((translationXLeft + translationXRight) * 0.5,
        (translationYTop + translationYBottom) * 0.5);
    if (!std::isfinite(translation.x()) || !std::isfinite(translation.y())
        || !std::isfinite(scaleX) || !std::isfinite(scaleY) || !(scaleX > 0.0)
        || !(scaleY > 0.0)) {
        return std::nullopt;
    }

    const bool active = !NumberUtil::almostEqual(scaleX, 1.0, 1e-6)
        || !NumberUtil::almostEqual(scaleY, 1.0, 1e-6)
        || !NumberUtil::almostEqual(translation.x(), 0.0, 1e-3)
        || !NumberUtil::almostEqual(translation.y(), 0.0, 1e-3);
    if (!active) {
        return std::nullopt;
    }

    return PreviewTransform{ center, translation, scaleX, scaleY };
}

void D3DWidget::_buildHUDVertices(std::vector<Vertex> &vertices) const {
    const int fpsValue = std::max(0,
        static_cast<int>(std::lround(_presentFPS)));
    const std::string text = std::to_string(fpsValue);
    if (text.empty()) {
        return;
    }

    static constexpr float marginPx = 4.0f;
    static constexpr float digitWidthPx = 6.0f;
    static constexpr float digitHeightPx = 10.0f;
    static constexpr float digitSpacingPx = 2.0f;

    vertices.clear();
    vertices.reserve(text.size() * 6);
    const float top = static_cast<float>(std::max(0, height()))
        - marginPx - digitHeightPx;
    for (size_t i = 0; i < text.size(); i++) {
        const int digit = text[i] >= '0' && text[i] <= '9'
            ? (text[i] - '0')
            : 0;
        const float left = marginPx
            + static_cast<float>(i) * (digitWidthPx + digitSpacingPx);
        const float right = left + digitWidthPx;
        const float bottom = top + digitHeightPx;
        const float x0 = pixelToNdcX(left, width());
        const float x1 = pixelToNdcX(right, width());
        const float y0 = pixelToNdcY(top, height());
        const float y1 = pixelToNdcY(bottom, height());
        const float u0 = static_cast<float>(digit * hudDigitStride)
            / static_cast<float>(hudAtlasWidth);
        const float u1 = static_cast<float>(digit * hudDigitStride + hudDigitWidth)
            / static_cast<float>(hudAtlasWidth);

        vertices.push_back(makeVertex(x0, y0, u0, 0.0f));
        vertices.push_back(makeVertex(x1, y0, u1, 0.0f));
        vertices.push_back(makeVertex(x0, y1, u0, 1.0f));
        vertices.push_back(makeVertex(x0, y1, u0, 1.0f));
        vertices.push_back(makeVertex(x1, y0, u1, 0.0f));
        vertices.push_back(makeVertex(x1, y1, u1, 1.0f));
    }
}
