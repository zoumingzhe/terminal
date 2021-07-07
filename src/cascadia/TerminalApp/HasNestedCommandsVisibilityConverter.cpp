#include "pch.h"
#include "HasNestedCommandsVisibilityConverter.h"
#include "HasNestedCommandsVisibilityConverter.g.cpp"

using namespace winrt::Windows;
using namespace winrt::Windows::UI::Xaml;

namespace winrt::TerminalApp::implementation
{
    // Method Description:
    // - Attempt to convert something into another type. For the
    //   HasNestedCommandsVisibilityConverter, we're gonna check if `value` is a
    //   string, and try and convert it into a Visibility value. If the input
    //   param wasn't a string, or was the empty string, we'll return
    //   Visibility::Collapsed. Otherwise, we'll return Visible.

    // Arguments:
    // - value: the input object to attempt to convert into a Visibility.
    // Return Value:
    // - Visible if the object was a string and wasn't the empty string.
    Foundation::IInspectable HasNestedCommandsVisibilityConverter::Convert(Foundation::IInspectable const& value,
                                                                           Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                                           Foundation::IInspectable const& /* parameter */,
                                                                           hstring const& /* language */)
    {
        const auto& hasNestedCommands = winrt::unbox_value_or<bool>(value, false);
        return winrt::box_value(hasNestedCommands ? Visibility::Visible : Visibility::Collapsed);
    }

    // unused for one-way bindings
    Foundation::IInspectable HasNestedCommandsVisibilityConverter::ConvertBack(Foundation::IInspectable const& /* value */,
                                                                               Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                                               Foundation::IInspectable const& /* parameter */,
                                                                               hstring const& /* language */)
    {
        throw hresult_not_implemented();
    }
}
