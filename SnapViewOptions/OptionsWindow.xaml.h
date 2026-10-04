#pragma once

#include "OptionsWindow.g.h"

namespace winrt::SnapViewOptions::implementation
{
    // Each setting is saved as it changes. Toggles and the combo box save
    // immediately; typed values save after a short debounce and only when
    // valid. SnapView is notified over its activation pipe after each save.
    struct OptionsWindow : OptionsWindowT<OptionsWindow>
    {
        explicit OptionsWindow(uint32_t parentProcessId);

        void InitializeComponent();

        void MaxHistoryBox_ValueChanged(
            const Microsoft::UI::Xaml::Controls::NumberBox& sender,
            const Microsoft::UI::Xaml::Controls::NumberBoxValueChangedEventArgs& args);
        void ConfirmClearHistory_Click(const IInspectable& sender, const Microsoft::UI::Xaml::RoutedEventArgs& args);
        void HideOnNewSnapToggle_Toggled(const IInspectable& sender, const Microsoft::UI::Xaml::RoutedEventArgs& args);
        void ShowHoverInfoToggle_Toggled(const IInspectable& sender, const Microsoft::UI::Xaml::RoutedEventArgs& args);
        void DefaultSaveTypeBox_SelectionChanged(
            const IInspectable& sender,
            const Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs& args);
        void QuickSavePathBox_TextChanged(
            const IInspectable& sender,
            const Microsoft::UI::Xaml::Controls::TextChangedEventArgs& args);
        fire_and_forget BrowseButton_Click(IInspectable sender, Microsoft::UI::Xaml::RoutedEventArgs args);
        fire_and_forget StartupToggle_Toggled(IInspectable sender, Microsoft::UI::Xaml::RoutedEventArgs args);
        fire_and_forget StartupSettingsLink_Click(IInspectable sender, Microsoft::UI::Xaml::RoutedEventArgs args);
        void CloseButton_Click(const IInspectable& sender, const Microsoft::UI::Xaml::RoutedEventArgs& args);

    private:
        void ConfigureWindow();
        void ApplyMinimumSize(double scale);
        void LoadSettings();

        void ScheduleSave();
        void FlushPendingChanges(bool notify);
        bool SaveMaxHistory();
        bool SaveQuickSavePath();
        void OnSaveCompleted(bool succeeded);
        void ShowStatus(
            Microsoft::UI::Xaml::Controls::InfoBarSeverity severity,
            const hstring& message);

        void SendCommand(const char* command, std::function<void(bool)> completed = nullptr);

        fire_and_forget RefreshStartupTask();
        void ApplyStartupState(Windows::ApplicationModel::StartupTaskState state);
        void SetStartupUnavailable(const hstring& message);
        void SetStartupToggle(bool isOn, bool isEnabled);

        uint32_t parentProcessId;
        bool loading = true;
        bool updatingStartupToggle = false;
        bool startupBusy = false;
        uint32_t startupRequestId = 0;

        bool maxHistoryPending = false;
        bool quickSavePathPending = false;
        int32_t savedMaxHistory = 0;
        std::wstring savedQuickSavePath;

        Microsoft::UI::Dispatching::DispatcherQueueTimer saveTimer{ nullptr };
        Microsoft::UI::Windowing::OverlappedPresenter presenter{ nullptr };
        double minimumSizeScale = 0;
    };
}

namespace winrt::SnapViewOptions::factory_implementation
{
    struct OptionsWindow : OptionsWindowT<OptionsWindow, implementation::OptionsWindow>
    {
    };
}
