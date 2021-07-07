// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "HwndTerminal.hpp"
#include <windowsx.h>
#include "../../types/TermControlUiaProvider.hpp"
#include <DefaultSettings.h>
#include "../../renderer/base/Renderer.hpp"
#include "../../renderer/dx/DxRenderer.hpp"
#include "../../cascadia/TerminalCore/Terminal.hpp"
#include "../../types/viewport.cpp"
#include "../../types/inc/GlyphWidth.hpp"

using namespace ::Microsoft::Terminal::Core;

static LPCWSTR term_window_class = L"HwndTerminalClass";

// This magic flag is "documented" at https://msdn.microsoft.com/en-us/library/windows/desktop/ms646301(v=vs.85).aspx
// "If the high-order bit is 1, the key is down; otherwise, it is up."
static constexpr short KeyPressed{ gsl::narrow_cast<short>(0x8000) };

static constexpr bool _IsMouseMessage(UINT uMsg)
{
    return uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONUP || uMsg == WM_LBUTTONDBLCLK ||
           uMsg == WM_MBUTTONDOWN || uMsg == WM_MBUTTONUP || uMsg == WM_MBUTTONDBLCLK ||
           uMsg == WM_RBUTTONDOWN || uMsg == WM_RBUTTONUP || uMsg == WM_RBUTTONDBLCLK ||
           uMsg == WM_MOUSEMOVE || uMsg == WM_MOUSEWHEEL || uMsg == WM_MOUSEHWHEEL;
}

// Helper static function to ensure that all ambiguous-width glyphs are reported as narrow.
// See microsoft/terminal#2066 for more info.
static bool _IsGlyphWideForceNarrowFallback(const std::wstring_view /* glyph */) noexcept
{
    return false; // glyph is not wide.
}

static bool _EnsureStaticInitialization()
{
    // use C++11 magic statics to make sure we only do this once.
    static bool initialized = []() {
        // *** THIS IS A SINGLETON ***
        SetGlyphWidthFallback(_IsGlyphWideForceNarrowFallback);

        return true;
    }();
    return initialized;
}

LRESULT CALLBACK HwndTerminal::HwndTerminalWndProc(
    HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam) noexcept
try
{
#pragma warning(suppress : 26490) // Win32 APIs can only store void*, have to use reinterpret_cast
    HwndTerminal* terminal = reinterpret_cast<HwndTerminal*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    if (terminal)
    {
        if (_IsMouseMessage(uMsg))
        {
            if (terminal->_CanSendVTMouseInput() && terminal->_SendMouseEvent(uMsg, wParam, lParam))
            {
                // GH#6401: Capturing the mouse ensures that we get drag/release events
                // even if the user moves outside the window.
                // _SendMouseEvent returns false if the terminal's not in VT mode, so we'll
                // fall through to release the capture.
                switch (uMsg)
                {
                case WM_LBUTTONDOWN:
                case WM_MBUTTONDOWN:
                case WM_RBUTTONDOWN:
                    SetCapture(hwnd);
                    break;
                case WM_LBUTTONUP:
                case WM_MBUTTONUP:
                case WM_RBUTTONUP:
                    ReleaseCapture();
                    break;
                default:
                    break;
                }

                // Suppress all mouse events that made it into the terminal.
                return 0;
            }
        }

        switch (uMsg)
        {
        case WM_GETOBJECT:
            if (lParam == UiaRootObjectId)
            {
                return UiaReturnRawElementProvider(hwnd, wParam, lParam, terminal->_GetUiaProvider());
            }
            break;
        case WM_LBUTTONDOWN:
            LOG_IF_FAILED(terminal->_StartSelection(lParam));
            return 0;
        case WM_LBUTTONUP:
            terminal->_singleClickTouchdownPos = std::nullopt;
            [[fallthrough]];
        case WM_MBUTTONUP:
        case WM_RBUTTONUP:
            ReleaseCapture();
            break;
        case WM_MOUSEMOVE:
            if (WI_IsFlagSet(wParam, MK_LBUTTON))
            {
                LOG_IF_FAILED(terminal->_MoveSelection(lParam));
                return 0;
            }
            break;
        case WM_RBUTTONDOWN:
            if (terminal->_terminal->IsSelectionActive())
            {
                try
                {
                    const auto bufferData = terminal->_terminal->RetrieveSelectedTextFromBuffer(false);
                    LOG_IF_FAILED(terminal->_CopyTextToSystemClipboard(bufferData, true));
                    TerminalClearSelection(terminal);
                }
                CATCH_LOG();
            }
            else
            {
                terminal->_PasteTextFromClipboard();
            }
            return 0;
        case WM_DESTROY:
            // Release Terminal's hwnd so Teardown doesn't try to destroy it again
            terminal->_hwnd.release();
            terminal->Teardown();
            return 0;
        default:
            break;
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
catch (...)
{
    LOG_CAUGHT_EXCEPTION();
    return 0;
}

static bool RegisterTermClass(HINSTANCE hInstance) noexcept
{
    WNDCLASSW wc;
    if (GetClassInfoW(hInstance, term_window_class, &wc))
    {
        return true;
    }

    wc.style = 0;
    wc.lpfnWndProc = HwndTerminal::HwndTerminalWndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = nullptr;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = term_window_class;

    return RegisterClassW(&wc) != 0;
}

HwndTerminal::HwndTerminal(HWND parentHwnd) :
    _desiredFont{ L"Consolas", 0, DEFAULT_FONT_WEIGHT, { 0, 14 }, CP_UTF8 },
    _actualFont{ L"Consolas", 0, DEFAULT_FONT_WEIGHT, { 0, 14 }, CP_UTF8, false },
    _uiaProvider{ nullptr },
    _uiaProviderInitialized{ false },
    _currentDpi{ USER_DEFAULT_SCREEN_DPI },
    _pfnWriteCallback{ nullptr },
    _multiClickTime{ 500 } // this will be overwritten by the windows system double-click time
{
    _EnsureStaticInitialization();

    HINSTANCE hInstance = wil::GetModuleInstanceHandle();

    if (RegisterTermClass(hInstance))
    {
        _hwnd = wil::unique_hwnd(CreateWindowExW(
            0,
            term_window_class,
            nullptr,
            WS_CHILD |
                WS_CLIPCHILDREN |
                WS_CLIPSIBLINGS |
                WS_VISIBLE,
            0,
            0,
            0,
            0,
            parentHwnd,
            nullptr,
            hInstance,
            nullptr));

#pragma warning(suppress : 26490) // Win32 APIs can only store void*, have to use reinterpret_cast
        SetWindowLongPtr(_hwnd.get(), GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    }
}

HwndTerminal::~HwndTerminal()
{
    Teardown();
}

HRESULT HwndTerminal::Initialize()
{
    _terminal = std::make_unique<::Microsoft::Terminal::Core::Terminal>();
    auto renderThread = std::make_unique<::Microsoft::Console::Render::RenderThread>();
    auto* const localPointerToThread = renderThread.get();
    _renderer = std::make_unique<::Microsoft::Console::Render::Renderer>(_terminal.get(), nullptr, 0, std::move(renderThread));
    RETURN_HR_IF_NULL(E_POINTER, localPointerToThread);
    RETURN_IF_FAILED(localPointerToThread->Initialize(_renderer.get()));

    auto dxEngine = std::make_unique<::Microsoft::Console::Render::DxEngine>();
    RETURN_IF_FAILED(dxEngine->SetHwnd(_hwnd.get()));
    RETURN_IF_FAILED(dxEngine->Enable());
    _renderer->AddRenderEngine(dxEngine.get());

    _UpdateFont(USER_DEFAULT_SCREEN_DPI);
    RECT windowRect;
    GetWindowRect(_hwnd.get(), &windowRect);

    const COORD windowSize{ gsl::narrow<short>(windowRect.right - windowRect.left), gsl::narrow<short>(windowRect.bottom - windowRect.top) };

    // Fist set up the dx engine with the window size in pixels.
    // Then, using the font, get the number of characters that can fit.
    const auto viewInPixels = Viewport::FromDimensions({ 0, 0 }, windowSize);
    RETURN_IF_FAILED(dxEngine->SetWindowSize({ viewInPixels.Width(), viewInPixels.Height() }));

    _renderEngine = std::move(dxEngine);

    _terminal->SetBackgroundCallback([](auto) {});

    _terminal->Create(COORD{ 80, 25 }, 1000, *_renderer);
    _terminal->SetDefaultBackground(RGB(12, 12, 12));
    _terminal->SetDefaultForeground(RGB(204, 204, 204));
    _terminal->SetWriteInputCallback([=](std::wstring& input) noexcept { _WriteTextToConnection(input); });
    localPointerToThread->EnablePainting();

    _multiClickTime = std::chrono::milliseconds{ GetDoubleClickTime() };

    return S_OK;
}

void HwndTerminal::Teardown() noexcept
try
{
    // As a rule, detach resources from the Terminal before shutting them down.
    // This ensures that teardown is reentrant.

    // Shut down the renderer (and therefore the thread) before we implode
    if (auto localRenderEngine{ std::exchange(_renderEngine, nullptr) })
    {
        if (auto localRenderer{ std::exchange(_renderer, nullptr) })
        {
            localRenderer->TriggerTeardown();
            // renderer is destroyed
        }
        // renderEngine is destroyed
    }

    if (auto localHwnd{ _hwnd.release() })
    {
        // If we're being called through WM_DESTROY, we won't get here (hwnd is already released)
        // If we're not, we may end up in Teardown _again_... but by the time we do, all other
        // resources have been released and will not be released again.
        DestroyWindow(localHwnd);
    }
}
CATCH_LOG();

void HwndTerminal::RegisterScrollCallback(std::function<void(int, int, int)> callback)
{
    _terminal->SetScrollPositionChangedCallback(callback);
}

void HwndTerminal::_WriteTextToConnection(const std::wstring& input) noexcept
{
    if (!_pfnWriteCallback)
    {
        return;
    }

    try
    {
        auto callingText{ wil::make_cotaskmem_string(input.data(), input.size()) };
        _pfnWriteCallback(callingText.release());
    }
    CATCH_LOG();
}

void HwndTerminal::RegisterWriteCallback(const void _stdcall callback(wchar_t*))
{
    _pfnWriteCallback = callback;
}

::Microsoft::Console::Types::IUiaData* HwndTerminal::GetUiaData() const noexcept
{
    return _terminal.get();
}

HWND HwndTerminal::GetHwnd() const noexcept
{
    return _hwnd.get();
}

void HwndTerminal::_UpdateFont(int newDpi)
{
    _currentDpi = newDpi;
    auto lock = _terminal->LockForWriting();

    // TODO: MSFT:20895307 If the font doesn't exist, this doesn't
    //      actually fail. We need a way to gracefully fallback.
    _renderer->TriggerFontChange(newDpi, _desiredFont, _actualFont);
}

IRawElementProviderSimple* HwndTerminal::_GetUiaProvider() noexcept
{
    if (nullptr == _uiaProvider && !_uiaProviderInitialized)
    {
        std::unique_lock<std::shared_mutex> lock;
        try
        {
#pragma warning(suppress : 26441) // The lock is named, this appears to be a false positive
            lock = _terminal->LockForWriting();
            if (_uiaProviderInitialized)
            {
                return _uiaProvider.Get();
            }

            LOG_IF_FAILED(::Microsoft::WRL::MakeAndInitialize<::Microsoft::Terminal::TermControlUiaProvider>(&_uiaProvider, this->GetUiaData(), this));
        }
        catch (...)
        {
            LOG_HR(wil::ResultFromCaughtException());
            _uiaProvider = nullptr;
        }
        _uiaProviderInitialized = true;
    }

    return _uiaProvider.Get();
}

HRESULT HwndTerminal::Refresh(const SIZE windowSize, _Out_ COORD* dimensions)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, dimensions);

    auto lock = _terminal->LockForWriting();

    _terminal->ClearSelection();

    RETURN_IF_FAILED(_renderEngine->SetWindowSize(windowSize));

    // Invalidate everything
    _renderer->TriggerRedrawAll();

    // Convert our new dimensions to characters
    const auto viewInPixels = Viewport::FromDimensions({ 0, 0 },
                                                       { gsl::narrow<short>(windowSize.cx), gsl::narrow<short>(windowSize.cy) });
    const auto vp = _renderEngine->GetViewportInCharacters(viewInPixels);

    // If this function succeeds with S_FALSE, then the terminal didn't
    //      actually change size. No need to notify the connection of this
    //      no-op.
    // TODO: MSFT:20642295 Resizing the buffer will corrupt it
    // I believe we'll need support for CSI 2J, and additionally I think
    //      we're resetting the viewport to the top
    RETURN_IF_FAILED(_terminal->UserResize({ vp.Width(), vp.Height() }));
    dimensions->X = vp.Width();
    dimensions->Y = vp.Height();

    return S_OK;
}

void HwndTerminal::SendOutput(std::wstring_view data)
{
    _terminal->Write(data);
}

HRESULT _stdcall CreateTerminal(HWND parentHwnd, _Out_ void** hwnd, _Out_ void** terminal)
{
    // In order for UIA to hook up properly there needs to be a "static" window hosting the
    // inner win32 control. If the static window is not present then WM_GETOBJECT messages
    // will not reach the child control, and the uia element will not be present in the tree.
    auto _hostWindow = CreateWindowEx(
        0,
        L"static",
        nullptr,
        WS_CHILD |
            WS_CLIPCHILDREN |
            WS_CLIPSIBLINGS |
            WS_VISIBLE,
        0,
        0,
        0,
        0,
        parentHwnd,
        nullptr,
        nullptr,
        nullptr);
    auto _terminal = std::make_unique<HwndTerminal>(_hostWindow);
    RETURN_IF_FAILED(_terminal->Initialize());

    *hwnd = _hostWindow;
    *terminal = _terminal.release();

    return S_OK;
}

void _stdcall TerminalRegisterScrollCallback(void* terminal, void __stdcall callback(int, int, int))
{
    auto publicTerminal = static_cast<HwndTerminal*>(terminal);
    publicTerminal->RegisterScrollCallback(callback);
}

void _stdcall TerminalRegisterWriteCallback(void* terminal, const void __stdcall callback(wchar_t*))
{
    const auto publicTerminal = static_cast<HwndTerminal*>(terminal);
    publicTerminal->RegisterWriteCallback(callback);
}

void _stdcall TerminalSendOutput(void* terminal, LPCWSTR data)
{
    const auto publicTerminal = static_cast<HwndTerminal*>(terminal);
    publicTerminal->SendOutput(data);
}

/// <summary>
/// Triggers a terminal resize using the new width and height in pixel.
/// </summary>
/// <param name="terminal">Terminal pointer.</param>
/// <param name="width">New width of the terminal in pixels.</param>
/// <param name="height">New height of the terminal in pixels</param>
/// <param name="dimensions">Out parameter containing the columns and rows that fit the new size.</param>
/// <returns>HRESULT of the attempted resize.</returns>
HRESULT _stdcall TerminalTriggerResize(_In_ void* terminal, _In_ short width, _In_ short height, _Out_ COORD* dimensions)
{
    const auto publicTerminal = static_cast<HwndTerminal*>(terminal);

    LOG_IF_WIN32_BOOL_FALSE(SetWindowPos(
        publicTerminal->GetHwnd(),
        nullptr,
        0,
        0,
        static_cast<int>(width),
        static_cast<int>(height),
        0));

    const SIZE windowSize{ width, height };
    return publicTerminal->Refresh(windowSize, dimensions);
}

/// <summary>
/// Helper method for resizing the terminal using character column and row counts
/// </summary>
/// <param name="terminal">Pointer to the terminal object.</param>
/// <param name="dimensionsInCharacters">New terminal size in row and column count.</param>
/// <param name="dimensionsInPixels">Out parameter with the new size of the renderer.</param>
/// <returns>HRESULT of the attempted resize.</returns>
HRESULT _stdcall TerminalTriggerResizeWithDimension(_In_ void* terminal, _In_ COORD dimensionsInCharacters, _Out_ SIZE* dimensionsInPixels)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, dimensionsInPixels);

    const auto publicTerminal = static_cast<const HwndTerminal*>(terminal);

    const auto viewInCharacters = Viewport::FromDimensions({ 0, 0 }, { (dimensionsInCharacters.X), (dimensionsInCharacters.Y) });
    const auto viewInPixels = publicTerminal->_renderEngine->GetViewportInPixels(viewInCharacters);

    dimensionsInPixels->cx = viewInPixels.Width();
    dimensionsInPixels->cy = viewInPixels.Height();

    COORD unused{ 0, 0 };

    return TerminalTriggerResize(terminal, viewInPixels.Width(), viewInPixels.Height(), &unused);
}

/// <summary>
/// Calculates the amount of rows and columns that fit in the provided width and height.
/// </summary>
/// <param name="terminal">Terminal pointer</param>
/// <param name="width">Width of the terminal area to calculate.</param>
/// <param name="height">Height of the terminal area to calculate.</param>
/// <param name="dimensions">Out parameter containing the columns and rows that fit the new size.</param>
/// <returns>HRESULT of the calculation.</returns>
HRESULT _stdcall TerminalCalculateResize(_In_ void* terminal, _In_ short width, _In_ short height, _Out_ COORD* dimensions)
{
    const auto publicTerminal = static_cast<const HwndTerminal*>(terminal);

    const auto viewInPixels = Viewport::FromDimensions({ 0, 0 }, { width, height });
    const auto viewInCharacters = publicTerminal->_renderEngine->GetViewportInCharacters(viewInPixels);

    dimensions->X = viewInCharacters.Width();
    dimensions->Y = viewInCharacters.Height();

    return S_OK;
}

void _stdcall TerminalDpiChanged(void* terminal, int newDpi)
{
    const auto publicTerminal = static_cast<HwndTerminal*>(terminal);
    publicTerminal->_UpdateFont(newDpi);
}

void _stdcall TerminalUserScroll(void* terminal, int viewTop)
{
    const auto publicTerminal = static_cast<const HwndTerminal*>(terminal);
    publicTerminal->_terminal->UserScrollViewport(viewTop);
}

const unsigned int HwndTerminal::_NumberOfClicks(til::point point, std::chrono::steady_clock::time_point timestamp) noexcept
{
    // if click occurred at a different location or past the multiClickTimer...
    const auto delta{ timestamp - _lastMouseClickTimestamp };
    if (point != _lastMouseClickPos || delta > _multiClickTime)
    {
        // exit early. This is a single click.
        _multiClickCounter = 1;
    }
    else
    {
        _multiClickCounter++;
    }
    return _multiClickCounter;
}

HRESULT HwndTerminal::_StartSelection(LPARAM lParam) noexcept
try
{
    const til::point cursorPosition{
        GET_X_LPARAM(lParam),
        GET_Y_LPARAM(lParam),
    };

    auto lock = _terminal->LockForWriting();
    const bool altPressed = GetKeyState(VK_MENU) < 0;
    const til::size fontSize{ this->_actualFont.GetSize() };

    this->_terminal->SetBlockSelection(altPressed);

    const auto clickCount{ _NumberOfClicks(cursorPosition, std::chrono::steady_clock::now()) };

    // This formula enables the number of clicks to cycle properly between single-, double-, and triple-click.
    // To increase the number of acceptable click states, simply increment MAX_CLICK_COUNT and add another if-statement
    const unsigned int MAX_CLICK_COUNT = 3;
    const auto multiClickMapper = clickCount > MAX_CLICK_COUNT ? ((clickCount + MAX_CLICK_COUNT - 1) % MAX_CLICK_COUNT) + 1 : clickCount;

    if (multiClickMapper == 3)
    {
        _terminal->MultiClickSelection(cursorPosition / fontSize, ::Terminal::SelectionExpansionMode::Line);
    }
    else if (multiClickMapper == 2)
    {
        _terminal->MultiClickSelection(cursorPosition / fontSize, ::Terminal::SelectionExpansionMode::Word);
    }
    else
    {
        this->_terminal->ClearSelection();
        _singleClickTouchdownPos = cursorPosition;

        _lastMouseClickTimestamp = std::chrono::steady_clock::now();
        _lastMouseClickPos = cursorPosition;
    }
    this->_renderer->TriggerSelection();

    return S_OK;
}
CATCH_RETURN();

HRESULT HwndTerminal::_MoveSelection(LPARAM lParam) noexcept
try
{
    const til::point cursorPosition{
        GET_X_LPARAM(lParam),
        GET_Y_LPARAM(lParam),
    };

    auto lock = _terminal->LockForWriting();
    const til::size fontSize{ this->_actualFont.GetSize() };

    RETURN_HR_IF(E_NOT_VALID_STATE, fontSize.area() == 0); // either dimension = 0, area == 0

    if (this->_singleClickTouchdownPos)
    {
        const auto& touchdownPoint{ *this->_singleClickTouchdownPos };
        const auto distance{ std::sqrtf(std::powf(cursorPosition.x<float>() - touchdownPoint.x<float>(), 2) + std::powf(cursorPosition.y<float>() - touchdownPoint.y<float>(), 2)) };
        if (distance >= (std::min(fontSize.width(), fontSize.height()) / 4.f))
        {
            _terminal->SetSelectionAnchor(touchdownPoint / fontSize);
            // stop tracking the touchdown point
            _singleClickTouchdownPos = std::nullopt;
        }
    }

    this->_terminal->SetSelectionEnd(cursorPosition / fontSize);
    this->_renderer->TriggerSelection();

    return S_OK;
}
CATCH_RETURN();

void HwndTerminal::_ClearSelection() noexcept
try
{
    auto lock{ _terminal->LockForWriting() };
    _terminal->ClearSelection();
    _renderer->TriggerSelection();
}
CATCH_LOG();

void _stdcall TerminalClearSelection(void* terminal)
{
    auto publicTerminal = static_cast<HwndTerminal*>(terminal);
    publicTerminal->_ClearSelection();
}

bool _stdcall TerminalIsSelectionActive(void* terminal)
{
    const auto publicTerminal = static_cast<const HwndTerminal*>(terminal);
    const bool selectionActive = publicTerminal->_terminal->IsSelectionActive();
    return selectionActive;
}

// Returns the selected text in the terminal.
const wchar_t* _stdcall TerminalGetSelection(void* terminal)
{
    auto publicTerminal = static_cast<HwndTerminal*>(terminal);

    const auto bufferData = publicTerminal->_terminal->RetrieveSelectedTextFromBuffer(false);
    publicTerminal->_ClearSelection();

    // convert text: vector<string> --> string
    std::wstring selectedText;
    for (const auto& text : bufferData.text)
    {
        selectedText += text;
    }

    auto returnText = wil::make_cotaskmem_string_nothrow(selectedText.c_str());
    return returnText.release();
}

static ControlKeyStates getControlKeyState() noexcept
{
    struct KeyModifier
    {
        int vkey;
        ControlKeyStates flags;
    };

    constexpr std::array<KeyModifier, 5> modifiers{ {
        { VK_RMENU, ControlKeyStates::RightAltPressed },
        { VK_LMENU, ControlKeyStates::LeftAltPressed },
        { VK_RCONTROL, ControlKeyStates::RightCtrlPressed },
        { VK_LCONTROL, ControlKeyStates::LeftCtrlPressed },
        { VK_SHIFT, ControlKeyStates::ShiftPressed },
    } };

    ControlKeyStates flags;

    for (const auto& mod : modifiers)
    {
        const auto state = GetKeyState(mod.vkey);
        const auto isDown = state < 0;

        if (isDown)
        {
            flags |= mod.flags;
        }
    }

    return flags;
}

bool HwndTerminal::_CanSendVTMouseInput() const noexcept
{
    // Only allow the transit of mouse events if shift isn't pressed.
    const bool shiftPressed = GetKeyState(VK_SHIFT) < 0;
    return !shiftPressed && _focused && _terminal->IsTrackingMouseInput();
}

bool HwndTerminal::_SendMouseEvent(UINT uMsg, WPARAM wParam, LPARAM lParam) noexcept
try
{
    til::point cursorPosition{
        GET_X_LPARAM(lParam),
        GET_Y_LPARAM(lParam),
    };

    const til::size fontSize{ this->_actualFont.GetSize() };
    short wheelDelta{ 0 };
    if (uMsg == WM_MOUSEWHEEL || uMsg == WM_MOUSEHWHEEL)
    {
        wheelDelta = HIWORD(wParam);

        // If it's a *WHEEL event, it's in screen coordinates, not window (?!)
        POINT coordsToTransform = cursorPosition;
        ScreenToClient(_hwnd.get(), &coordsToTransform);
        cursorPosition = coordsToTransform;
    }

    const TerminalInput::MouseButtonState state{
        WI_IsFlagSet(GetKeyState(VK_LBUTTON), KeyPressed),
        WI_IsFlagSet(GetKeyState(VK_MBUTTON), KeyPressed),
        WI_IsFlagSet(GetKeyState(VK_RBUTTON), KeyPressed)
    };

    return _terminal->SendMouseEvent(cursorPosition / fontSize, uMsg, getControlKeyState(), wheelDelta, state);
}
catch (...)
{
    LOG_CAUGHT_EXCEPTION();
    return false;
}

void HwndTerminal::_SendKeyEvent(WORD vkey, WORD scanCode, WORD flags, bool keyDown) noexcept
try
{
    auto modifiers = getControlKeyState();
    if (WI_IsFlagSet(flags, ENHANCED_KEY))
    {
        modifiers |= ControlKeyStates::EnhancedKey;
    }
    _terminal->SendKeyEvent(vkey, scanCode, modifiers, keyDown);
}
CATCH_LOG();

void HwndTerminal::_SendCharEvent(wchar_t ch, WORD scanCode, WORD flags) noexcept
try
{
    if (_terminal->IsSelectionActive())
    {
        _ClearSelection();
        if (ch == UNICODE_ESC)
        {
            // ESC should clear any selection before it triggers input.
            // Other characters pass through.
            return;
        }
    }

    if (ch == UNICODE_TAB)
    {
        // TAB was handled as a keydown event (cf. Terminal::SendKeyEvent)
        return;
    }

    auto modifiers = getControlKeyState();
    if (WI_IsFlagSet(flags, ENHANCED_KEY))
    {
        modifiers |= ControlKeyStates::EnhancedKey;
    }
    _terminal->SendCharEvent(ch, scanCode, modifiers);
}
CATCH_LOG();

void _stdcall TerminalSendKeyEvent(void* terminal, WORD vkey, WORD scanCode, WORD flags, bool keyDown)
{
    const auto publicTerminal = static_cast<HwndTerminal*>(terminal);
    publicTerminal->_SendKeyEvent(vkey, scanCode, flags, keyDown);
}

void _stdcall TerminalSendCharEvent(void* terminal, wchar_t ch, WORD scanCode, WORD flags)
{
    const auto publicTerminal = static_cast<HwndTerminal*>(terminal);
    publicTerminal->_SendCharEvent(ch, scanCode, flags);
}

void _stdcall DestroyTerminal(void* terminal)
{
    const auto publicTerminal = static_cast<HwndTerminal*>(terminal);
    delete publicTerminal;
}

// Updates the terminal font type, size, color, as well as the background/foreground colors to a specified theme.
void _stdcall TerminalSetTheme(void* terminal, TerminalTheme theme, LPCWSTR fontFamily, short fontSize, int newDpi)
{
    const auto publicTerminal = static_cast<HwndTerminal*>(terminal);
    {
        auto lock = publicTerminal->_terminal->LockForWriting();

        publicTerminal->_terminal->SetDefaultForeground(theme.DefaultForeground);
        publicTerminal->_terminal->SetDefaultBackground(theme.DefaultBackground);
        publicTerminal->_renderEngine->SetSelectionBackground(theme.DefaultSelectionBackground, theme.SelectionBackgroundAlpha);

        // Set the font colors
        for (size_t tableIndex = 0; tableIndex < 16; tableIndex++)
        {
            // It's using gsl::at to check the index is in bounds, but the analyzer still calls this array-to-pointer-decay
            [[gsl::suppress(bounds .3)]] publicTerminal->_terminal->SetColorTableEntry(tableIndex, gsl::at(theme.ColorTable, tableIndex));
        }
    }

    publicTerminal->_terminal->SetCursorStyle(theme.CursorStyle);

    publicTerminal->_desiredFont = { fontFamily, 0, DEFAULT_FONT_WEIGHT, { 0, fontSize }, CP_UTF8 };
    publicTerminal->_UpdateFont(newDpi);

    // When the font changes the terminal dimensions need to be recalculated since the available row and column
    // space will have changed.
    RECT windowRect;
    GetWindowRect(publicTerminal->_hwnd.get(), &windowRect);

    COORD dimensions = {};
    const SIZE windowSize{ windowRect.right - windowRect.left, windowRect.bottom - windowRect.top };
    publicTerminal->Refresh(windowSize, &dimensions);
}

void _stdcall TerminalBlinkCursor(void* terminal)
{
    const auto publicTerminal = static_cast<const HwndTerminal*>(terminal);
    if (!publicTerminal->_terminal->IsCursorBlinkingAllowed() && publicTerminal->_terminal->IsCursorVisible())
    {
        return;
    }

    publicTerminal->_terminal->SetCursorOn(!publicTerminal->_terminal->IsCursorOn());
}

void _stdcall TerminalSetCursorVisible(void* terminal, const bool visible)
{
    const auto publicTerminal = static_cast<const HwndTerminal*>(terminal);
    publicTerminal->_terminal->SetCursorOn(visible);
}

void __stdcall TerminalSetFocus(void* terminal)
{
    auto publicTerminal = static_cast<HwndTerminal*>(terminal);
    publicTerminal->_focused = true;
}

void __stdcall TerminalKillFocus(void* terminal)
{
    auto publicTerminal = static_cast<HwndTerminal*>(terminal);
    publicTerminal->_focused = false;
}

// Routine Description:
// - Copies the text given onto the global system clipboard.
// Arguments:
// - rows - Rows of text data to copy
// - fAlsoCopyFormatting - true if the color and formatting should also be copied, false otherwise
HRESULT HwndTerminal::_CopyTextToSystemClipboard(const TextBuffer::TextAndColor& rows, bool const fAlsoCopyFormatting)
try
{
    std::wstring finalString;

    // Concatenate strings into one giant string to put onto the clipboard.
    for (const auto& str : rows.text)
    {
        finalString += str;
    }

    // allocate the final clipboard data
    const size_t cchNeeded = finalString.size() + 1;
    const size_t cbNeeded = sizeof(wchar_t) * cchNeeded;
    wil::unique_hglobal globalHandle(GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, cbNeeded));
    RETURN_LAST_ERROR_IF_NULL(globalHandle.get());

    PWSTR pwszClipboard = static_cast<PWSTR>(GlobalLock(globalHandle.get()));
    RETURN_LAST_ERROR_IF_NULL(pwszClipboard);

    // The pattern gets a bit strange here because there's no good wil built-in for global lock of this type.
    // Try to copy then immediately unlock. Don't throw until after (so the hglobal won't be freed until we unlock).
    const HRESULT hr = StringCchCopyW(pwszClipboard, cchNeeded, finalString.data());
    GlobalUnlock(globalHandle.get());
    RETURN_IF_FAILED(hr);

    // Set global data to clipboard
    RETURN_LAST_ERROR_IF(!OpenClipboard(_hwnd.get()));

    { // Clipboard Scope
        auto clipboardCloser = wil::scope_exit([]() {
            LOG_LAST_ERROR_IF(!CloseClipboard());
        });

        RETURN_LAST_ERROR_IF(!EmptyClipboard());
        RETURN_LAST_ERROR_IF_NULL(SetClipboardData(CF_UNICODETEXT, globalHandle.get()));

        if (fAlsoCopyFormatting)
        {
            const auto& fontData = _actualFont;
            int const iFontHeightPoints = fontData.GetUnscaledSize().Y; // this renderer uses points already
            const COLORREF bgColor = _terminal->GetAttributeColors(_terminal->GetDefaultBrushColors()).second;

            std::string HTMLToPlaceOnClip = TextBuffer::GenHTML(rows, iFontHeightPoints, fontData.GetFaceName(), bgColor);
            _CopyToSystemClipboard(HTMLToPlaceOnClip, L"HTML Format");

            std::string RTFToPlaceOnClip = TextBuffer::GenRTF(rows, iFontHeightPoints, fontData.GetFaceName(), bgColor);
            _CopyToSystemClipboard(RTFToPlaceOnClip, L"Rich Text Format");
        }
    }

    // only free if we failed.
    // the memory has to remain allocated if we successfully placed it on the clipboard.
    // Releasing the smart pointer will leave it allocated as we exit scope.
    globalHandle.release();

    return S_OK;
}
CATCH_RETURN()

// Routine Description:
// - Copies the given string onto the global system clipboard in the specified format
// Arguments:
// - stringToCopy - The string to copy
// - lpszFormat - the name of the format
HRESULT HwndTerminal::_CopyToSystemClipboard(std::string stringToCopy, LPCWSTR lpszFormat)
{
    const size_t cbData = stringToCopy.size() + 1; // +1 for '\0'
    if (cbData)
    {
        wil::unique_hglobal globalHandleData(GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, cbData));
        RETURN_LAST_ERROR_IF_NULL(globalHandleData.get());

        PSTR pszClipboardHTML = static_cast<PSTR>(GlobalLock(globalHandleData.get()));
        RETURN_LAST_ERROR_IF_NULL(pszClipboardHTML);

        // The pattern gets a bit strange here because there's no good wil built-in for global lock of this type.
        // Try to copy then immediately unlock. Don't throw until after (so the hglobal won't be freed until we unlock).
        const HRESULT hr2 = StringCchCopyA(pszClipboardHTML, cbData, stringToCopy.data());
        GlobalUnlock(globalHandleData.get());
        RETURN_IF_FAILED(hr2);

        UINT const CF_FORMAT = RegisterClipboardFormatW(lpszFormat);
        RETURN_LAST_ERROR_IF(0 == CF_FORMAT);

        RETURN_LAST_ERROR_IF_NULL(SetClipboardData(CF_FORMAT, globalHandleData.get()));

        // only free if we failed.
        // the memory has to remain allocated if we successfully placed it on the clipboard.
        // Releasing the smart pointer will leave it allocated as we exit scope.
        globalHandleData.release();
    }

    return S_OK;
}

void HwndTerminal::_PasteTextFromClipboard() noexcept
{
    // Get paste data from clipboard
    if (!OpenClipboard(_hwnd.get()))
    {
        return;
    }

    HANDLE ClipboardDataHandle = GetClipboardData(CF_UNICODETEXT);
    if (ClipboardDataHandle == nullptr)
    {
        CloseClipboard();
        return;
    }

    PCWCH pwstr = static_cast<PCWCH>(GlobalLock(ClipboardDataHandle));

    _StringPaste(pwstr);

    GlobalUnlock(ClipboardDataHandle);

    CloseClipboard();
}

void HwndTerminal::_StringPaste(const wchar_t* const pData) noexcept
{
    if (pData == nullptr)
    {
        return;
    }

    try
    {
        std::wstring text(pData);
        _WriteTextToConnection(text);
    }
    CATCH_LOG();
}

COORD HwndTerminal::GetFontSize() const
{
    return _actualFont.GetSize();
}

RECT HwndTerminal::GetBounds() const noexcept
{
    RECT windowRect;
    GetWindowRect(_hwnd.get(), &windowRect);
    return windowRect;
}

RECT HwndTerminal::GetPadding() const noexcept
{
    return { 0 };
}

double HwndTerminal::GetScaleFactor() const noexcept
{
    return static_cast<double>(_currentDpi) / static_cast<double>(USER_DEFAULT_SCREEN_DPI);
}

void HwndTerminal::ChangeViewport(const SMALL_RECT NewWindow)
{
    _terminal->UserScrollViewport(NewWindow.Top);
}

HRESULT HwndTerminal::GetHostUiaProvider(IRawElementProviderSimple** provider) noexcept
{
    return UiaHostProviderFromHwnd(_hwnd.get(), provider);
}
