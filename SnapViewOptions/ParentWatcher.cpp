#include "pch.h"
#include "ParentWatcher.h"

#include <shellapi.h>

ParentWatcher::~ParentWatcher()
{
    if (waitHandle != NULL)
        UnregisterWaitEx(waitHandle, INVALID_HANDLE_VALUE);

    if (parentProcess != NULL)
        CloseHandle(parentProcess);
}

bool ParentWatcher::Initialize()
{
    int argumentCount = 0;
    LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
    if (arguments == NULL)
        return false;

    ULONG_PTR handleValue = 0;
    for (int i = 1; i + 1 < argumentCount; i++)
    {
        if (_wcsicmp(arguments[i], L"--parent") == 0)
        {
            wchar_t* end = nullptr;
            handleValue = static_cast<ULONG_PTR>(wcstoull(arguments[i + 1], &end, 10));
            if (end == arguments[i + 1] || *end != L'\0')
                handleValue = 0;
            break;
        }
    }

    LocalFree(arguments);
    if (handleValue == 0)
        return false;

    HANDLE handle = reinterpret_cast<HANDLE>(handleValue);
    const DWORD processId = GetProcessId(handle);
    if (processId == 0)
        return false;

    parentProcess = handle;
    parentProcessId = processId;
    return true;
}

void ParentWatcher::Watch(
    const winrt::Microsoft::UI::Dispatching::DispatcherQueue& queue,
    std::function<void()> callback)
{
    if (parentProcess == NULL || waitHandle != NULL)
        return;

    dispatcherQueue = queue;
    onExited = std::move(callback);
    if (!RegisterWaitForSingleObject(
        &waitHandle,
        parentProcess,
        OnParentExited,
        this,
        INFINITE,
        WT_EXECUTEONLYONCE))
    {
        waitHandle = NULL;
    }
}

void CALLBACK ParentWatcher::OnParentExited(PVOID context, BOOLEAN)
{
    auto watcher = static_cast<ParentWatcher*>(context);
    watcher->dispatcherQueue.TryEnqueue([watcher]()
    {
        if (watcher->onExited)
            watcher->onExited();
    });
}
