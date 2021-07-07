// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TerminalDispatch.hpp"
using namespace ::Microsoft::Terminal::Core;
using namespace ::Microsoft::Console::VirtualTerminal;

// NOTE:
// Functions related to Set Graphics Renditions (SGR) are in
//      TerminalDispatchGraphics.cpp, not this file

TerminalDispatch::TerminalDispatch(ITerminalApi& terminalApi) noexcept :
    _terminalApi{ terminalApi }
{
}

void TerminalDispatch::Execute(const wchar_t wchControl) noexcept
{
    _terminalApi.ExecuteChar(wchControl);
}

void TerminalDispatch::Print(const wchar_t wchPrintable) noexcept
{
    _terminalApi.PrintString({ &wchPrintable, 1 });
}

void TerminalDispatch::PrintString(const std::wstring_view string) noexcept
{
    _terminalApi.PrintString(string);
}

bool TerminalDispatch::CursorPosition(const size_t line,
                                      const size_t column) noexcept
try
{
    SHORT x{ 0 };
    SHORT y{ 0 };

    RETURN_BOOL_IF_FALSE(SUCCEEDED(SizeTToShort(column, &x)) &&
                         SUCCEEDED(SizeTToShort(line, &y)));

    RETURN_BOOL_IF_FALSE(SUCCEEDED(ShortSub(x, 1, &x)) &&
                         SUCCEEDED(ShortSub(y, 1, &y)));

    return _terminalApi.SetCursorPosition(x, y);
}
CATCH_LOG_RETURN_FALSE()

bool TerminalDispatch::CursorVisibility(const bool isVisible) noexcept
{
    return _terminalApi.SetCursorVisibility(isVisible);
}

bool TerminalDispatch::EnableCursorBlinking(const bool enable) noexcept
{
    return _terminalApi.EnableCursorBlinking(enable);
}

bool TerminalDispatch::CursorForward(const size_t distance) noexcept
try
{
    const auto cursorPos = _terminalApi.GetCursorPosition();
    const COORD newCursorPos{ cursorPos.X + gsl::narrow<short>(distance), cursorPos.Y };
    return _terminalApi.SetCursorPosition(newCursorPos.X, newCursorPos.Y);
}
CATCH_LOG_RETURN_FALSE()

bool TerminalDispatch::CursorBackward(const size_t distance) noexcept
try
{
    const auto cursorPos = _terminalApi.GetCursorPosition();
    const COORD newCursorPos{ cursorPos.X - gsl::narrow<short>(distance), cursorPos.Y };
    return _terminalApi.SetCursorPosition(newCursorPos.X, newCursorPos.Y);
}
CATCH_LOG_RETURN_FALSE()

bool TerminalDispatch::CursorUp(const size_t distance) noexcept
try
{
    const auto cursorPos = _terminalApi.GetCursorPosition();
    const COORD newCursorPos{ cursorPos.X, cursorPos.Y + gsl::narrow<short>(distance) };
    return _terminalApi.SetCursorPosition(newCursorPos.X, newCursorPos.Y);
}
CATCH_LOG_RETURN_FALSE()

bool TerminalDispatch::LineFeed(const DispatchTypes::LineFeedType lineFeedType) noexcept
try
{
    switch (lineFeedType)
    {
    case DispatchTypes::LineFeedType::DependsOnMode:
        // There is currently no need for mode-specific line feeds in the Terminal,
        // so for now we just treat them as a line feed without carriage return.
    case DispatchTypes::LineFeedType::WithoutReturn:
        return _terminalApi.CursorLineFeed(false);
    case DispatchTypes::LineFeedType::WithReturn:
        return _terminalApi.CursorLineFeed(true);
    default:
        return false;
    }
}
CATCH_LOG_RETURN_FALSE()

bool TerminalDispatch::EraseCharacters(const size_t numChars) noexcept
try
{
    return _terminalApi.EraseCharacters(numChars);
}
CATCH_LOG_RETURN_FALSE()

bool TerminalDispatch::WarningBell() noexcept
try
{
    return _terminalApi.WarningBell();
}
CATCH_LOG_RETURN_FALSE()

bool TerminalDispatch::CarriageReturn() noexcept
try
{
    const auto cursorPos = _terminalApi.GetCursorPosition();
    return _terminalApi.SetCursorPosition(0, cursorPos.Y);
}
CATCH_LOG_RETURN_FALSE()

bool TerminalDispatch::SetWindowTitle(std::wstring_view title) noexcept
try
{
    return _terminalApi.SetWindowTitle(title);
}
CATCH_LOG_RETURN_FALSE()

// Method Description:
// - Sets a single entry of the colortable to a new value
// Arguments:
// - tableIndex: The VT color table index
// - color: The new RGB color value to use.
// Return Value:
// True if handled successfully. False otherwise.
bool TerminalDispatch::SetColorTableEntry(const size_t tableIndex,
                                          const DWORD color) noexcept
try
{
    return _terminalApi.SetColorTableEntry(tableIndex, color);
}
CATCH_LOG_RETURN_FALSE()

bool TerminalDispatch::SetCursorStyle(const DispatchTypes::CursorStyle cursorStyle) noexcept
try
{
    return _terminalApi.SetCursorStyle(cursorStyle);
}
CATCH_LOG_RETURN_FALSE()

bool TerminalDispatch::SetCursorColor(const DWORD color) noexcept
try
{
    return _terminalApi.SetCursorColor(color);
}
CATCH_LOG_RETURN_FALSE()

bool TerminalDispatch::SetClipboard(std::wstring_view content) noexcept
try
{
    return _terminalApi.CopyToClipboard(content);
}
CATCH_LOG_RETURN_FALSE()

// Method Description:
// - Sets the default foreground color to a new value
// Arguments:
// - color: The new RGB color value to use, in 0x00BBGGRR form
// Return Value:
// True if handled successfully. False otherwise.
bool TerminalDispatch::SetDefaultForeground(const DWORD color) noexcept
try
{
    return _terminalApi.SetDefaultForeground(color);
}
CATCH_LOG_RETURN_FALSE()

// Method Description:
// - Sets the default background color to a new value
// Arguments:
// - color: The new RGB color value to use, in 0x00BBGGRR form
// Return Value:
// True if handled successfully. False otherwise.
bool TerminalDispatch::SetDefaultBackground(const DWORD color) noexcept
try
{
    return _terminalApi.SetDefaultBackground(color);
}
CATCH_LOG_RETURN_FALSE()

// Method Description:
// - Erases characters in the buffer depending on the erase type
// Arguments:
// - eraseType: the erase type (from beginning, to end, or all)
// Return Value:
// True if handled successfully. False otherwise.
bool TerminalDispatch::EraseInLine(const DispatchTypes::EraseType eraseType) noexcept
try
{
    return _terminalApi.EraseInLine(eraseType);
}
CATCH_LOG_RETURN_FALSE()

// Method Description:
// - Deletes count number of characters starting from where the cursor is currently
// Arguments:
// - count, the number of characters to delete
// Return Value:
// True if handled successfully. False otherwise.
bool TerminalDispatch::DeleteCharacter(const size_t count) noexcept
try
{
    return _terminalApi.DeleteCharacter(count);
}
CATCH_LOG_RETURN_FALSE()

// Method Description:
// - Adds count number of spaces starting from where the cursor is currently
// Arguments:
// - count, the number of spaces to add
// Return Value:
// True if handled successfully, false otherwise
bool TerminalDispatch::InsertCharacter(const size_t count) noexcept
try
{
    return _terminalApi.InsertCharacter(count);
}
CATCH_LOG_RETURN_FALSE()

// Method Description:
// - Moves the viewport and erases text from the buffer depending on the eraseType
// Arguments:
// - eraseType: the desired erase type
// Return Value:
// True if handled successfully. False otherwise
bool TerminalDispatch::EraseInDisplay(const DispatchTypes::EraseType eraseType) noexcept
try
{
    return _terminalApi.EraseInDisplay(eraseType);
}
CATCH_LOG_RETURN_FALSE()

// - DECKPAM, DECKPNM - Sets the keypad input mode to either Application mode or Numeric mode (true, false respectively)
// Arguments:
// - applicationMode - set to true to enable Application Mode Input, false for Numeric Mode Input.
// Return Value:
// - True if handled successfully. False otherwise.
bool TerminalDispatch::SetKeypadMode(const bool fApplicationMode) noexcept
{
    _terminalApi.SetKeypadMode(fApplicationMode);
    return true;
}

// - DECCKM - Sets the cursor keys input mode to either Application mode or Normal mode (true, false respectively)
// Arguments:
// - applicationMode - set to true to enable Application Mode Input, false for Normal Mode Input.
// Return Value:
// - True if handled successfully. False otherwise.
bool TerminalDispatch::SetCursorKeysMode(const bool applicationMode) noexcept
{
    _terminalApi.SetCursorKeysMode(applicationMode);
    return true;
}

// Routine Description:
// - DECSCNM - Sets the screen mode to either normal or reverse.
//    When in reverse screen mode, the background and foreground colors are switched.
// Arguments:
// - reverseMode - set to true to enable reverse screen mode, false for normal mode.
// Return Value:
// - True if handled successfully. False otherwise.
bool TerminalDispatch::SetScreenMode(const bool reverseMode) noexcept
{
    return _terminalApi.SetScreenMode(reverseMode);
}

// Method Description:
// - win32-input-mode: Enable sending full input records encoded as a string of
//   characters to the client application.
// Arguments:
// - win32InputMode - set to true to enable win32-input-mode, false to disable.
// Return Value:
// - True if handled successfully. False otherwise.
bool TerminalDispatch::EnableWin32InputMode(const bool win32Mode) noexcept
{
    _terminalApi.EnableWin32InputMode(win32Mode);
    return true;
}

//Routine Description:
// Enable VT200 Mouse Mode - Enables/disables the mouse input handler in default tracking mode.
//Arguments:
// - enabled - true to enable, false to disable.
// Return value:
// True if handled successfully. False otherwise.
bool TerminalDispatch::EnableVT200MouseMode(const bool enabled) noexcept
{
    _terminalApi.EnableVT200MouseMode(enabled);
    return true;
}

//Routine Description:
// Enable UTF-8 Extended Encoding - this changes the encoding scheme for sequences
//      emitted by the mouse input handler. Does not enable/disable mouse mode on its own.
//Arguments:
// - enabled - true to enable, false to disable.
// Return value:
// True if handled successfully. False otherwise.
bool TerminalDispatch::EnableUTF8ExtendedMouseMode(const bool enabled) noexcept
{
    _terminalApi.EnableUTF8ExtendedMouseMode(enabled);
    return true;
}

//Routine Description:
// Enable SGR Extended Encoding - this changes the encoding scheme for sequences
//      emitted by the mouse input handler. Does not enable/disable mouse mode on its own.
//Arguments:
// - enabled - true to enable, false to disable.
// Return value:
// True if handled successfully. False otherwise.
bool TerminalDispatch::EnableSGRExtendedMouseMode(const bool enabled) noexcept
{
    _terminalApi.EnableSGRExtendedMouseMode(enabled);
    return true;
}

//Routine Description:
// Enable Button Event mode - send mouse move events WITH A BUTTON PRESSED to the input.
//Arguments:
// - enabled - true to enable, false to disable.
// Return value:
// True if handled successfully. False otherwise.
bool TerminalDispatch::EnableButtonEventMouseMode(const bool enabled) noexcept
{
    _terminalApi.EnableButtonEventMouseMode(enabled);
    return true;
}

//Routine Description:
// Enable Any Event mode - send all mouse events to the input.

//Arguments:
// - enabled - true to enable, false to disable.
// Return value:
// True if handled successfully. False otherwise.
bool TerminalDispatch::EnableAnyEventMouseMode(const bool enabled) noexcept
{
    _terminalApi.EnableAnyEventMouseMode(enabled);
    return true;
}

//Routine Description:
// Enable Alternate Scroll Mode - When in the Alt Buffer, send CUP and CUD on
//      scroll up/down events instead of the usual sequences
//Arguments:
// - enabled - true to enable, false to disable.
// Return value:
// True if handled successfully. False otherwise.
bool TerminalDispatch::EnableAlternateScroll(const bool enabled) noexcept
{
    _terminalApi.EnableAlternateScrollMode(enabled);
    return true;
}

bool TerminalDispatch::SetPrivateMode(const DispatchTypes::PrivateModeParams param) noexcept
{
    return _PrivateModeParamsHelper(param, true);
}

bool TerminalDispatch::ResetPrivateMode(const DispatchTypes::PrivateModeParams param) noexcept
{
    return _PrivateModeParamsHelper(param, false);
}

// Method Description:
// - Start a hyperlink
// Arguments:
// - uri - the hyperlink URI
// - params - the optional custom ID
// Return Value:
// - true
bool TerminalDispatch::AddHyperlink(const std::wstring_view uri, const std::wstring_view params) noexcept
{
    return _terminalApi.AddHyperlink(uri, params);
}

// Method Description:
// - End a hyperlink
// Return Value:
// - true
bool TerminalDispatch::EndHyperlink() noexcept
{
    return _terminalApi.EndHyperlink();
}

// Routine Description:
// - Support routine for routing private mode parameters to be set/reset as flags
// Arguments:
// - param - mode parameter to set/reset
// - enable - True for set, false for unset.
// Return Value:
// - True if handled successfully. False otherwise.
bool TerminalDispatch::_PrivateModeParamsHelper(const DispatchTypes::PrivateModeParams param, const bool enable) noexcept
{
    bool success = false;
    switch (param)
    {
    case DispatchTypes::PrivateModeParams::DECCKM_CursorKeysMode:
        // set - Enable Application Mode, reset - Normal mode
        success = SetCursorKeysMode(enable);
        break;
    case DispatchTypes::PrivateModeParams::DECSCNM_ScreenMode:
        success = SetScreenMode(enable);
        break;
    case DispatchTypes::PrivateModeParams::VT200_MOUSE_MODE:
        success = EnableVT200MouseMode(enable);
        break;
    case DispatchTypes::PrivateModeParams::BUTTON_EVENT_MOUSE_MODE:
        success = EnableButtonEventMouseMode(enable);
        break;
    case DispatchTypes::PrivateModeParams::ANY_EVENT_MOUSE_MODE:
        success = EnableAnyEventMouseMode(enable);
        break;
    case DispatchTypes::PrivateModeParams::UTF8_EXTENDED_MODE:
        success = EnableUTF8ExtendedMouseMode(enable);
        break;
    case DispatchTypes::PrivateModeParams::SGR_EXTENDED_MODE:
        success = EnableSGRExtendedMouseMode(enable);
        break;
    case DispatchTypes::PrivateModeParams::ALTERNATE_SCROLL:
        success = EnableAlternateScroll(enable);
        break;
    case DispatchTypes::PrivateModeParams::DECTCEM_TextCursorEnableMode:
        success = CursorVisibility(enable);
        break;
    case DispatchTypes::PrivateModeParams::ATT610_StartCursorBlink:
        success = EnableCursorBlinking(enable);
        break;
    case DispatchTypes::PrivateModeParams::W32IM_Win32InputMode:
        success = EnableWin32InputMode(enable);
        break;
    default:
        // If no functions to call, overall dispatch was a failure.
        success = false;
        break;
    }
    return success;
}

bool TerminalDispatch::SoftReset() noexcept
{
    // TODO:GH#1883 much of this method is not yet implemented in the Terminal,
    // because the Terminal _doesn't need to_ yet. The terminal is only ever
    // connected to conpty, so it doesn't implement most of these things that
    // Hard/Soft Reset would reset. As those things are implemented, they should

    // also get cleared here.
    //
    // This code is left here (from its original form in conhost) as a reminder
    // of what needs to be done.

    bool success = CursorVisibility(true); // Cursor enabled.
    // success = SetOriginMode(false) && success; // Absolute cursor addressing.
    // success = SetAutoWrapMode(true) && success; // Wrap at end of line.
    success = SetCursorKeysMode(false) && success; // Normal characters.
    success = SetKeypadMode(false) && success; // Numeric characters.

    // // Top margin = 1; bottom margin = page length.
    // success = _DoSetTopBottomScrollingMargins(0, 0) && success;

    // _termOutput = {}; // Reset all character set designations.
    // if (_initialCodePage.has_value())
    // {
    //     // Restore initial code page if previously changed by a DOCS sequence.
    //     success = _pConApi->SetConsoleOutputCP(_initialCodePage.value()) && success;
    // }

    success = SetGraphicsRendition({}) && success; // Normal rendition.

    // // Reset the saved cursor state.
    // // Note that XTerm only resets the main buffer state, but that
    // // seems likely to be a bug. Most other terminals reset both.
    // _savedCursorState.at(0) = {}; // Main buffer
    // _savedCursorState.at(1) = {}; // Alt buffer

    return success;
}

bool TerminalDispatch::HardReset() noexcept
{
    // TODO:GH#1883 much of this method is not yet implemented in the Terminal,
    // because the Terminal _doesn't need to_ yet. The terminal is only ever
    // connected to conpty, so it doesn't implement most of these things that
    // Hard/Soft Reset would reset. As those things ar implemented, they should
    // also get cleared here.
    //
    // This code is left here (from its original form in conhost) as a reminder
    // of what needs to be done.

    bool success = true;

    // // If in the alt buffer, switch back to main before doing anything else.
    // if (_usingAltBuffer)
    // {
    //     success = _pConApi->PrivateUseMainScreenBuffer();
    //     _usingAltBuffer = !success;
    // }

    // Sets the SGR state to normal - this must be done before EraseInDisplay
    //      to ensure that it clears with the default background color.
    success = SoftReset() && success;

    // Clears the screen - Needs to be done in two operations.
    success = EraseInDisplay(DispatchTypes::EraseType::All) && success;
    success = EraseInDisplay(DispatchTypes::EraseType::Scrollback) && success;

    // Set the DECSCNM screen mode back to normal.
    success = SetScreenMode(false) && success;

    // Cursor to 1,1 - the Soft Reset guarantees this is absolute
    success = CursorPosition(1, 1) && success;

    // // Delete all current tab stops and reapply
    // _ResetTabStops();

    return success;
}
