// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <WexTestClass.h>

#include "../cascadia/TerminalCore/Terminal.hpp"
#include "../renderer/inc/DummyRenderTarget.hpp"
#include "consoletaeftemplates.hpp"

using namespace WEX::Logging;
using namespace WEX::TestExecution;

using namespace Microsoft::Terminal::Core;
using namespace Microsoft::Console::Render;

namespace TerminalCoreUnitTests
{
    class InputTest
    {
        TEST_CLASS(InputTest);
        TEST_CLASS_SETUP(ClassSetup)
        {
            DummyRenderTarget emptyRT;
            term.Create({ 100, 100 }, 0, emptyRT);
            auto inputFn = std::bind(&InputTest::_VerifyExpectedInput, this, std::placeholders::_1);
            term.SetWriteInputCallback(inputFn);
            return true;
        };

        TEST_METHOD(AltShiftKey);
        TEST_METHOD(AltSpace);
        TEST_METHOD(InvalidKeyEvent);

        void _VerifyExpectedInput(std::wstring& actualInput)
        {
            VERIFY_ARE_EQUAL(expectedinput.size(), actualInput.size());
            VERIFY_ARE_EQUAL(expectedinput, actualInput);
        };

        Terminal term{};
        std::wstring expectedinput{};
    };

    void InputTest::AltShiftKey()
    {
        // Tests GH:637

        // Verify that Alt+a generates a lowercase a on the input
        expectedinput = L"\x1b"
                        "a";
        VERIFY_IS_TRUE(term.SendCharEvent(L'a', 0, ControlKeyStates::LeftAltPressed));

        // Verify that Alt+shift+a generates a uppercase a on the input
        expectedinput = L"\x1b"
                        "A";
        VERIFY_IS_TRUE(term.SendCharEvent(L'A', 0, ControlKeyStates::LeftAltPressed | ControlKeyStates::ShiftPressed));
    }

    void InputTest::AltSpace()
    {
        // Make sure we don't handle Alt+Space. The system will use this to
        // bring up the system menu for restore, min/maximize, size, move,
        // close
        VERIFY_IS_FALSE(term.SendKeyEvent(L' ', 0, ControlKeyStates::LeftAltPressed, true));
        VERIFY_IS_FALSE(term.SendKeyEvent(L' ', 0, ControlKeyStates::LeftAltPressed, false));
        VERIFY_IS_FALSE(term.SendCharEvent(L' ', 0, ControlKeyStates::LeftAltPressed));
    }

    void InputTest::InvalidKeyEvent()
    {
        // Certain applications like AutoHotKey and its keyboard remapping feature,
        // send us key events using SendInput() whose values are outside of the valid range.
        // We don't want to handle those events as we're probably going to get a proper followup character event.
        VERIFY_IS_FALSE(term.SendKeyEvent(0, 123, {}, true));
        VERIFY_IS_FALSE(term.SendKeyEvent(255, 123, {}, true));
        VERIFY_IS_FALSE(term.SendKeyEvent(123, 0, {}, true));
    }
}
