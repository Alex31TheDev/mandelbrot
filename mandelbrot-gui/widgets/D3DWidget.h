#pragma once

#include <chrono>
#include <optional>
#include <vector>

#include <d3d11.h>
#include <dxgi1_2.h>

#include <QPaintEngine>
#include <QPaintEvent>
#include <QPoint>
#include <QPointF>
#include <QResizeEvent>
#include <QSize>
#include <QShowEvent>
#include <QTimer>
#include <QWidget>

#include <wrl/client.h>

#include "app/GUITypes.h"
#include "runtime/D3DPresentationSurface.h"
#include "windows/viewport/ViewportHost.h"

class D3DWidget final : public QWidget {
public:
    struct Vertex {
        float position[2];
        float texcoord[2];
    };

    explicit D3DWidget(QWidget *parent = nullptr);
    explicit D3DWidget(ViewportHost *host, QWidget *parent = nullptr);
    ~D3DWidget() override;

    void setHost(ViewportHost *host);
    void setPresentationSize(const QSize &size);
    void presentFrame(const GUI::PresentedFrame &frame);
    void clearFrame();
    void refreshPreviewTransform();
    void setPanPreviewOffset(const QPoint &logicalOffset);
    void setZoomPreviewScale(double scale);

    [[nodiscard]] QPoint panPreviewOffset() const { return _panPreviewOffset; }
    [[nodiscard]] double zoomPreviewScale() const { return _zoomPreviewScale; }

protected:
    QPaintEngine *paintEngine() const override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    static constexpr size_t HUDVertexCapacity = 512;

    struct PreviewTransform {
        QPointF center;
        QPointF translation;
        double scaleX = 1.0;
        double scaleY = 1.0;
    };

    struct TransformConstants {
        float center[2]{ 0.0f, 0.0f };
        float translation[2]{ 0.0f, 0.0f };
        float scale[2]{ 1.0f, 1.0f };
        float viewportSize[2]{ 1.0f, 1.0f };
    };

    ViewportHost *_host = nullptr;
    QSize _presentationSize;
    GUI::PresentedFrame _displayedFrame;
    std::optional<PreviewTransform> _cachedPreviewTransform;
    QPoint _panPreviewOffset;
    double _zoomPreviewScale = 1.0;
    QTimer _presentTimer;
    int _presentFrameCounter = 0;
    double _presentFPS = 0.0;
    std::chrono::steady_clock::time_point _presentFPSWindowStartedAt{};
    bool _d3dReady = false;
    bool _swapChainReady = false;
    bool _frameTextureReady = false;
    QSize _frameTextureSize;

    Microsoft::WRL::ComPtr<ID3D11Device> _device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> _deviceContext;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> _swapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> _renderTargetView;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> _frameTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> _frameTextureSRV;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> _hudTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> _hudTextureSRV;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> _vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> _framePixelShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> _hudPixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> _inputLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer> _quadVertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> _hudVertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> _transformConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> _frameSamplerState;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> _hudSamplerState;

    bool _ensureD3D();
    bool _ensureSwapChain();
    bool _ensureFrameTexture(const QSize &textureSize);
    bool _createDeviceResources();
    bool _createSwapChainResources();
    bool _createFrameTextureResources(const QSize &textureSize);
    void _render();
    void _renderFrameTexture();
    void _renderPresentFPS();
    void _updatePresentFPS();
    bool _updateTransformBuffer(
        const std::optional<PreviewTransform> &transform
    );
    void _resetPresentationResources();
    [[nodiscard]] QSize _swapChainSize() const;
    [[nodiscard]] D3DPresentationSurface _presentationSurface() const;
    [[nodiscard]] std::optional<GUI::ViewTextState> _targetPreviewView() const;
    [[nodiscard]] std::optional<PreviewTransform> _calculatePreviewTransform() const;
    void _buildHUDVertices(std::vector<Vertex> &vertices) const;
};
