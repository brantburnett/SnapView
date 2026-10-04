#include "pch.h"
#include "App.xaml.h"
#include "OptionsWindow.xaml.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace
{
    std::wstring GetExecutableDirectory()
    {
        std::wstring path(MAX_PATH, L'\0');
        for (;;)
        {
            const DWORD length = GetModuleFileNameW(NULL, path.data(), static_cast<DWORD>(path.size()));
            if (length == 0)
                return {};

            if (length < path.size())
            {
                path.resize(length);
                break;
            }

            path.resize(path.size() * 2);
        }

        const size_t separator = path.find_last_of(L'\\');
        return separator == std::wstring::npos ? std::wstring{} : path.substr(0, separator + 1);
    }
}

namespace winrt::SnapViewOptions::implementation
{
    App::App()
    {
        // The package's resources.pri indexes only the package assets. This
        // app's compiled XAML is in SnapViewOptions.pri next to the exe.
        ResourceManagerRequested([](const IInspectable&, const ResourceManagerRequestedEventArgs& args)
        {
            const std::wstring priPath = GetExecutableDirectory() + L"SnapViewOptions.pri";
            args.CustomResourceManager(
                Microsoft::Windows::ApplicationModel::Resources::ResourceManager(priPath));
        });

#if defined _DEBUG && !defined DISABLE_XAML_GENERATED_BREAK_ON_UNHANDLED_EXCEPTION
        UnhandledException([](const IInspectable&, const UnhandledExceptionEventArgs& e)
        {
            if (IsDebuggerPresent())
            {
                auto errorMessage = e.Message();
                __debugbreak();
            }
        });
#endif
    }

    void App::OnLaunched(const LaunchActivatedEventArgs&)
    {
        const bool hasParent = parentWatcher.Initialize();

        window = make<OptionsWindow>(hasParent ? parentWatcher.ParentProcessId() : 0);
        window.Closed([this](const IInspectable&, const WindowEventArgs&)
        {
            window = nullptr;
        });

        if (hasParent)
        {
            // Close with the tray app, including when it crashes.
            parentWatcher.Watch(window.DispatcherQueue(), [this]()
            {
                if (window)
                    window.Close();
            });
        }

        window.Activate();
    }
}
