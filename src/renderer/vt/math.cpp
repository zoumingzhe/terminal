// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "vtrenderer.hpp"

#pragma hdrstop

using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::Types;

// Routine Description:
// - Gets the size in characters of the current dirty portion of the frame.
// Arguments:
// - <none>
// Return Value:
// - The character dimensions of the current dirty area of the frame.
//      This is an Inclusive rect.
std::vector<til::rectangle> VtEngine::GetDirtyArea()
{
    return _invalidMap.runs();
}

// Routine Description:
// - Uses the currently selected font to determine how wide the given character will be when rendered.
// - NOTE: Only supports determining half-width/full-width status for CJK-type languages (e.g. is it 1 character wide or 2. a.k.a. is it a rectangle or square.)
// Arguments:
// - glyph - utf16 encoded codepoint to check
// - pResult - receives return value, True if it is full-width (2 wide). False if it is half-width (1 wide).
// Return Value:
// - S_FALSE: This is unsupported by the VT Renderer and should use another engine's value.
[[nodiscard]] HRESULT VtEngine::IsGlyphWideByFont(const std::wstring_view /*glyph*/, _Out_ bool* const pResult) noexcept
{
    *pResult = false;
    return S_FALSE;
}

// Routine Description:
// - Performs a "CombineRect" with the "OR" operation.
// - Basically extends the existing rect outward to also encompass the passed-in region.
// Arguments:
// - pRectExisting - Expand this rectangle to encompass the add rect.
// - pRectToOr - Add this rectangle to the existing one.
// Return Value:
// - <none>
void VtEngine::_OrRect(_Inout_ SMALL_RECT* const pRectExisting, const SMALL_RECT* const pRectToOr) const
{
    pRectExisting->Left = std::min(pRectExisting->Left, pRectToOr->Left);
    pRectExisting->Top = std::min(pRectExisting->Top, pRectToOr->Top);
    pRectExisting->Right = std::max(pRectExisting->Right, pRectToOr->Right);
    pRectExisting->Bottom = std::max(pRectExisting->Bottom, pRectToOr->Bottom);
}

// Method Description:
// - Returns true if the invalidated region indicates that we only need to
//      simply print text from the current cursor position. This will prevent us
//      from sending extra VT set-up/tear down sequences (?12h/l) when all we
//      need to do is print more text at the current cursor position.
// Arguments:
// - <none>
// Return Value:
// - true iff only the next character is invalid
bool VtEngine::_WillWriteSingleChar() const
{
    // If there is no scroll delta, return false.
    if (til::point{ 0, 0 } != _scrollDelta)
    {
        return false;
    }

    // If there is more than one invalid char, return false.
    if (!_invalidMap.one())
    {
        return false;
    }

    // Get the single point at which things are invalid.
    const auto invalidPoint = _invalidMap.runs().front().origin();

    // Either the next character to the right or the immediately previous
    //      character should follow this code path
    //      (The immediate previous character would suggest a backspace)
    bool invalidIsNext = invalidPoint == til::point{ _lastText };
    bool invalidIsLast = invalidPoint == til::point{ _lastText.X - 1, _lastText.Y };

    return invalidIsNext || invalidIsLast;
}
