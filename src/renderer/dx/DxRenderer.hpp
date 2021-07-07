// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "../../renderer/inc/RenderEngineBase.hpp"

#include <functional>

#include <dxgi.h>
#include <dxgi1_2.h>
#include <dxgi1_3.h>

#include <d3d11.h>
#include <d2d1.h>
#include <d2d1_1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <dwrite_1.h>
#include <dwrite_2.h>
#include <dwrite_3.h>

#include <wrl.h>
#include <wrl/client.h>

#include "CustomTextLayout.h"
#include "CustomTextRenderer.h"

#include "../../types/inc/Viewport.hpp"

#include <TraceLoggingProvider.h>

TRACELOGGING_DECLARE_PROVIDER(g_hDxRenderProvider);

namespace Microsoft::Console::Render
{
    class DxEngine final : public RenderEngineBase
    {
    public:
        DxEngine();
        ~DxEngine();
        DxEngine(const DxEngine&) = default;
        DxEngine(DxEngine&&) = default;
        DxEngine& operator=(const DxEngine&) = default;
        DxEngine& operator=(DxEngine&&) = default;

        // Used to release device resources so that another instance of
        // conhost can render to the screen (i.e. only one DirectX
        // application may control the screen at a time.)
        [[nodiscard]] HRESULT Enable() noexcept;
        [[nodiscard]] HRESULT Disable() noexcept;

        [[nodiscard]] HRESULT SetHwnd(const HWND hwnd) noexcept;

        [[nodiscard]] HRESULT SetWindowSize(const SIZE pixels) noexcept;

        void SetCallback(std::function<void()> pfn);

        bool GetRetroTerminalEffects() const noexcept;
        void SetRetroTerminalEffects(bool enable) noexcept;

        void SetForceFullRepaintRendering(bool enable) noexcept;

        void SetSoftwareRendering(bool enable) noexcept;

        ::Microsoft::WRL::ComPtr<IDXGISwapChain1> GetSwapChain();

        // IRenderEngine Members
        [[nodiscard]] HRESULT Invalidate(const SMALL_RECT* const psrRegion) noexcept override;
        [[nodiscard]] HRESULT InvalidateCursor(const COORD* const pcoordCursor) noexcept override;
        [[nodiscard]] HRESULT InvalidateSystem(const RECT* const prcDirtyClient) noexcept override;
        [[nodiscard]] HRESULT InvalidateSelection(const std::vector<SMALL_RECT>& rectangles) noexcept override;
        [[nodiscard]] HRESULT InvalidateScroll(const COORD* const pcoordDelta) noexcept override;
        [[nodiscard]] HRESULT InvalidateAll() noexcept override;
        [[nodiscard]] HRESULT InvalidateCircling(_Out_ bool* const pForcePaint) noexcept override;
        [[nodiscard]] HRESULT PrepareForTeardown(_Out_ bool* const pForcePaint) noexcept override;

        [[nodiscard]] HRESULT StartPaint() noexcept override;
        [[nodiscard]] HRESULT EndPaint() noexcept override;

        void WaitUntilCanRender() noexcept override;
        [[nodiscard]] HRESULT Present() noexcept override;

        [[nodiscard]] HRESULT ScrollFrame() noexcept override;

        [[nodiscard]] HRESULT PrepareRenderInfo(const RenderFrameInfo& info) noexcept override;

        [[nodiscard]] HRESULT PaintBackground() noexcept override;
        [[nodiscard]] HRESULT PaintBufferLine(gsl::span<const Cluster> const clusters,
                                              COORD const coord,
                                              bool const fTrimLeft,
                                              const bool lineWrapped) noexcept override;

        [[nodiscard]] HRESULT PaintBufferGridLines(GridLines const lines, COLORREF const color, size_t const cchLine, COORD const coordTarget) noexcept override;
        [[nodiscard]] HRESULT PaintSelection(const SMALL_RECT rect) noexcept override;

        [[nodiscard]] HRESULT PaintCursor(const CursorOptions& options) noexcept override;

        [[nodiscard]] HRESULT UpdateDrawingBrushes(const TextAttribute& textAttributes,
                                                   const gsl::not_null<IRenderData*> pData,
                                                   const bool isSettingDefaultBrushes) noexcept override;
        [[nodiscard]] HRESULT UpdateFont(const FontInfoDesired& fiFontInfoDesired, FontInfo& fiFontInfo) noexcept override;
        [[nodiscard]] HRESULT UpdateDpi(int const iDpi) noexcept override;
        [[nodiscard]] HRESULT UpdateViewport(const SMALL_RECT srNewViewport) noexcept override;

        [[nodiscard]] HRESULT GetProposedFont(const FontInfoDesired& fiFontInfoDesired, FontInfo& fiFontInfo, int const iDpi) noexcept override;

        [[nodiscard]] std::vector<til::rectangle> GetDirtyArea() override;

        [[nodiscard]] HRESULT GetFontSize(_Out_ COORD* const pFontSize) noexcept override;
        [[nodiscard]] HRESULT IsGlyphWideByFont(const std::wstring_view glyph, _Out_ bool* const pResult) noexcept override;

        [[nodiscard]] ::Microsoft::Console::Types::Viewport GetViewportInCharacters(const ::Microsoft::Console::Types::Viewport& viewInPixels) noexcept;
        [[nodiscard]] ::Microsoft::Console::Types::Viewport GetViewportInPixels(const ::Microsoft::Console::Types::Viewport& viewInCharacters) noexcept;

        float GetScaling() const noexcept;

        void SetSelectionBackground(const COLORREF color, const float alpha = 0.5f) noexcept;
        void SetAntialiasingMode(const D2D1_TEXT_ANTIALIAS_MODE antialiasingMode) noexcept;
        void SetDefaultTextBackgroundOpacity(const float opacity) noexcept;

        void UpdateHyperlinkHoveredId(const uint16_t hoveredId) noexcept;

    protected:
        [[nodiscard]] HRESULT _DoUpdateTitle(_In_ const std::wstring& newTitle) noexcept override;
        [[nodiscard]] HRESULT _PaintTerminalEffects() noexcept;

    private:
        enum class SwapChainMode
        {
            ForHwnd,
            ForComposition
        };

        SwapChainMode _chainMode;

        HWND _hwndTarget;
        til::size _sizeTarget;
        int _dpi;
        float _scale;
        float _prevScale;

        std::function<void()> _pfn;

        bool _isEnabled;
        bool _isPainting;

        struct LineMetrics
        {
            float gridlineWidth;
            float underlineOffset;
            float underlineOffset2;
            float underlineWidth;
            float strikethroughOffset;
            float strikethroughWidth;
        };

        LineMetrics _lineMetrics;
        til::size _displaySizePixels;
        til::size _glyphCell;
        ::Microsoft::WRL::ComPtr<IBoxDrawingEffect> _boxDrawingEffect;

        D2D1_COLOR_F _defaultForegroundColor;
        D2D1_COLOR_F _defaultBackgroundColor;

        D2D1_COLOR_F _foregroundColor;
        D2D1_COLOR_F _backgroundColor;
        D2D1_COLOR_F _selectionBackground;

        uint16_t _hyperlinkHoveredId;

        bool _firstFrame;
        bool _invalidateFullRows;
        til::bitmap _invalidMap;
        til::point _invalidScroll;
        bool _allInvalid;

        bool _presentReady;
        std::vector<RECT> _presentDirty;
        RECT _presentScroll;
        POINT _presentOffset;
        DXGI_PRESENT_PARAMETERS _presentParams;

        static std::atomic<size_t> _tracelogCount;

        // Device-Independent Resources
        ::Microsoft::WRL::ComPtr<ID2D1Factory1> _d2dFactory;

        ::Microsoft::WRL::ComPtr<IDWriteFactory1> _dwriteFactory;
        ::Microsoft::WRL::ComPtr<IDWriteTextFormat> _dwriteTextFormat;
        ::Microsoft::WRL::ComPtr<IDWriteFontFace1> _dwriteFontFace;
        ::Microsoft::WRL::ComPtr<IDWriteTextAnalyzer1> _dwriteTextAnalyzer;
        ::Microsoft::WRL::ComPtr<CustomTextLayout> _customLayout;
        ::Microsoft::WRL::ComPtr<CustomTextRenderer> _customRenderer;
        ::Microsoft::WRL::ComPtr<ID2D1StrokeStyle> _strokeStyle;
        ::Microsoft::WRL::ComPtr<ID2D1StrokeStyle> _dashStrokeStyle;
        ::Microsoft::WRL::ComPtr<ID2D1StrokeStyle> _hyperlinkStrokeStyle;

        D2D1_STROKE_STYLE_PROPERTIES _strokeStyleProperties;
        D2D1_STROKE_STYLE_PROPERTIES _dashStrokeStyleProperties;

        // Device-Dependent Resources
        bool _recreateDeviceRequested;
        bool _haveDeviceResources;
        ::Microsoft::WRL::ComPtr<ID3D11Device> _d3dDevice;
        ::Microsoft::WRL::ComPtr<ID3D11DeviceContext> _d3dDeviceContext;

        ::Microsoft::WRL::ComPtr<ID2D1Device> _d2dDevice;
        ::Microsoft::WRL::ComPtr<ID2D1DeviceContext> _d2dDeviceContext;
        ::Microsoft::WRL::ComPtr<ID2D1Bitmap1> _d2dBitmap;
        ::Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> _d2dBrushForeground;
        ::Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> _d2dBrushBackground;

        ::Microsoft::WRL::ComPtr<IDXGIFactory2> _dxgiFactory2;
        ::Microsoft::WRL::ComPtr<IDXGIDevice> _dxgiDevice;
        ::Microsoft::WRL::ComPtr<IDXGISurface> _dxgiSurface;

        DXGI_SWAP_CHAIN_DESC1 _swapChainDesc;
        ::Microsoft::WRL::ComPtr<IDXGISwapChain1> _dxgiSwapChain;
        wil::unique_handle _swapChainFrameLatencyWaitableObject;
        std::unique_ptr<DrawingContext> _drawingContext;

        // Terminal effects resources.
        bool _retroTerminalEffects;
        ::Microsoft::WRL::ComPtr<ID3D11RenderTargetView> _renderTargetView;
        ::Microsoft::WRL::ComPtr<ID3D11VertexShader> _vertexShader;
        ::Microsoft::WRL::ComPtr<ID3D11PixelShader> _pixelShader;
        ::Microsoft::WRL::ComPtr<ID3D11InputLayout> _vertexLayout;
        ::Microsoft::WRL::ComPtr<ID3D11Buffer> _screenQuadVertexBuffer;
        ::Microsoft::WRL::ComPtr<ID3D11Buffer> _pixelShaderSettingsBuffer;
        ::Microsoft::WRL::ComPtr<ID3D11SamplerState> _samplerState;
        ::Microsoft::WRL::ComPtr<ID3D11Texture2D> _framebufferCapture;

        // Preferences and overrides
        bool _softwareRendering;
        bool _forceFullRepaintRendering;

        D2D1_TEXT_ANTIALIAS_MODE _antialiasingMode;

        float _defaultTextBackgroundOpacity;

        // DirectX constant buffers need to be a multiple of 16; align to pad the size.
        __declspec(align(16)) struct
        {
            float ScaledScanLinePeriod;
            float ScaledGaussianSigma;
#pragma warning(suppress : 4324) // structure was padded due to __declspec(align())
        } _pixelShaderSettings;

        [[nodiscard]] HRESULT _CreateDeviceResources(const bool createSwapChain) noexcept;
        HRESULT _SetupTerminalEffects();
        void _ComputePixelShaderSettings() noexcept;

        [[nodiscard]] HRESULT _PrepareRenderTarget() noexcept;

        void _ReleaseDeviceResources() noexcept;

        bool _ShouldForceGrayscaleAA() noexcept;

        [[nodiscard]] HRESULT _CreateTextLayout(
            _In_reads_(StringLength) PCWCHAR String,
            _In_ size_t StringLength,
            _Out_ IDWriteTextLayout** ppTextLayout) noexcept;

        [[nodiscard]] HRESULT _CopyFrontToBack() noexcept;

        [[nodiscard]] HRESULT _EnableDisplayAccess(const bool outputEnabled) noexcept;

        [[nodiscard]] ::Microsoft::WRL::ComPtr<IDWriteFontFace1> _ResolveFontFaceWithFallback(std::wstring& familyName,
                                                                                              DWRITE_FONT_WEIGHT& weight,
                                                                                              DWRITE_FONT_STRETCH& stretch,
                                                                                              DWRITE_FONT_STYLE& style,
                                                                                              std::wstring& localeName) const;

        [[nodiscard]] ::Microsoft::WRL::ComPtr<IDWriteFontFace1> _FindFontFace(std::wstring& familyName,
                                                                               DWRITE_FONT_WEIGHT& weight,
                                                                               DWRITE_FONT_STRETCH& stretch,
                                                                               DWRITE_FONT_STYLE& style,
                                                                               std::wstring& localeName) const;

        [[nodiscard]] std::wstring _GetLocaleName() const;

        [[nodiscard]] std::wstring _GetFontFamilyName(gsl::not_null<IDWriteFontFamily*> const fontFamily,
                                                      std::wstring& localeName) const;

        [[nodiscard]] HRESULT _GetProposedFont(const FontInfoDesired& desired,
                                               FontInfo& actual,
                                               const int dpi,
                                               ::Microsoft::WRL::ComPtr<IDWriteTextFormat>& textFormat,
                                               ::Microsoft::WRL::ComPtr<IDWriteTextAnalyzer1>& textAnalyzer,
                                               ::Microsoft::WRL::ComPtr<IDWriteFontFace1>& fontFace,
                                               LineMetrics& lineMetrics) const noexcept;

        [[nodiscard]] til::size _GetClientSize() const;

        void _InvalidateRectangle(const til::rectangle& rc);
        bool _IsAllInvalid() const noexcept;

        [[nodiscard]] D2D1_COLOR_F _ColorFFromColorRef(const COLORREF color) noexcept;

        // Routine Description:
        // - Helps convert a Direct2D ColorF into a DXGI RGBA
        // Arguments:
        // - color - Direct2D Color F
        // Return Value:
        // - DXGI RGBA
        [[nodiscard]] constexpr DXGI_RGBA s_RgbaFromColorF(const D2D1_COLOR_F color) noexcept
        {
            return { color.r, color.g, color.b, color.a };
        }
    };
}
