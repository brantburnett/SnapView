#include "pch.h"
#include "OptionsWindow.xaml.h"
#if __has_include("OptionsWindow.g.cpp")
#include "OptionsWindow.g.cpp"
#endif

#include "ActivationPipe.h"
#include "SnapViewSettings.h"
#include "resource.h"

#include <cmath>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Windowing;
using winrt::Windows::ApplicationModel::StartupTask;
using winrt::Windows::ApplicationModel::StartupTaskState;

namespace
{
    constexpr std::chrono::milliseconds SaveDebounce{ 400 };

    // Window size in effective pixels.
    constexpr int WindowWidth = 640;
    constexpr int WindowHeight = 780;
    constexpr int MinimumWindowWidth = 480;
    constexpr int MinimumWindowHeight = 480;

    std::wstring NormalizeFolderPath(std::wstring path)
    {
        const auto isTrimmed = [](wchar_t c) { return c == L' ' || c == L'\t' || c == L'"'; };

        // Explorer's "Copy as path" wraps the path in quotes.
        while (!path.empty() && isTrimmed(path.back()))
            path.pop_back();
        size_t start = 0;
        while (start < path.size() && isTrimmed(path[start]))
            start++;
        path.erase(0, start);

        // Keep the separator on a drive root such as "C:\".
        while (path.size() > 3 && (path.back() == L'\\' || path.back() == L'/'))
            path.pop_back();

        return path;
    }

    std::wstring GetPicturesFolder()
    {
        PWSTR folder = nullptr;
        std::wstring result;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Pictures, 0, NULL, &folder)))
            result = folder;
        CoTaskMemFree(folder);
        return result;
    }

    fire_and_forget SendCommandInBackground(
        const char* command,
        DWORD serverProcessId,
        Microsoft::UI::Dispatching::DispatcherQueue dispatcherQueue,
        std::function<void(bool)> completed)
    {
        co_await resume_background();

        bool succeeded = false;
        for (int attempt = 0; attempt < 3 && !succeeded; attempt++)
        {
            if (attempt > 0)
                Sleep(100);
            succeeded = ActivationPipe::SendCommand(command, 200, false, serverProcessId);
        }

        if (completed)
        {
            dispatcherQueue.TryEnqueue([completed = std::move(completed), succeeded]()
            {
                completed(succeeded);
            });
        }
    }
}

namespace winrt::SnapViewOptions::implementation
{
    OptionsWindow::OptionsWindow(uint32_t parentProcessId) :
        parentProcessId(parentProcessId)
    {
        // Xaml objects should not call InitializeComponent during construction.
        // See https://github.com/microsoft/cppwinrt/tree/master/nuget#initializecomponent
    }

    void OptionsWindow::InitializeComponent()
    {
        OptionsWindowT::InitializeComponent();

        saveTimer = DispatcherQueue().CreateTimer();
        saveTimer.Interval(SaveDebounce);
        saveTimer.IsRepeating(false);
        saveTimer.Tick([weak = get_weak()](auto&&, auto&&)
        {
            if (auto self = weak.get())
                self->FlushPendingChanges(true);
        });

        Closed([this](const IInspectable&, const WindowEventArgs&)
        {
            // SnapView reloads settings when this process exits, so the final
            // flush does not need to notify it.
            saveTimer.Stop();
            FlushPendingChanges(false);
        });

        Activated([this](const IInspectable&, const WindowActivatedEventArgs& args)
        {
            // Startup can be changed in Windows Settings while Options is open.
            if (args.WindowActivationState() != WindowActivationState::Deactivated && !startupBusy)
                RefreshStartupTask();
        });

        ConfigureWindow();
        LoadSettings();
    }

    void OptionsWindow::ConfigureWindow()
    {
        ExtendsContentIntoTitleBar(true);
        SetTitleBar(AppTitleBar());

        const auto appWindow = AppWindow();
        const HICON icon = LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDI_SNAPVIEWOPTIONS));
        if (icon != NULL)
            appWindow.SetIcon(Microsoft::UI::GetIconIdFromIcon(icon));

        presenter = OverlappedPresenter::Create();
        presenter.IsMaximizable(false);
        appWindow.SetPresenter(presenter);

        // Open centered on the monitor under the pointer. Move there first so
        // the size is computed with that monitor's DPI.
        POINT cursor{};
        GetCursorPos(&cursor);
        const auto displayArea = DisplayArea::GetFromPoint({ cursor.x, cursor.y }, DisplayAreaFallback::Nearest);
        const auto workArea = displayArea.WorkArea();
        appWindow.Move({ workArea.X, workArea.Y });

        const HWND hwnd = Microsoft::UI::GetWindowFromWindowId(appWindow.Id());
        const double scale = GetDpiForWindow(hwnd) / 96.0;
        ApplyMinimumSize(scale);

        // The presenter's minimum size is in physical pixels, so rescale it
        // when the window moves to a monitor with a different DPI.
        RootGrid().Loaded([weak = get_weak()](const IInspectable&, const RoutedEventArgs&)
        {
            auto self = weak.get();
            if (!self)
                return;

            const auto xamlRoot = self->RootGrid().XamlRoot();
            self->ApplyMinimumSize(xamlRoot.RasterizationScale());
            xamlRoot.Changed([weak](const Microsoft::UI::Xaml::XamlRoot& sender, const Microsoft::UI::Xaml::XamlRootChangedEventArgs&)
            {
                if (auto self = weak.get())
                    self->ApplyMinimumSize(sender.RasterizationScale());
            });
        });

        const int width = (std::min)(static_cast<int>(WindowWidth * scale), workArea.Width);
        const int height = (std::min)(static_cast<int>(WindowHeight * scale), workArea.Height);
        appWindow.MoveAndResize({
            workArea.X + (workArea.Width - width) / 2,
            workArea.Y + (workArea.Height - height) / 2,
            width,
            height });
    }

    void OptionsWindow::ApplyMinimumSize(double scale)
    {
        if (scale <= 0 || scale == minimumSizeScale)
            return;

        minimumSizeScale = scale;
        presenter.PreferredMinimumWidth(static_cast<int32_t>(std::ceil(MinimumWindowWidth * scale)));
        presenter.PreferredMinimumHeight(static_cast<int32_t>(std::ceil(MinimumWindowHeight * scale)));
    }

    void OptionsWindow::LoadSettings()
    {
        loading = true;

        SnapViewSettings::Settings settings;
        try
        {
            settings = SnapViewSettings::Load();
        }
        catch (const hresult_error&)
        {
            ShowStatus(
                InfoBarSeverity::Error,
                L"SnapView couldn't read your settings. Defaults are shown, and changes may not be saved.");
        }

        if (parentProcessId == 0)
        {
            ShowStatus(
                InfoBarSeverity::Warning,
                L"Options isn't connected to SnapView. Changes are saved, but SnapView applies them only after it restarts.");
        }

        savedMaxHistory = settings.maxHistory;
        savedQuickSavePath = settings.quickSavePath;

        MaxHistoryBox().Value(settings.maxHistory);
        HideOnNewSnapToggle().IsOn(settings.hideOnNewSnap);
        ShowHoverInfoToggle().IsOn(settings.showHoverInfo);
        DefaultSaveTypeBox().SelectedIndex(settings.defaultSaveType - SnapViewSettings::SaveTypePng);
        QuickSavePathBox().PlaceholderText(GetPicturesFolder());
        QuickSavePathBox().Text(settings.quickSavePath);

        loading = false;
    }

    void OptionsWindow::ScheduleSave()
    {
        saveTimer.Stop();
        saveTimer.Start();
    }

    void OptionsWindow::FlushPendingChanges(bool notify)
    {
        bool saved = false;
        bool failed = false;

        if (maxHistoryPending)
        {
            maxHistoryPending = false;
            (SaveMaxHistory() ? saved : failed) = true;
        }

        if (quickSavePathPending)
        {
            quickSavePathPending = false;
            // An invalid folder shows an inline error rather than a save error.
            saved = SaveQuickSavePath() || saved;
        }

        if (failed)
            OnSaveCompleted(false);
        else if (saved && notify)
            OnSaveCompleted(true);
    }

    bool OptionsWindow::SaveMaxHistory()
    {
        const double value = MaxHistoryBox().Value();
        if (std::isnan(value))
            return true;

        const int32_t maxHistory = static_cast<int32_t>(std::lround(value));
        if (maxHistory == savedMaxHistory)
            return true;

        if (!SnapViewSettings::WriteMaxHistory(maxHistory))
            return false;

        savedMaxHistory = maxHistory;
        return true;
    }

    bool OptionsWindow::SaveQuickSavePath()
    {
        const std::wstring path = NormalizeFolderPath(std::wstring(QuickSavePathBox().Text()));
        if (!SnapViewSettings::IsValidQuickSavePath(path))
        {
            QuickSavePathError().Visibility(Visibility::Visible);
            return false;
        }

        QuickSavePathError().Visibility(Visibility::Collapsed);
        if (path == savedQuickSavePath)
            return false;

        if (!SnapViewSettings::WriteQuickSavePath(path))
        {
            OnSaveCompleted(false);
            return false;
        }

        savedQuickSavePath = path;
        return true;
    }

    void OptionsWindow::OnSaveCompleted(bool succeeded)
    {
        if (!succeeded)
        {
            ShowStatus(InfoBarSeverity::Error, L"SnapView couldn't save your change. Try again.");
            return;
        }

        SendCommand(ActivationPipe::SettingsChangedCommand);
    }

    void OptionsWindow::ShowStatus(InfoBarSeverity severity, const hstring& message)
    {
        const auto infoBar = StatusInfoBar();
        infoBar.Severity(severity);
        infoBar.Message(message);
        infoBar.IsOpen(true);
    }

    void OptionsWindow::SendCommand(const char* command, std::function<void(bool)> completed)
    {
        // Without a parent, Options was not launched by SnapView and there is
        // no tray process to notify.
        if (parentProcessId == 0)
        {
            if (completed)
                completed(false);
            return;
        }

        SendCommandInBackground(command, parentProcessId, DispatcherQueue(), std::move(completed));
    }

    void OptionsWindow::MaxHistoryBox_ValueChanged(
        const NumberBox& sender,
        const NumberBoxValueChangedEventArgs& args)
    {
        if (loading)
            return;

        const double value = args.NewValue();
        if (std::isnan(value))
        {
            // An empty box reverts to the saved value.
            DispatcherQueue().TryEnqueue([weak = get_weak()]()
            {
                if (auto self = weak.get())
                    self->MaxHistoryBox().Value(self->savedMaxHistory);
            });
            return;
        }

        const double rounded = std::round(value);
        if (rounded != value)
        {
            sender.Value(rounded);
            return;
        }

        maxHistoryPending = true;
        ScheduleSave();
    }

    void OptionsWindow::ConfirmClearHistory_Click(const IInspectable&, const RoutedEventArgs&)
    {
        ClearHistoryFlyout().Hide();
        SendCommand(ActivationPipe::ClearHistoryCommand, [weak = get_weak()](bool succeeded)
        {
            auto self = weak.get();
            if (!self)
                return;

            if (succeeded)
                self->ShowStatus(InfoBarSeverity::Success, L"Snap history cleared.");
            else
                self->ShowStatus(InfoBarSeverity::Error, L"SnapView couldn't clear history. Make sure SnapView is running, then try again.");
        });
    }

    void OptionsWindow::HideOnNewSnapToggle_Toggled(const IInspectable&, const RoutedEventArgs&)
    {
        if (!loading)
            OnSaveCompleted(SnapViewSettings::WriteHideOnNewSnap(HideOnNewSnapToggle().IsOn()));
    }

    void OptionsWindow::ShowHoverInfoToggle_Toggled(const IInspectable&, const RoutedEventArgs&)
    {
        if (!loading)
            OnSaveCompleted(SnapViewSettings::WriteShowHoverInfo(ShowHoverInfoToggle().IsOn()));
    }

    void OptionsWindow::DefaultSaveTypeBox_SelectionChanged(const IInspectable&, const SelectionChangedEventArgs&)
    {
        const int32_t index = DefaultSaveTypeBox().SelectedIndex();
        if (loading || index < 0)
            return;

        OnSaveCompleted(SnapViewSettings::WriteDefaultSaveType(index + SnapViewSettings::SaveTypePng));
    }

    void OptionsWindow::QuickSavePathBox_TextChanged(const IInspectable&, const TextChangedEventArgs&)
    {
        if (loading)
            return;

        // TextChanged is raised asynchronously, including for the initial
        // value, so compare with what is saved instead of relying on loading.
        const std::wstring path = NormalizeFolderPath(std::wstring(QuickSavePathBox().Text()));
        if (path == savedQuickSavePath)
        {
            quickSavePathPending = false;
            QuickSavePathError().Visibility(Visibility::Collapsed);
            return;
        }

        quickSavePathPending = true;
        ScheduleSave();
    }

    fire_and_forget OptionsWindow::BrowseButton_Click(IInspectable, RoutedEventArgs)
    {
        auto weak = get_weak();

        Microsoft::Windows::Storage::Pickers::FolderPicker picker(AppWindow().Id());
        picker.SuggestedStartLocation(Microsoft::Windows::Storage::Pickers::PickerLocationId::PicturesLibrary);

        Microsoft::Windows::Storage::Pickers::PickFolderResult result{ nullptr };
        try
        {
            result = co_await picker.PickSingleFolderAsync();
        }
        catch (const hresult_error&)
        {
            if (auto self = weak.get())
                self->ShowStatus(InfoBarSeverity::Error, L"SnapView couldn't open the folder picker.");
            co_return;
        }

        auto self = weak.get();
        if (!self || !result)
            co_return;

        self->QuickSavePathBox().Text(result.Path());
        self->quickSavePathPending = true;
        self->saveTimer.Stop();
        self->FlushPendingChanges(true);
    }

    fire_and_forget OptionsWindow::RefreshStartupTask()
    {
        auto weak = get_weak();
        const uint32_t requestId = ++startupRequestId;

        bool succeeded = true;
        StartupTaskState state = StartupTaskState::Disabled;
        try
        {
            const auto startupTask = co_await StartupTask::GetAsync(SnapViewSettings::StartupTaskId);
            state = startupTask.State();
        }
        catch (const hresult_error&)
        {
            succeeded = false;
        }

        auto self = weak.get();
        if (!self || requestId != self->startupRequestId || self->startupBusy)
            co_return;

        if (succeeded)
            self->ApplyStartupState(state);
        else
            self->SetStartupUnavailable(L"The startup setting is unavailable. Reinstall SnapView to restore it.");
    }

    fire_and_forget OptionsWindow::StartupToggle_Toggled(IInspectable, RoutedEventArgs)
    {
        if (updatingStartupToggle)
            co_return;

        auto weak = get_weak();
        const bool enable = StartupToggle().IsOn();

        startupBusy = true;
        ++startupRequestId;
        StartupToggle().IsEnabled(false);
        StartupStatusText().Text(L"Updating startup setting...");

        bool succeeded = true;
        StartupTaskState state = StartupTaskState::Disabled;
        try
        {
            const auto startupTask = co_await StartupTask::GetAsync(SnapViewSettings::StartupTaskId);
            if (enable)
            {
                state = co_await startupTask.RequestEnableAsync();
            }
            else
            {
                startupTask.Disable();
                state = startupTask.State();
            }
        }
        catch (const hresult_error&)
        {
            succeeded = false;
        }

        auto self = weak.get();
        if (!self)
            co_return;

        self->startupBusy = false;
        if (succeeded)
            self->ApplyStartupState(state);
        else
            self->SetStartupUnavailable(L"SnapView couldn't change the startup setting. Try again.");
    }

    fire_and_forget OptionsWindow::StartupSettingsLink_Click(IInspectable, RoutedEventArgs)
    {
        try
        {
            co_await Windows::System::Launcher::LaunchUriAsync(Windows::Foundation::Uri(L"ms-settings:startupapps"));
        }
        catch (const hresult_error&)
        {
        }
    }

    void OptionsWindow::SetStartupToggle(bool isOn, bool isEnabled)
    {
        updatingStartupToggle = true;
        StartupToggle().IsOn(isOn);
        updatingStartupToggle = false;
        StartupToggle().IsEnabled(isEnabled);
    }

    void OptionsWindow::ApplyStartupState(StartupTaskState state)
    {
        const auto infoBar = StartupInfoBar();
        infoBar.IsOpen(false);
        StartupSettingsLink().Visibility(Visibility::Collapsed);

        switch (state)
        {
        case StartupTaskState::Enabled:
            SetStartupToggle(true, true);
            StartupStatusText().Text(L"SnapView starts when you sign in to Windows.");
            break;

        case StartupTaskState::Disabled:
            SetStartupToggle(false, true);
            StartupStatusText().Text(L"SnapView doesn't start when you sign in to Windows.");
            break;

        case StartupTaskState::DisabledByUser:
            SetStartupToggle(false, false);
            StartupStatusText().Text(L"Turned off in Windows.");
            infoBar.Severity(InfoBarSeverity::Warning);
            infoBar.Message(L"Startup for SnapView was turned off in Windows. Turn it back on in Startup apps settings.");
            StartupSettingsLink().Visibility(Visibility::Visible);
            infoBar.IsOpen(true);
            break;

        case StartupTaskState::DisabledByPolicy:
            SetStartupToggle(false, false);
            StartupStatusText().Text(L"Managed by your organization.");
            infoBar.Severity(InfoBarSeverity::Informational);
            infoBar.Message(L"Your organization has turned off startup for SnapView.");
            infoBar.IsOpen(true);
            break;

        case StartupTaskState::EnabledByPolicy:
            SetStartupToggle(true, false);
            StartupStatusText().Text(L"Managed by your organization.");
            infoBar.Severity(InfoBarSeverity::Informational);
            infoBar.Message(L"Your organization has turned on startup for SnapView.");
            infoBar.IsOpen(true);
            break;

        default:
            SetStartupUnavailable(L"The startup setting is unavailable. Reinstall SnapView to restore it.");
            break;
        }
    }

    void OptionsWindow::SetStartupUnavailable(const hstring& message)
    {
        SetStartupToggle(false, false);
        StartupStatusText().Text(message);
        StartupInfoBar().IsOpen(false);
    }

    void OptionsWindow::CloseButton_Click(const IInspectable&, const RoutedEventArgs&)
    {
        Close();
    }
}
