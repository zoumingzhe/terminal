/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- XtermEngine.hpp

Abstract:
- This is the definition of the VT specific implementation of the renderer.
    This is the xterm implementation, which supports advanced sequences such as
    inserting and deleting lines, but only 16 colors.

    This engine supports both xterm and xterm-ascii VT modes.
    The difference being that xterm-ascii will render any characters above 0x7f
        as '?', in order to support older legacy tools.

Author(s):
- Mike Griese (migrie) 01-Sept-2017
--*/

#pragma once

#include "vtrenderer.hpp"

namespace Microsoft::Console::Render
{
    class XtermEngine : public VtEngine
    {
    public:
        XtermEngine(_In_ wil::unique_hfile hPipe,
                    const Microsoft::Console::Types::Viewport initialViewport,
                    const bool fUseAsciiOnly);

        virtual ~XtermEngine() override = default;

        [[nodiscard]] HRESULT StartPaint() noexcept override;
        [[nodiscard]] HRESULT EndPaint() noexcept override;

        [[nodiscard]] HRESULT PaintCursor(const CursorOptions& options) noexcept override;

        [[nodiscard]] virtual HRESULT UpdateDrawingBrushes(const TextAttribute& textAttributes,
                                                           const gsl::not_null<IRenderData*> pData,
                                                           const bool isSettingDefaultBrushes) noexcept override;
        [[nodiscard]] HRESULT PaintBufferLine(gsl::span<const Cluster> const clusters,
                                              const COORD coord,
                                              const bool trimLeft,
                                              const bool lineWrapped) noexcept override;
        [[nodiscard]] HRESULT ScrollFrame() noexcept override;

        [[nodiscard]] HRESULT InvalidateScroll(const COORD* const pcoordDelta) noexcept override;

        [[nodiscard]] HRESULT WriteTerminalW(const std::wstring_view str) noexcept override;

    protected:
        const bool _fUseAsciiOnly;
        bool _needToDisableCursor;
        bool _lastCursorIsVisible;
        bool _nextCursorIsVisible;

        [[nodiscard]] HRESULT _MoveCursor(const COORD coord) noexcept override;

        [[nodiscard]] HRESULT _DoUpdateTitle(const std::wstring& newTitle) noexcept override;

#ifdef UNIT_TESTING
        friend class VtRendererTest;
        friend class ConptyOutputTests;
#endif
    };
}
