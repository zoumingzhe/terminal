/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- CascadiaSettings.hpp

Abstract:
- This class encapsulates all of the settings that are global to the app, and
    not a part of any particular profile.

Author(s):
- Mike Griese - March 2019

--*/
#pragma once

#include "GlobalAppSettings.g.h"

#include "KeyMapping.h"
#include "Command.h"
#include "ColorScheme.h"

// fwdecl unittest classes
namespace SettingsModelLocalTests
{
    class DeserializationTests;
    class ColorSchemeTests;
};

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct GlobalAppSettings : GlobalAppSettingsT<GlobalAppSettings>
    {
    public:
        GlobalAppSettings();
        com_ptr<GlobalAppSettings> Copy() const;

        Windows::Foundation::Collections::IMapView<hstring, Model::ColorScheme> ColorSchemes() noexcept;
        void AddColorScheme(const Model::ColorScheme& scheme);

        Model::KeyMapping KeyMap() const noexcept;

        static com_ptr<GlobalAppSettings> FromJson(const Json::Value& json);
        void LayerJson(const Json::Value& json);

        std::vector<SettingsLoadWarnings> KeybindingsWarnings() const;

        Windows::Foundation::Collections::IMapView<hstring, Model::Command> Commands() noexcept;

        // These are implemented manually to handle the string/GUID exchange
        // by higher layers in the app.
        void DefaultProfile(const guid& defaultProfile) noexcept;
        guid DefaultProfile() const;
        hstring UnparsedDefaultProfile() const;

        GETSET_PROPERTY(int32_t, InitialRows, DEFAULT_ROWS);
        GETSET_PROPERTY(int32_t, InitialCols, DEFAULT_COLS);
        GETSET_PROPERTY(bool, AlwaysShowTabs, true);
        GETSET_PROPERTY(bool, ShowTitleInTitlebar, true);
        GETSET_PROPERTY(bool, ConfirmCloseAllTabs, true);
        GETSET_PROPERTY(winrt::Windows::UI::Xaml::ElementTheme, Theme, winrt::Windows::UI::Xaml::ElementTheme::Default);
        GETSET_PROPERTY(winrt::Microsoft::UI::Xaml::Controls::TabViewWidthMode, TabWidthMode, winrt::Microsoft::UI::Xaml::Controls::TabViewWidthMode::Equal);
        GETSET_PROPERTY(bool, ShowTabsInTitlebar, true);
        GETSET_PROPERTY(hstring, WordDelimiters, DEFAULT_WORD_DELIMITERS);
        GETSET_PROPERTY(bool, CopyOnSelect, false);
        GETSET_PROPERTY(winrt::Microsoft::Terminal::TerminalControl::CopyFormat, CopyFormatting, 0);
        GETSET_PROPERTY(bool, WarnAboutLargePaste, true);
        GETSET_PROPERTY(bool, WarnAboutMultiLinePaste, true);
        GETSET_PROPERTY(Model::LaunchPosition, InitialPosition, nullptr, nullptr);
        GETSET_PROPERTY(Model::LaunchMode, LaunchMode, LaunchMode::DefaultMode);
        GETSET_PROPERTY(bool, SnapToGridOnResize, true);
        GETSET_PROPERTY(bool, ForceFullRepaintRendering, false);
        GETSET_PROPERTY(bool, SoftwareRendering, false);
        GETSET_PROPERTY(bool, ForceVTInput, false);
        GETSET_PROPERTY(bool, DebugFeaturesEnabled); // default value set in constructor
        GETSET_PROPERTY(bool, StartOnUserLogin, false);
        GETSET_PROPERTY(bool, AlwaysOnTop, false);
        GETSET_PROPERTY(bool, UseTabSwitcher, true);
        GETSET_PROPERTY(bool, DisableAnimations, false);

    private:
        hstring _unparsedDefaultProfile;
        guid _defaultProfile;

        com_ptr<KeyMapping> _keymap;
        std::vector<SettingsLoadWarnings> _keybindingsWarnings;

        Windows::Foundation::Collections::IMap<hstring, Model::ColorScheme> _colorSchemes;
        Windows::Foundation::Collections::IMap<hstring, Model::Command> _commands;

        friend class SettingsModelLocalTests::DeserializationTests;
        friend class SettingsModelLocalTests::ColorSchemeTests;
    };
}
