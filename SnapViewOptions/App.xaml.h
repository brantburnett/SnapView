#pragma once

#include "App.xaml.g.h"
#include "ParentWatcher.h"

namespace winrt::SnapViewOptions::implementation
{
    struct App : AppT<App>
    {
        App();

        void OnLaunched(const Microsoft::UI::Xaml::LaunchActivatedEventArgs&);

    private:
        Microsoft::UI::Xaml::Window window{ nullptr };
        ParentWatcher parentWatcher;
    };
}
