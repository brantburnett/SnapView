#include "stdafx.h"
#include "OptionsProcess.h"
#include "ActivationIpc.h"
#include "CaptureBox.h"
#include "Options.h"
#include "SnapView.h"
#include "SnapViewBase.h"

#include <algorithm>
#include <vector>

namespace
{
    constexpr wchar_t OptionsExecutableName[] = L"SnapViewOptions.exe";

    HANDLE optionsProcess = NULL;
    DWORD optionsProcessId = 0;
    HANDLE optionsProcessWait = NULL;

    struct FindWindowData
    {
        DWORD processId;
        HWND window;
    };

    BOOL CALLBACK FindOptionsWindowProc(HWND window, LPARAM lParam)
    {
        auto data = reinterpret_cast<FindWindowData*>(lParam);

        DWORD processId = 0;
        GetWindowThreadProcessId(window, &processId);
        if (processId == data->processId &&
            GetWindow(window, GW_OWNER) == NULL &&
            IsWindowVisible(window))
        {
            data->window = window;
            return FALSE;
        }

        return TRUE;
    }

    HWND FindOptionsWindow()
    {
        if (optionsProcessId == 0)
            return NULL;

        FindWindowData data{ optionsProcessId, NULL };
        EnumWindows(FindOptionsWindowProc, reinterpret_cast<LPARAM>(&data));
        return data.window;
    }

    bool IsOptionsProcessRunning()
    {
        return optionsProcess != NULL && WaitForSingleObject(optionsProcess, 0) == WAIT_TIMEOUT;
    }

    void CALLBACK OptionsProcessExitedCallback(PVOID context, BOOLEAN)
    {
        PostMessage(
            hWndApp,
            WM_OPTIONS_CLOSED,
            static_cast<WPARAM>(reinterpret_cast<ULONG_PTR>(context)),
            0);
    }

    void ReleaseOptionsProcess(HANDLE completionEvent)
    {
        if (optionsProcessWait != NULL)
        {
            UnregisterWaitEx(optionsProcessWait, completionEvent);
            optionsProcessWait = NULL;
        }

        if (optionsProcess != NULL)
        {
            CloseHandle(optionsProcess);
            optionsProcess = NULL;
        }

        optionsProcessId = 0;
        SetActivationOptionsProcessId(0);
    }

    std::wstring GetOptionsExecutablePath()
    {
        std::vector<wchar_t> buffer(MAX_PATH);
        for (;;)
        {
            const DWORD length = GetModuleFileNameW(NULL, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (length == 0)
                return {};

            if (length < buffer.size())
            {
                std::wstring path(buffer.data(), length);
                const size_t separator = path.find_last_of(L'\\');
                if (separator == std::wstring::npos)
                    return {};

                path.resize(separator + 1);
                return path + OptionsExecutableName;
            }

            buffer.resize(buffer.size() * 2);
        }
    }

    void ActivateOptionsWindow()
    {
        // The window does not exist yet while the process is starting; that
        // request is dropped because the new window activates itself.
        HWND window = FindOptionsWindow();
        if (window == NULL)
            return;

        AllowSetForegroundWindow(optionsProcessId);
        if (IsIconic(window))
            ShowWindow(window, SW_RESTORE);
        SetForegroundWindow(window);
    }

    bool StartOptionsProcess()
    {
        const std::wstring path = GetOptionsExecutablePath();
        if (path.empty())
            return false;

        // The child reads this process's ID from the handle (which requires
        // query access) to address the pipe, and waits on it to close itself
        // when the tray exits.
        HANDLE parentHandle = NULL;
        if (!DuplicateHandle(
            GetCurrentProcess(),
            GetCurrentProcess(),
            GetCurrentProcess(),
            &parentHandle,
            SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION,
            TRUE,
            0))
        {
            return false;
        }

        SIZE_T attributeListSize = 0;
        InitializeProcThreadAttributeList(NULL, 1, 0, &attributeListSize);
        std::vector<BYTE> attributeListBuffer(attributeListSize);
        auto attributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributeListBuffer.data());

        bool started = false;
        if (InitializeProcThreadAttributeList(attributeList, 1, 0, &attributeListSize))
        {
            if (UpdateProcThreadAttribute(
                attributeList,
                0,
                PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                &parentHandle,
                sizeof(parentHandle),
                NULL,
                NULL))
            {
                std::wstring commandLine = L"\"" + path + L"\" --parent " +
                    std::to_wstring(reinterpret_cast<ULONG_PTR>(parentHandle));

                STARTUPINFOEXW startupInfo{};
                startupInfo.StartupInfo.cb = sizeof(startupInfo);
                startupInfo.lpAttributeList = attributeList;

                PROCESS_INFORMATION processInfo{};
                if (CreateProcessW(
                    path.c_str(),
                    commandLine.data(),
                    NULL,
                    NULL,
                    TRUE,
                    EXTENDED_STARTUPINFO_PRESENT,
                    NULL,
                    NULL,
                    &startupInfo.StartupInfo,
                    &processInfo))
                {
                    CloseHandle(processInfo.hThread);
                    optionsProcess = processInfo.hProcess;
                    optionsProcessId = processInfo.dwProcessId;
                    SetActivationOptionsProcessId(optionsProcessId);
                    AllowSetForegroundWindow(optionsProcessId);

                    if (!RegisterWaitForSingleObject(
                        &optionsProcessWait,
                        optionsProcess,
                        OptionsProcessExitedCallback,
                        reinterpret_cast<PVOID>(static_cast<ULONG_PTR>(optionsProcessId)),
                        INFINITE,
                        WT_EXECUTEONLYONCE))
                    {
                        optionsProcessWait = NULL;
                    }

                    started = true;
                }
            }

            DeleteProcThreadAttributeList(attributeList);
        }

        CloseHandle(parentHandle);
        return started;
    }
}

void LaunchOrActivateOptions()
{
    if (IsOptionsProcessRunning())
    {
        ActivateOptionsWindow();
        return;
    }

    if (optionsProcess != NULL)
        OnOptionsProcessExited(optionsProcessId);

    if (!StartOptionsProcess())
    {
        MessageBox(
            hWndApp,
            _T("SnapView could not open the Options window. Reinstall the SnapView MSIX package."),
            _T("SnapView"),
            MB_ICONERROR | MB_OK);
    }
}

void OnOptionsProcessExited(DWORD processId)
{
    // Ignore a late notification for a process that was already handled.
    if (optionsProcess == NULL || processId != optionsProcessId)
        return;

    ReleaseOptionsProcess(NULL);

    // History is trimmed only when Options closes, so stepping the limit down
    // while editing does not discard snaps.
    LoadOptions();
    TrimCaptureHistory(options.maxHistory);
    SetHistoryCapacity(options.maxHistory);
}

void OnOptionsSettingsChanged()
{
    LoadOptions();
    SetHistoryCapacity((std::max)(GetHistoryCapacity(), options.maxHistory));
}

void CloseOptionsForExit()
{
    if (IsOptionsProcessRunning())
    {
        HWND window = FindOptionsWindow();
        if (window != NULL)
            PostMessage(window, WM_CLOSE, 0, 0);
    }

    ReleaseOptionsProcess(INVALID_HANDLE_VALUE);
}
