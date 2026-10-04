#pragma once

#include <functional>

// Watches the SnapView tray process that launched this process. SnapView
// passes an inheritable handle to itself, with SYNCHRONIZE and
// PROCESS_QUERY_LIMITED_INFORMATION access, as "--parent <handle>".
class ParentWatcher
{
public:
    ParentWatcher() = default;
    ParentWatcher(const ParentWatcher&) = delete;
    ParentWatcher& operator=(const ParentWatcher&) = delete;
    ~ParentWatcher();

    // Reads the parent handle from the command line. Returns false when the
    // process was not launched by SnapView.
    bool Initialize();

    // Calls onExited on the given dispatcher queue when the parent exits.
    void Watch(
        const winrt::Microsoft::UI::Dispatching::DispatcherQueue& dispatcherQueue,
        std::function<void()> onExited);

    DWORD ParentProcessId() const noexcept { return parentProcessId; }

private:
    static void CALLBACK OnParentExited(PVOID context, BOOLEAN timedOut);

    HANDLE parentProcess = NULL;
    DWORD parentProcessId = 0;
    HANDLE waitHandle = NULL;
    winrt::Microsoft::UI::Dispatching::DispatcherQueue dispatcherQueue{ nullptr };
    std::function<void()> onExited;
};
