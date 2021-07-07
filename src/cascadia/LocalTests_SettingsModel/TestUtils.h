/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- TestUtils.h

Abstract:
- This file has helper functions for writing tests for the TerminalApp project.

Author(s):
    Mike Griese (migrie) December-2019
--*/

class TestUtils
{
public:
    // Function Description:
    // - This is a helper to retrieve the ActionAndArgs from the keybindings
    //   for a given chord.
    // Arguments:
    // - keymap: The AppKeyBindings to lookup the ActionAndArgs from.
    // - kc: The key chord to look up the bound ActionAndArgs for.
    // Return Value:
    // - The ActionAndArgs bound to the given key, or nullptr if nothing is bound to it.
    static const winrt::Microsoft::Terminal::Settings::Model::ActionAndArgs GetActionAndArgs(const winrt::Microsoft::Terminal::Settings::Model::KeyMapping& keymap,
                                                                                             const winrt::Microsoft::Terminal::TerminalControl::KeyChord& kc)
    {
        std::wstring buffer{ L"" };
        if (WI_IsFlagSet(kc.Modifiers(), winrt::Microsoft::Terminal::TerminalControl::KeyModifiers::Ctrl))
        {
            buffer += L"Ctrl+";
        }
        if (WI_IsFlagSet(kc.Modifiers(), winrt::Microsoft::Terminal::TerminalControl::KeyModifiers::Shift))
        {
            buffer += L"Shift+";
        }
        if (WI_IsFlagSet(kc.Modifiers(), winrt::Microsoft::Terminal::TerminalControl::KeyModifiers::Alt))
        {
            buffer += L"Alt+";
        }
        buffer += static_cast<wchar_t>(MapVirtualKeyW(kc.Vkey(), MAPVK_VK_TO_CHAR));
        WEX::Logging::Log::Comment(WEX::Common::NoThrowString().Format(L"Looking for key:%s", buffer.c_str()));

        const auto action = keymap.TryLookup(kc);
        VERIFY_IS_NOT_NULL(action, L"Expected to find an action bound to the given KeyChord");
        return action;
    };
};
