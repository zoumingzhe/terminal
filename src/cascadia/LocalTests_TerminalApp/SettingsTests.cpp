// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "../TerminalApp/TerminalPage.h"
#include "../TerminalApp/TerminalSettings.h"
#include "../LocalTests_SettingsModel/TestUtils.h"

using namespace Microsoft::Console;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;
using namespace winrt::TerminalApp;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::TerminalControl;

namespace TerminalAppLocalTests
{
    // TODO:microsoft/terminal#3838:
    // Unfortunately, these tests _WILL NOT_ work in our CI. We're waiting for
    // an updated TAEF that will let us install framework packages when the test
    // package is deployed. Until then, these tests won't deploy in CI.

    class SettingsTests
    {
        // Use a custom AppxManifest to ensure that we can activate winrt types
        // from our test. This property will tell taef to manually use this as
        // the AppxManifest for this test class.
        // This does not yet work for anything XAML-y. See TabTests.cpp for more
        // details on that.
        BEGIN_TEST_CLASS(SettingsTests)
            TEST_CLASS_PROPERTY(L"RunAs", L"UAP")
            TEST_CLASS_PROPERTY(L"UAP:AppXManifest", L"TestHostAppXManifest.xml")
        END_TEST_CLASS()

        TEST_METHOD(TryCreateWinRTType);

        TEST_METHOD(TestTerminalArgsForBinding);

        TEST_METHOD(MakeSettingsForProfileThatDoesntExist);
        TEST_METHOD(MakeSettingsForDefaultProfileThatDoesntExist);

        TEST_METHOD(TestLayerProfileOnColorScheme);

        TEST_METHOD(TestIterateCommands);
        TEST_METHOD(TestIterateOnGeneratedNamedCommands);
        TEST_METHOD(TestIterateOnBadJson);
        TEST_METHOD(TestNestedCommands);
        TEST_METHOD(TestNestedInNestedCommand);
        TEST_METHOD(TestNestedInIterableCommand);
        TEST_METHOD(TestIterableInNestedCommand);
        TEST_METHOD(TestMixedNestedAndIterableCommand);

        TEST_METHOD(TestIterableColorSchemeCommands);

        TEST_CLASS_SETUP(ClassSetup)
        {
            return true;
        }

    private:
        void _logCommandNames(winrt::Windows::Foundation::Collections::IMapView<winrt::hstring, Command> commands, const int indentation = 1)
        {
            if (indentation == 1)
            {
                Log::Comment((commands.Size() == 0) ? L"Commands:\n  <none>" : L"Commands:");
            }
            for (const auto& nameAndCommand : commands)
            {
                Log::Comment(fmt::format(L"{0:>{1}}* {2}->{3}",
                                         L"",
                                         indentation,
                                         nameAndCommand.Key(),
                                         nameAndCommand.Value().Name())
                                 .c_str());

                if (nameAndCommand.Value().HasNestedCommands())
                {
                    _logCommandNames(nameAndCommand.Value().NestedCommands(), indentation + 2);
                }
            }
        }
    };

    void SettingsTests::TryCreateWinRTType()
    {
        TerminalSettings settings;
        VERIFY_IS_NOT_NULL(settings);
        auto oldFontSize = settings.FontSize();
        settings.FontSize(oldFontSize + 5);
        auto newFontSize = settings.FontSize();
        VERIFY_ARE_NOT_EQUAL(oldFontSize, newFontSize);
    }

    void SettingsTests::TestTerminalArgsForBinding()
    {
        const std::string settingsJson{ R"(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "profiles": [
                {
                    "name": "profile0",
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                    "historySize": 1,
                    "commandline": "cmd.exe"
                },
                {
                    "name": "profile1",
                    "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                    "historySize": 2,
                    "commandline": "pwsh.exe"
                },
                {
                    "name": "profile2",
                    "historySize": 3,
                    "commandline": "wsl.exe"
                }
            ],
            "keybindings": [
                { "keys": ["ctrl+a"], "command": { "action": "splitPane", "split": "vertical" } },
                { "keys": ["ctrl+b"], "command": { "action": "splitPane", "split": "vertical", "profile": "{6239a42c-1111-49a3-80bd-e8fdd045185c}" } },
                { "keys": ["ctrl+c"], "command": { "action": "splitPane", "split": "vertical", "profile": "profile1" } },
                { "keys": ["ctrl+d"], "command": { "action": "splitPane", "split": "vertical", "profile": "profile2" } },
                { "keys": ["ctrl+e"], "command": { "action": "splitPane", "split": "horizontal", "commandline": "foo.exe" } },
                { "keys": ["ctrl+f"], "command": { "action": "splitPane", "split": "horizontal", "profile": "profile1", "commandline": "foo.exe" } },
                { "keys": ["ctrl+g"], "command": { "action": "newTab" } },
                { "keys": ["ctrl+h"], "command": { "action": "newTab", "startingDirectory": "c:\\foo" } },
                { "keys": ["ctrl+i"], "command": { "action": "newTab", "profile": "profile2", "startingDirectory": "c:\\foo" } },
                { "keys": ["ctrl+j"], "command": { "action": "newTab", "tabTitle": "bar" } },
                { "keys": ["ctrl+k"], "command": { "action": "newTab", "profile": "profile2", "tabTitle": "bar" } },
                { "keys": ["ctrl+l"], "command": { "action": "newTab", "profile": "profile1", "tabTitle": "bar", "startingDirectory": "c:\\foo", "commandline":"foo.exe" } }
            ]
        })" };

        const winrt::guid guid0{ ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-0000-49a3-80bd-e8fdd045185c}") };
        const winrt::guid guid1{ ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-1111-49a3-80bd-e8fdd045185c}") };

        CascadiaSettings settings{ til::u8u16(settingsJson) };

        auto keymap = settings.GlobalSettings().KeyMap();
        VERIFY_ARE_EQUAL(3u, settings.Profiles().Size());

        const auto profile2Guid = settings.Profiles().GetAt(2).Guid();
        VERIFY_ARE_NOT_EQUAL(winrt::guid{}, profile2Guid);

        VERIFY_ARE_EQUAL(12u, keymap.Size());

        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('A') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(keymap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitState::Vertical, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Profile().empty());

            const auto [guid, termSettings] = winrt::TerminalApp::implementation::TerminalSettings::BuildSettings(settings, realArgs.TerminalArgs(), nullptr);
            VERIFY_ARE_EQUAL(guid0, guid);
            VERIFY_ARE_EQUAL(L"cmd.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(1, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('B') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(keymap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitState::Vertical, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"{6239a42c-1111-49a3-80bd-e8fdd045185c}", realArgs.TerminalArgs().Profile());

            const auto [guid, termSettings] = winrt::TerminalApp::implementation::TerminalSettings::BuildSettings(settings, realArgs.TerminalArgs(), nullptr);
            VERIFY_ARE_EQUAL(guid1, guid);
            VERIFY_ARE_EQUAL(L"pwsh.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(2, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('C') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(keymap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitState::Vertical, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile1", realArgs.TerminalArgs().Profile());

            const auto [guid, termSettings] = winrt::TerminalApp::implementation::TerminalSettings::BuildSettings(settings, realArgs.TerminalArgs(), nullptr);
            VERIFY_ARE_EQUAL(guid1, guid);
            VERIFY_ARE_EQUAL(L"pwsh.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(2, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('D') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(keymap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitState::Vertical, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile2", realArgs.TerminalArgs().Profile());

            const auto [guid, termSettings] = winrt::TerminalApp::implementation::TerminalSettings::BuildSettings(settings, realArgs.TerminalArgs(), nullptr);
            VERIFY_ARE_EQUAL(profile2Guid, guid);
            VERIFY_ARE_EQUAL(L"wsl.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(3, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('E') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(keymap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitState::Horizontal, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"foo.exe", realArgs.TerminalArgs().Commandline());

            const auto [guid, termSettings] = winrt::TerminalApp::implementation::TerminalSettings::BuildSettings(settings, realArgs.TerminalArgs(), nullptr);
            VERIFY_ARE_EQUAL(guid0, guid);
            VERIFY_ARE_EQUAL(L"foo.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(1, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('F') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(keymap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitState::Horizontal, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile1", realArgs.TerminalArgs().Profile());
            VERIFY_ARE_EQUAL(L"foo.exe", realArgs.TerminalArgs().Commandline());

            const auto [guid, termSettings] = winrt::TerminalApp::implementation::TerminalSettings::BuildSettings(settings, realArgs.TerminalArgs(), nullptr);
            VERIFY_ARE_EQUAL(guid1, guid);
            VERIFY_ARE_EQUAL(L"foo.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(2, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('G') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(keymap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Profile().empty());

            const auto [guid, termSettings] = winrt::TerminalApp::implementation::TerminalSettings::BuildSettings(settings, realArgs.TerminalArgs(), nullptr);
            VERIFY_ARE_EQUAL(guid0, guid);
            VERIFY_ARE_EQUAL(L"cmd.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(1, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('H') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(keymap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"c:\\foo", realArgs.TerminalArgs().StartingDirectory());

            const auto [guid, termSettings] = winrt::TerminalApp::implementation::TerminalSettings::BuildSettings(settings, realArgs.TerminalArgs(), nullptr);
            VERIFY_ARE_EQUAL(guid0, guid);
            VERIFY_ARE_EQUAL(L"cmd.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(L"c:\\foo", termSettings.StartingDirectory());
            VERIFY_ARE_EQUAL(1, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('I') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(keymap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"c:\\foo", realArgs.TerminalArgs().StartingDirectory());
            VERIFY_ARE_EQUAL(L"profile2", realArgs.TerminalArgs().Profile());

            const auto [guid, termSettings] = winrt::TerminalApp::implementation::TerminalSettings::BuildSettings(settings, realArgs.TerminalArgs(), nullptr);
            VERIFY_ARE_EQUAL(profile2Guid, guid);
            VERIFY_ARE_EQUAL(L"wsl.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(L"c:\\foo", termSettings.StartingDirectory());
            VERIFY_ARE_EQUAL(3, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('J') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(keymap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"bar", realArgs.TerminalArgs().TabTitle());

            const auto [guid, termSettings] = winrt::TerminalApp::implementation::TerminalSettings::BuildSettings(settings, realArgs.TerminalArgs(), nullptr);
            VERIFY_ARE_EQUAL(guid0, guid);
            VERIFY_ARE_EQUAL(L"cmd.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(L"bar", termSettings.StartingTitle());
            VERIFY_ARE_EQUAL(1, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('K') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(keymap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"bar", realArgs.TerminalArgs().TabTitle());
            VERIFY_ARE_EQUAL(L"profile2", realArgs.TerminalArgs().Profile());

            const auto [guid, termSettings] = winrt::TerminalApp::implementation::TerminalSettings::BuildSettings(settings, realArgs.TerminalArgs(), nullptr);
            VERIFY_ARE_EQUAL(profile2Guid, guid);
            VERIFY_ARE_EQUAL(L"wsl.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(L"bar", termSettings.StartingTitle());
            VERIFY_ARE_EQUAL(3, termSettings.HistorySize());
        }
        {
            KeyChord kc{ true, false, false, static_cast<int32_t>('L') };
            auto actionAndArgs = TestUtils::GetActionAndArgs(keymap, kc);
            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"foo.exe", realArgs.TerminalArgs().Commandline());
            VERIFY_ARE_EQUAL(L"c:\\foo", realArgs.TerminalArgs().StartingDirectory());
            VERIFY_ARE_EQUAL(L"bar", realArgs.TerminalArgs().TabTitle());
            VERIFY_ARE_EQUAL(L"profile1", realArgs.TerminalArgs().Profile());

            const auto [guid, termSettings] = winrt::TerminalApp::implementation::TerminalSettings::BuildSettings(settings, realArgs.TerminalArgs(), nullptr);
            VERIFY_ARE_EQUAL(guid1, guid);
            VERIFY_ARE_EQUAL(L"foo.exe", termSettings.Commandline());
            VERIFY_ARE_EQUAL(L"bar", termSettings.StartingTitle());
            VERIFY_ARE_EQUAL(L"c:\\foo", termSettings.StartingDirectory());
            VERIFY_ARE_EQUAL(2, termSettings.HistorySize());
        }
    }

    void SettingsTests::MakeSettingsForProfileThatDoesntExist()
    {
        // Test that MakeSettings throws when the GUID doesn't exist
        const std::string settingsString{ R"(
        {
            "defaultProfile": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
            "profiles": [
                {
                    "name" : "profile0",
                    "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                    "historySize": 1
                },
                {
                    "name" : "profile1",
                    "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}",
                    "historySize": 2
                }
            ]
        })" };
        CascadiaSettings settings{ til::u8u16(settingsString) };

        const auto guid1 = ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-1111-49a3-80bd-e8fdd045185c}");
        const auto guid2 = ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-2222-49a3-80bd-e8fdd045185c}");
        const auto guid3 = ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-3333-49a3-80bd-e8fdd045185c}");

        try
        {
            auto terminalSettings = winrt::make<winrt::TerminalApp::implementation::TerminalSettings>(settings, guid1, nullptr);
            VERIFY_ARE_NOT_EQUAL(nullptr, terminalSettings);
            VERIFY_ARE_EQUAL(1, terminalSettings.HistorySize());
        }
        catch (...)
        {
            VERIFY_IS_TRUE(false, L"This call to BuildSettings should succeed");
        }

        try
        {
            auto terminalSettings = winrt::make<winrt::TerminalApp::implementation::TerminalSettings>(settings, guid2, nullptr);
            VERIFY_ARE_NOT_EQUAL(nullptr, terminalSettings);
            VERIFY_ARE_EQUAL(2, terminalSettings.HistorySize());
        }
        catch (...)
        {
            VERIFY_IS_TRUE(false, L"This call to BuildSettings should succeed");
        }

        VERIFY_THROWS(auto terminalSettings = winrt::make<winrt::TerminalApp::implementation::TerminalSettings>(settings, guid3, nullptr), wil::ResultException, L"This call to BuildSettings should fail");

        try
        {
            const auto [guid, termSettings] = winrt::TerminalApp::implementation::TerminalSettings::BuildSettings(settings, nullptr, nullptr);
            VERIFY_ARE_NOT_EQUAL(nullptr, termSettings);
            VERIFY_ARE_EQUAL(1, termSettings.HistorySize());
        }
        catch (...)
        {
            VERIFY_IS_TRUE(false, L"This call to BuildSettings should succeed");
        }
    }

    void SettingsTests::MakeSettingsForDefaultProfileThatDoesntExist()
    {
        // Test that MakeSettings _doesnt_ throw when we load settings with a
        // defaultProfile that's not in the list, we validate the settings, and
        // then call MakeSettings(nullopt). The validation should ensure that
        // the default profile is something reasonable
        const std::string settingsString{ R"(
        {
            "defaultProfile": "{6239a42c-3333-49a3-80bd-e8fdd045185c}",
            "profiles": [
                {
                    "name" : "profile0",
                    "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                    "historySize": 1
                },
                {
                    "name" : "profile1",
                    "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}",
                    "historySize": 2
                }
            ]
        })" };
        CascadiaSettings settings{ til::u8u16(settingsString) };

        VERIFY_ARE_EQUAL(2u, settings.Warnings().Size());
        VERIFY_ARE_EQUAL(2u, settings.Profiles().Size());
        VERIFY_ARE_EQUAL(settings.GlobalSettings().DefaultProfile(), settings.Profiles().GetAt(0).Guid());
        try
        {
            const auto [guid, termSettings] = winrt::TerminalApp::implementation::TerminalSettings::BuildSettings(settings, nullptr, nullptr);
            VERIFY_ARE_NOT_EQUAL(nullptr, termSettings);
            VERIFY_ARE_EQUAL(1, termSettings.HistorySize());
        }
        catch (...)
        {
            VERIFY_IS_TRUE(false, L"This call to BuildSettings should succeed");
        }
    }

    void SettingsTests::TestLayerProfileOnColorScheme()
    {
        Log::Comment(NoThrowString().Format(
            L"Ensure that setting (or not) a property in the profile that should override a property of the color scheme works correctly."));

        const std::string settings0String{ R"(
        {
            "profiles": [
                {
                    "name" : "profile0",
                    "colorScheme": "schemeWithCursorColor"
                },
                {
                    "name" : "profile1",
                    "colorScheme": "schemeWithoutCursorColor"
                },
                {
                    "name" : "profile2",
                    "colorScheme": "schemeWithCursorColor",
                    "cursorColor": "#234567"
                },
                {
                    "name" : "profile3",
                    "colorScheme": "schemeWithoutCursorColor",
                    "cursorColor": "#345678"
                },
                {
                    "name" : "profile4",
                    "cursorColor": "#456789"
                },
                {
                    "name" : "profile5"
                }
            ],
            "schemes": [
                {
                    "name": "schemeWithCursorColor",
                    "cursorColor": "#123456"
                },
                {
                    "name": "schemeWithoutCursorColor"
                }
            ]
        })" };

        CascadiaSettings settings{ til::u8u16(settings0String) };

        VERIFY_ARE_EQUAL(6u, settings.Profiles().Size());
        VERIFY_ARE_EQUAL(2u, settings.GlobalSettings().ColorSchemes().Size());

        auto createTerminalSettings = [&](const auto& profile, const auto& schemes) {
            auto terminalSettings{ winrt::make_self<winrt::TerminalApp::implementation::TerminalSettings>() };
            terminalSettings->_ApplyProfileSettings(profile, schemes);
            return terminalSettings;
        };

        auto terminalSettings0 = createTerminalSettings(settings.Profiles().GetAt(0), settings.GlobalSettings().ColorSchemes());
        auto terminalSettings1 = createTerminalSettings(settings.Profiles().GetAt(1), settings.GlobalSettings().ColorSchemes());
        auto terminalSettings2 = createTerminalSettings(settings.Profiles().GetAt(2), settings.GlobalSettings().ColorSchemes());
        auto terminalSettings3 = createTerminalSettings(settings.Profiles().GetAt(3), settings.GlobalSettings().ColorSchemes());
        auto terminalSettings4 = createTerminalSettings(settings.Profiles().GetAt(4), settings.GlobalSettings().ColorSchemes());
        auto terminalSettings5 = createTerminalSettings(settings.Profiles().GetAt(5), settings.GlobalSettings().ColorSchemes());

        VERIFY_ARE_EQUAL(ARGB(0, 0x12, 0x34, 0x56), terminalSettings0->CursorColor()); // from color scheme
        VERIFY_ARE_EQUAL(DEFAULT_CURSOR_COLOR, terminalSettings1->CursorColor()); // default
        VERIFY_ARE_EQUAL(ARGB(0, 0x23, 0x45, 0x67), terminalSettings2->CursorColor()); // from profile (trumps color scheme)
        VERIFY_ARE_EQUAL(ARGB(0, 0x34, 0x56, 0x78), terminalSettings3->CursorColor()); // from profile (not set in color scheme)
        VERIFY_ARE_EQUAL(ARGB(0, 0x45, 0x67, 0x89), terminalSettings4->CursorColor()); // from profile (no color scheme)
        VERIFY_ARE_EQUAL(DEFAULT_CURSOR_COLOR, terminalSettings5->CursorColor()); // default
    }

    void SettingsTests::TestIterateCommands()
    {
        // For this test, put an iterable command with a given `name`,
        // containing a ${profile.name} to replace. When we expand it, it should
        // have created one command for each profile.

        const std::string settingsJson{ R"(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "profiles": [
                {
                    "name": "profile0",
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                    "historySize": 1,
                    "commandline": "cmd.exe"
                },
                {
                    "name": "profile1",
                    "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                    "historySize": 2,
                    "commandline": "pwsh.exe"
                },
                {
                    "name": "profile2",
                    "historySize": 3,
                    "commandline": "wsl.exe"
                }
            ],
            "actions": [
                {
                    "name": "iterable command ${profile.name}",
                    "iterateOn": "profiles",
                    "command": { "action": "splitPane", "profile": "${profile.name}" }
                },
            ],
            "schemes": [ { "name": "Campbell" } ] // This is included here to prevent settings validation errors.
        })" };

        const auto guid0 = ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-0000-49a3-80bd-e8fdd045185c}");
        const auto guid1 = ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-1111-49a3-80bd-e8fdd045185c}");

        CascadiaSettings settings{ til::u8u16(settingsJson) };

        VERIFY_ARE_EQUAL(0u, settings.Warnings().Size());

        VERIFY_ARE_EQUAL(3u, settings.Profiles().Size());

        auto commands = settings.GlobalSettings().Commands();
        VERIFY_ARE_EQUAL(1u, commands.Size());

        {
            auto command = commands.Lookup(L"iterable command ${profile.name}");
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitState::Automatic, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"${profile.name}", realArgs.TerminalArgs().Profile());
        }

        auto expandedCommands = winrt::TerminalApp::implementation::TerminalPage::_ExpandCommands(commands, settings.Profiles().GetView(), settings.GlobalSettings().ColorSchemes());
        _logCommandNames(expandedCommands.GetView());

        VERIFY_ARE_EQUAL(0u, settings.Warnings().Size());
        VERIFY_ARE_EQUAL(3u, expandedCommands.Size());

        {
            auto command = expandedCommands.Lookup(L"iterable command profile0");
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitState::Automatic, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile0", realArgs.TerminalArgs().Profile());
        }

        {
            auto command = expandedCommands.Lookup(L"iterable command profile1");
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitState::Automatic, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile1", realArgs.TerminalArgs().Profile());
        }

        {
            auto command = expandedCommands.Lookup(L"iterable command profile2");
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitState::Automatic, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile2", realArgs.TerminalArgs().Profile());
        }
    }

    void SettingsTests::TestIterateOnGeneratedNamedCommands()
    {
        // For this test, put an iterable command without a given `name` to
        // replace. When we expand it, it should still work.

        const std::string settingsJson{ R"(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "profiles": [
                {
                    "name": "profile0",
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                    "historySize": 1,
                    "commandline": "cmd.exe"
                },
                {
                    "name": "profile1",
                    "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                    "historySize": 2,
                    "commandline": "pwsh.exe"
                },
                {
                    "name": "profile2",
                    "historySize": 3,
                    "commandline": "wsl.exe"
                }
            ],
            "actions": [
                {
                    "iterateOn": "profiles",
                    "command": { "action": "splitPane", "profile": "${profile.name}" }
                },
            ],
            "schemes": [ { "name": "Campbell" } ] // This is included here to prevent settings validation errors.
        })" };

        const auto guid0 = ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-0000-49a3-80bd-e8fdd045185c}");
        const auto guid1 = ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-1111-49a3-80bd-e8fdd045185c}");

        CascadiaSettings settings{ til::u8u16(settingsJson) };

        VERIFY_ARE_EQUAL(0u, settings.Warnings().Size());

        VERIFY_ARE_EQUAL(3u, settings.Profiles().Size());

        auto commands = settings.GlobalSettings().Commands();
        VERIFY_ARE_EQUAL(1u, commands.Size());

        {
            auto command = commands.Lookup(L"Split pane, profile: ${profile.name}");
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitState::Automatic, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"${profile.name}", realArgs.TerminalArgs().Profile());
        }

        auto expandedCommands = winrt::TerminalApp::implementation::TerminalPage::_ExpandCommands(commands, settings.Profiles().GetView(), settings.GlobalSettings().ColorSchemes());
        _logCommandNames(expandedCommands.GetView());

        VERIFY_ARE_EQUAL(0u, settings.Warnings().Size());
        VERIFY_ARE_EQUAL(3u, expandedCommands.Size());

        {
            auto command = expandedCommands.Lookup(L"Split pane, profile: profile0");
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitState::Automatic, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile0", realArgs.TerminalArgs().Profile());
        }

        {
            auto command = expandedCommands.Lookup(L"Split pane, profile: profile1");
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitState::Automatic, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile1", realArgs.TerminalArgs().Profile());
        }

        {
            auto command = expandedCommands.Lookup(L"Split pane, profile: profile2");
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitState::Automatic, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile2", realArgs.TerminalArgs().Profile());
        }
    }

    void SettingsTests::TestIterateOnBadJson()
    {
        // For this test, put an iterable command with a profile name that would
        // cause bad json to be filled in. Something like a profile with a name
        // of "Foo\"", so the trailing '"' might break the json parsing.

        const std::string settingsJson{ R"(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "profiles": [
                {
                    "name": "profile0",
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                    "historySize": 1,
                    "commandline": "cmd.exe"
                },
                {
                    "name": "profile1\"",
                    "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                    "historySize": 2,
                    "commandline": "pwsh.exe"
                },
                {
                    "name": "profile2",
                    "historySize": 3,
                    "commandline": "wsl.exe"
                }
            ],
            "actions": [
                {
                    "name": "iterable command ${profile.name}",
                    "iterateOn": "profiles",
                    "command": { "action": "splitPane", "profile": "${profile.name}" }
                },
            ],
            "schemes": [ { "name": "Campbell" } ] // This is included here to prevent settings validation errors.
        })" };

        const auto guid0 = ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-0000-49a3-80bd-e8fdd045185c}");
        const auto guid1 = ::Microsoft::Console::Utils::GuidFromString(L"{6239a42c-1111-49a3-80bd-e8fdd045185c}");

        CascadiaSettings settings{ til::u8u16(settingsJson) };

        VERIFY_ARE_EQUAL(0u, settings.Warnings().Size());

        VERIFY_ARE_EQUAL(3u, settings.Profiles().Size());

        auto commands = settings.GlobalSettings().Commands();
        VERIFY_ARE_EQUAL(1u, commands.Size());

        {
            auto command = commands.Lookup(L"iterable command ${profile.name}");
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitState::Automatic, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"${profile.name}", realArgs.TerminalArgs().Profile());
        }

        auto expandedCommands = winrt::TerminalApp::implementation::TerminalPage::_ExpandCommands(commands, settings.Profiles().GetView(), settings.GlobalSettings().ColorSchemes());
        _logCommandNames(expandedCommands.GetView());

        VERIFY_ARE_EQUAL(0u, settings.Warnings().Size());
        VERIFY_ARE_EQUAL(3u, expandedCommands.Size());

        {
            auto command = expandedCommands.Lookup(L"iterable command profile0");
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitState::Automatic, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile0", realArgs.TerminalArgs().Profile());
        }

        {
            auto command = expandedCommands.Lookup(L"iterable command profile1\"");
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitState::Automatic, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile1\"", realArgs.TerminalArgs().Profile());
        }

        {
            auto command = expandedCommands.Lookup(L"iterable command profile2");
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitState::Automatic, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"profile2", realArgs.TerminalArgs().Profile());
        }
    }

    void SettingsTests::TestNestedCommands()
    {
        // This test checks a nested command.
        // The commands should look like:
        //
        // <Command Palette>
        // └─ Connect to ssh...
        //    ├─ first.com
        //    └─ second.com

        const std::string settingsJson{ R"(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "profiles": [
                {
                    "name": "profile0",
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                    "historySize": 1,
                    "commandline": "cmd.exe"
                },
                {
                    "name": "profile1",
                    "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                    "historySize": 2,
                    "commandline": "pwsh.exe"
                },
                {
                    "name": "profile2",
                    "historySize": 3,
                    "commandline": "wsl.exe"
                }
            ],
            "actions": [
                {
                    "name": "Connect to ssh...",
                    "commands": [
                        {
                            "name": "first.com",
                            "command": { "action": "newTab", "commandline": "ssh me@first.com" }
                        },
                        {
                            "name": "second.com",
                            "command": { "action": "newTab", "commandline": "ssh me@second.com" }
                        }
                    ]
                },
            ],
            "schemes": [ { "name": "Campbell" } ] // This is included here to prevent settings validation errors.
        })" };

        CascadiaSettings settings{ til::u8u16(settingsJson) };

        VERIFY_ARE_EQUAL(0u, settings.Warnings().Size());
        VERIFY_ARE_EQUAL(3u, settings.Profiles().Size());

        auto commands = settings.GlobalSettings().Commands();
        auto expandedCommands = winrt::TerminalApp::implementation::TerminalPage::_ExpandCommands(commands, settings.Profiles().GetView(), settings.GlobalSettings().ColorSchemes());
        _logCommandNames(expandedCommands.GetView());

        VERIFY_ARE_EQUAL(0u, settings.Warnings().Size());
        VERIFY_ARE_EQUAL(1u, expandedCommands.Size());

        auto rootCommand = expandedCommands.Lookup(L"Connect to ssh...");
        VERIFY_IS_NOT_NULL(rootCommand);
        auto rootActionAndArgs = rootCommand.Action();
        VERIFY_IS_NULL(rootActionAndArgs);

        VERIFY_ARE_EQUAL(2u, rootCommand.NestedCommands().Size());

        {
            winrt::hstring commandName{ L"first.com" };
            auto command = rootCommand.NestedCommands().Lookup(commandName);
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);

            VERIFY_IS_FALSE(command.HasNestedCommands());
        }
        {
            winrt::hstring commandName{ L"second.com" };
            auto command = rootCommand.NestedCommands().Lookup(commandName);
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);

            VERIFY_IS_FALSE(command.HasNestedCommands());
        }
    }

    void SettingsTests::TestNestedInNestedCommand()
    {
        // This test checks a nested command that includes nested commands.
        // The commands should look like:
        //
        // <Command Palette>
        // └─ grandparent
        //    └─ parent
        //       ├─ child1
        //       └─ child2

        const std::string settingsJson{ R"(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "profiles": [
                {
                    "name": "profile0",
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                    "historySize": 1,
                    "commandline": "cmd.exe"
                },
                {
                    "name": "profile1",
                    "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                    "historySize": 2,
                    "commandline": "pwsh.exe"
                },
                {
                    "name": "profile2",
                    "historySize": 3,
                    "commandline": "wsl.exe"
                }
            ],
            "actions": [
                {
                    "name": "grandparent",
                    "commands": [
                        {
                            "name": "parent",
                            "commands": [
                                {
                                    "name": "child1",
                                    "command": { "action": "newTab", "commandline": "ssh me@first.com" }
                                },
                                {
                                    "name": "child2",
                                    "command": { "action": "newTab", "commandline": "ssh me@second.com" }
                                }
                            ]
                        },
                    ]
                },
            ],
            "schemes": [ { "name": "Campbell" } ] // This is included here to prevent settings validation errors.
        })" };

        CascadiaSettings settings{ til::u8u16(settingsJson) };

        VERIFY_ARE_EQUAL(0u, settings.Warnings().Size());
        VERIFY_ARE_EQUAL(3u, settings.Profiles().Size());

        auto commands = settings.GlobalSettings().Commands();
        auto expandedCommands = winrt::TerminalApp::implementation::TerminalPage::_ExpandCommands(commands, settings.Profiles().GetView(), settings.GlobalSettings().ColorSchemes());
        _logCommandNames(expandedCommands.GetView());

        VERIFY_ARE_EQUAL(0u, settings.Warnings().Size());
        VERIFY_ARE_EQUAL(1u, expandedCommands.Size());

        auto grandparentCommand = expandedCommands.Lookup(L"grandparent");
        VERIFY_IS_NOT_NULL(grandparentCommand);
        auto grandparentActionAndArgs = grandparentCommand.Action();
        VERIFY_IS_NULL(grandparentActionAndArgs);

        VERIFY_ARE_EQUAL(1u, grandparentCommand.NestedCommands().Size());

        winrt::hstring parentName{ L"parent" };
        auto parent = grandparentCommand.NestedCommands().Lookup(parentName);
        VERIFY_IS_NOT_NULL(parent);
        auto parentActionAndArgs = parent.Action();
        VERIFY_IS_NULL(parentActionAndArgs);

        VERIFY_ARE_EQUAL(2u, parent.NestedCommands().Size());
        {
            winrt::hstring childName{ L"child1" };
            auto child = parent.NestedCommands().Lookup(childName);
            VERIFY_IS_NOT_NULL(child);
            auto childActionAndArgs = child.Action();
            VERIFY_IS_NOT_NULL(childActionAndArgs);

            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, childActionAndArgs.Action());
            const auto& realArgs = childActionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"ssh me@first.com", realArgs.TerminalArgs().Commandline());

            VERIFY_IS_FALSE(child.HasNestedCommands());
        }
        {
            winrt::hstring childName{ L"child2" };
            auto child = parent.NestedCommands().Lookup(childName);
            VERIFY_IS_NOT_NULL(child);
            auto childActionAndArgs = child.Action();
            VERIFY_IS_NOT_NULL(childActionAndArgs);

            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, childActionAndArgs.Action());
            const auto& realArgs = childActionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"ssh me@second.com", realArgs.TerminalArgs().Commandline());

            VERIFY_IS_FALSE(child.HasNestedCommands());
        }
    }

    void SettingsTests::TestNestedInIterableCommand()
    {
        // This test checks a iterable command that includes a nested command.
        // The commands should look like:
        //
        // <Command Palette>
        //  ├─ profile0...
        //  |  ├─ Split pane, profile: profile0
        //  |  ├─ Split pane, direction: vertical, profile: profile0
        //  |  └─ Split pane, direction: horizontal, profile: profile0
        //  ├─ profile1...
        //  |  ├─Split pane, profile: profile1
        //  |  ├─Split pane, direction: vertical, profile: profile1
        //  |  └─Split pane, direction: horizontal, profile: profile1
        //  └─ profile2...
        //     ├─ Split pane, profile: profile2
        //     ├─ Split pane, direction: vertical, profile: profile2
        //     └─ Split pane, direction: horizontal, profile: profile2

        const std::string settingsJson{ R"(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "profiles": [
                {
                    "name": "profile0",
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                    "historySize": 1,
                    "commandline": "cmd.exe"
                },
                {
                    "name": "profile1",
                    "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                    "historySize": 2,
                    "commandline": "pwsh.exe"
                },
                {
                    "name": "profile2",
                    "historySize": 3,
                    "commandline": "wsl.exe"
                }
            ],
            "actions": [
                {
                    "iterateOn": "profiles",
                    "name": "${profile.name}...",
                    "commands": [
                        { "command": { "action": "splitPane", "profile": "${profile.name}", "split": "auto" } },
                        { "command": { "action": "splitPane", "profile": "${profile.name}", "split": "vertical" } },
                        { "command": { "action": "splitPane", "profile": "${profile.name}", "split": "horizontal" } }
                    ]
                }
            ],
            "schemes": [ { "name": "Campbell" } ] // This is included here to prevent settings validation errors.
        })" };

        CascadiaSettings settings{ til::u8u16(settingsJson) };

        VERIFY_ARE_EQUAL(0u, settings.Warnings().Size());
        VERIFY_ARE_EQUAL(3u, settings.Profiles().Size());

        auto commands = settings.GlobalSettings().Commands();
        auto expandedCommands = winrt::TerminalApp::implementation::TerminalPage::_ExpandCommands(commands, settings.Profiles().GetView(), settings.GlobalSettings().ColorSchemes());
        _logCommandNames(expandedCommands.GetView());

        VERIFY_ARE_EQUAL(0u, settings.Warnings().Size());

        VERIFY_ARE_EQUAL(3u, expandedCommands.Size());

        for (auto name : std::vector<std::wstring>({ L"profile0", L"profile1", L"profile2" }))
        {
            winrt::hstring commandName{ name + L"..." };
            auto command = expandedCommands.Lookup(commandName);
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.Action();
            VERIFY_IS_NULL(actionAndArgs);

            VERIFY_IS_TRUE(command.HasNestedCommands());
            VERIFY_ARE_EQUAL(3u, command.NestedCommands().Size());
            _logCommandNames(command.NestedCommands());
            {
                winrt::hstring childCommandName{ fmt::format(L"Split pane, profile: {}", name) };
                auto childCommand = command.NestedCommands().Lookup(childCommandName);
                VERIFY_IS_NOT_NULL(childCommand);
                auto childActionAndArgs = childCommand.Action();
                VERIFY_IS_NOT_NULL(childActionAndArgs);

                VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, childActionAndArgs.Action());
                const auto& realArgs = childActionAndArgs.Args().try_as<SplitPaneArgs>();
                VERIFY_IS_NOT_NULL(realArgs);
                // Verify the args have the expected value
                VERIFY_ARE_EQUAL(SplitState::Automatic, realArgs.SplitStyle());
                VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
                VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
                VERIFY_ARE_EQUAL(name, realArgs.TerminalArgs().Profile());

                VERIFY_IS_FALSE(childCommand.HasNestedCommands());
            }
            {
                winrt::hstring childCommandName{ fmt::format(L"Split pane, split: horizontal, profile: {}", name) };
                auto childCommand = command.NestedCommands().Lookup(childCommandName);
                VERIFY_IS_NOT_NULL(childCommand);
                auto childActionAndArgs = childCommand.Action();
                VERIFY_IS_NOT_NULL(childActionAndArgs);

                VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, childActionAndArgs.Action());
                const auto& realArgs = childActionAndArgs.Args().try_as<SplitPaneArgs>();
                VERIFY_IS_NOT_NULL(realArgs);
                // Verify the args have the expected value
                VERIFY_ARE_EQUAL(SplitState::Horizontal, realArgs.SplitStyle());
                VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
                VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
                VERIFY_ARE_EQUAL(name, realArgs.TerminalArgs().Profile());

                VERIFY_IS_FALSE(childCommand.HasNestedCommands());
            }
            {
                winrt::hstring childCommandName{ fmt::format(L"Split pane, split: vertical, profile: {}", name) };
                auto childCommand = command.NestedCommands().Lookup(childCommandName);
                VERIFY_IS_NOT_NULL(childCommand);
                auto childActionAndArgs = childCommand.Action();
                VERIFY_IS_NOT_NULL(childActionAndArgs);

                VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, childActionAndArgs.Action());
                const auto& realArgs = childActionAndArgs.Args().try_as<SplitPaneArgs>();
                VERIFY_IS_NOT_NULL(realArgs);
                // Verify the args have the expected value
                VERIFY_ARE_EQUAL(SplitState::Vertical, realArgs.SplitStyle());
                VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
                VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
                VERIFY_ARE_EQUAL(name, realArgs.TerminalArgs().Profile());

                VERIFY_IS_FALSE(childCommand.HasNestedCommands());
            }
        }
    }

    void SettingsTests::TestIterableInNestedCommand()
    {
        // This test checks a nested command that includes an iterable command.
        // The commands should look like:
        //
        // <Command Palette>
        // └─ New Tab With Profile...
        //    ├─ Profile 1
        //    ├─ Profile 2
        //    └─ Profile 3

        const std::string settingsJson{ R"(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "profiles": [
                {
                    "name": "profile0",
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                    "historySize": 1,
                    "commandline": "cmd.exe"
                },
                {
                    "name": "profile1",
                    "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                    "historySize": 2,
                    "commandline": "pwsh.exe"
                },
                {
                    "name": "profile2",
                    "historySize": 3,
                    "commandline": "wsl.exe"
                }
            ],
            "actions": [
                {
                    "name": "New Tab With Profile...",
                    "commands": [
                        {
                            "iterateOn": "profiles",
                            "command": { "action": "newTab", "profile": "${profile.name}" }
                        }
                    ]
                }
            ],
            "schemes": [ { "name": "Campbell" } ] // This is included here to prevent settings validation errors.
        })" };

        CascadiaSettings settings{ til::u8u16(settingsJson) };

        VERIFY_ARE_EQUAL(0u, settings.Warnings().Size());
        VERIFY_ARE_EQUAL(3u, settings.Profiles().Size());

        auto commands = settings.GlobalSettings().Commands();
        auto expandedCommands = winrt::TerminalApp::implementation::TerminalPage::_ExpandCommands(commands, settings.Profiles().GetView(), settings.GlobalSettings().ColorSchemes());
        _logCommandNames(expandedCommands.GetView());

        VERIFY_ARE_EQUAL(0u, settings.Warnings().Size());
        VERIFY_ARE_EQUAL(1u, expandedCommands.Size());

        auto rootCommand = expandedCommands.Lookup(L"New Tab With Profile...");
        VERIFY_IS_NOT_NULL(rootCommand);
        auto rootActionAndArgs = rootCommand.Action();
        VERIFY_IS_NULL(rootActionAndArgs);

        VERIFY_ARE_EQUAL(3u, rootCommand.NestedCommands().Size());

        for (auto name : std::vector<std::wstring>({ L"profile0", L"profile1", L"profile2" }))
        {
            winrt::hstring commandName{ fmt::format(L"New tab, profile: {}", name) };
            auto command = rootCommand.NestedCommands().Lookup(commandName);
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);

            VERIFY_ARE_EQUAL(ShortcutAction::NewTab, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<NewTabArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(name, realArgs.TerminalArgs().Profile());

            VERIFY_IS_FALSE(command.HasNestedCommands());
        }
    }
    void SettingsTests::TestMixedNestedAndIterableCommand()
    {
        // This test checks a nested commands that includes an iterable command
        // that includes a nested command.
        // The commands should look like:
        //
        // <Command Palette>
        // └─ New Pane...
        //    ├─ profile0...
        //    |  ├─ Split automatically
        //    |  ├─ Split vertically
        //    |  └─ Split horizontally
        //    ├─ profile1...
        //    |  ├─ Split automatically
        //    |  ├─ Split vertically
        //    |  └─ Split horizontally
        //    └─ profile2...
        //       ├─ Split automatically
        //       ├─ Split vertically
        //       └─ Split horizontally

        const std::string settingsJson{ R"(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "profiles": [
                {
                    "name": "profile0",
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                    "historySize": 1,
                    "commandline": "cmd.exe"
                },
                {
                    "name": "profile1",
                    "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                    "historySize": 2,
                    "commandline": "pwsh.exe"
                },
                {
                    "name": "profile2",
                    "historySize": 3,
                    "commandline": "wsl.exe"
                }
            ],
            "actions": [
                {
                    "name": "New Pane...",
                    "commands": [
                        {
                            "iterateOn": "profiles",
                            "name": "${profile.name}...",
                            "commands": [
                                { "command": { "action": "splitPane", "profile": "${profile.name}", "split": "auto" } },
                                { "command": { "action": "splitPane", "profile": "${profile.name}", "split": "vertical" } },
                                { "command": { "action": "splitPane", "profile": "${profile.name}", "split": "horizontal" } }
                            ]
                        }
                    ]
                }
            ],
            "schemes": [ { "name": "Campbell" } ] // This is included here to prevent settings validation errors.
        })" };

        CascadiaSettings settings{ til::u8u16(settingsJson) };

        VERIFY_ARE_EQUAL(0u, settings.Warnings().Size());
        VERIFY_ARE_EQUAL(3u, settings.Profiles().Size());

        auto commands = settings.GlobalSettings().Commands();
        auto expandedCommands = winrt::TerminalApp::implementation::TerminalPage::_ExpandCommands(commands, settings.Profiles().GetView(), settings.GlobalSettings().ColorSchemes());
        _logCommandNames(expandedCommands.GetView());

        VERIFY_ARE_EQUAL(0u, settings.Warnings().Size());
        VERIFY_ARE_EQUAL(1u, expandedCommands.Size());

        auto rootCommand = expandedCommands.Lookup(L"New Pane...");
        VERIFY_IS_NOT_NULL(rootCommand);
        auto rootActionAndArgs = rootCommand.Action();
        VERIFY_IS_NULL(rootActionAndArgs);

        VERIFY_ARE_EQUAL(3u, rootCommand.NestedCommands().Size());

        for (auto name : std::vector<std::wstring>({ L"profile0", L"profile1", L"profile2" }))
        {
            winrt::hstring commandName{ name + L"..." };
            auto command = rootCommand.NestedCommands().Lookup(commandName);
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.Action();
            VERIFY_IS_NULL(actionAndArgs);

            VERIFY_IS_TRUE(command.HasNestedCommands());
            VERIFY_ARE_EQUAL(3u, command.NestedCommands().Size());

            _logCommandNames(command.NestedCommands());
            {
                winrt::hstring childCommandName{ fmt::format(L"Split pane, profile: {}", name) };
                auto childCommand = command.NestedCommands().Lookup(childCommandName);
                VERIFY_IS_NOT_NULL(childCommand);
                auto childActionAndArgs = childCommand.Action();
                VERIFY_IS_NOT_NULL(childActionAndArgs);

                VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, childActionAndArgs.Action());
                const auto& realArgs = childActionAndArgs.Args().try_as<SplitPaneArgs>();
                VERIFY_IS_NOT_NULL(realArgs);
                // Verify the args have the expected value
                VERIFY_ARE_EQUAL(SplitState::Automatic, realArgs.SplitStyle());
                VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
                VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
                VERIFY_ARE_EQUAL(name, realArgs.TerminalArgs().Profile());

                VERIFY_IS_FALSE(childCommand.HasNestedCommands());
            }
            {
                winrt::hstring childCommandName{ fmt::format(L"Split pane, split: horizontal, profile: {}", name) };
                auto childCommand = command.NestedCommands().Lookup(childCommandName);
                VERIFY_IS_NOT_NULL(childCommand);
                auto childActionAndArgs = childCommand.Action();
                VERIFY_IS_NOT_NULL(childActionAndArgs);

                VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, childActionAndArgs.Action());
                const auto& realArgs = childActionAndArgs.Args().try_as<SplitPaneArgs>();
                VERIFY_IS_NOT_NULL(realArgs);
                // Verify the args have the expected value
                VERIFY_ARE_EQUAL(SplitState::Horizontal, realArgs.SplitStyle());
                VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
                VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
                VERIFY_ARE_EQUAL(name, realArgs.TerminalArgs().Profile());

                VERIFY_IS_FALSE(childCommand.HasNestedCommands());
            }
            {
                winrt::hstring childCommandName{ fmt::format(L"Split pane, split: vertical, profile: {}", name) };
                auto childCommand = command.NestedCommands().Lookup(childCommandName);
                VERIFY_IS_NOT_NULL(childCommand);
                auto childActionAndArgs = childCommand.Action();
                VERIFY_IS_NOT_NULL(childActionAndArgs);

                VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, childActionAndArgs.Action());
                const auto& realArgs = childActionAndArgs.Args().try_as<SplitPaneArgs>();
                VERIFY_IS_NOT_NULL(realArgs);
                // Verify the args have the expected value
                VERIFY_ARE_EQUAL(SplitState::Vertical, realArgs.SplitStyle());
                VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
                VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
                VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
                VERIFY_ARE_EQUAL(name, realArgs.TerminalArgs().Profile());

                VERIFY_IS_FALSE(childCommand.HasNestedCommands());
            }
        }
    }

    void SettingsTests::TestIterableColorSchemeCommands()
    {
        // For this test, put an iterable command with a given `name`,
        // containing a ${profile.name} to replace. When we expand it, it should
        // have created one command for each profile.

        const std::string settingsJson{ R"(
        {
            "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
            "profiles": [
                {
                    "name": "profile0",
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                    "historySize": 1,
                    "commandline": "cmd.exe"
                },
                {
                    "name": "profile1",
                    "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                    "historySize": 2,
                    "commandline": "pwsh.exe"
                },
                {
                    "name": "profile2",
                    "historySize": 3,
                    "commandline": "wsl.exe"
                }
            ],
            "schemes": [
                { "name": "scheme_0" },
                { "name": "scheme_1" },
                { "name": "scheme_2" },
            ],
            "actions": [
                {
                    "name": "iterable command ${scheme.name}",
                    "iterateOn": "schemes",
                    "command": { "action": "splitPane", "profile": "${scheme.name}" }
                },
            ]
        })" };

        CascadiaSettings settings{ til::u8u16(settingsJson) };

        // Since at least one profile does not reference a color scheme,
        // we add a warning saying "the color scheme is unknown"
        VERIFY_ARE_EQUAL(1u, settings.Warnings().Size());

        VERIFY_ARE_EQUAL(3u, settings.Profiles().Size());

        auto commands = settings.GlobalSettings().Commands();
        VERIFY_ARE_EQUAL(1u, commands.Size());

        {
            auto command = commands.Lookup(L"iterable command ${scheme.name}");
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitState::Automatic, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"${scheme.name}", realArgs.TerminalArgs().Profile());
        }

        auto expandedCommands = winrt::TerminalApp::implementation::TerminalPage::_ExpandCommands(commands, settings.Profiles().GetView(), settings.GlobalSettings().ColorSchemes());
        _logCommandNames(expandedCommands.GetView());

        // This is the same warning as above
        VERIFY_ARE_EQUAL(1u, settings.Warnings().Size());
        VERIFY_ARE_EQUAL(3u, expandedCommands.Size());

        // Yes, this test is testing splitPane with profiles named after each
        // color scheme. These would obviously not work in real life, they're
        // just easy tests to write.

        {
            auto command = expandedCommands.Lookup(L"iterable command scheme_0");
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitState::Automatic, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"scheme_0", realArgs.TerminalArgs().Profile());
        }

        {
            auto command = expandedCommands.Lookup(L"iterable command scheme_1");
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitState::Automatic, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"scheme_1", realArgs.TerminalArgs().Profile());
        }

        {
            auto command = expandedCommands.Lookup(L"iterable command scheme_2");
            VERIFY_IS_NOT_NULL(command);
            auto actionAndArgs = command.Action();
            VERIFY_IS_NOT_NULL(actionAndArgs);
            VERIFY_ARE_EQUAL(ShortcutAction::SplitPane, actionAndArgs.Action());
            const auto& realArgs = actionAndArgs.Args().try_as<SplitPaneArgs>();
            VERIFY_IS_NOT_NULL(realArgs);
            // Verify the args have the expected value
            VERIFY_ARE_EQUAL(SplitState::Automatic, realArgs.SplitStyle());
            VERIFY_IS_NOT_NULL(realArgs.TerminalArgs());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().Commandline().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().StartingDirectory().empty());
            VERIFY_IS_TRUE(realArgs.TerminalArgs().TabTitle().empty());
            VERIFY_IS_FALSE(realArgs.TerminalArgs().Profile().empty());
            VERIFY_ARE_EQUAL(L"scheme_2", realArgs.TerminalArgs().Profile());
        }
    }

}
