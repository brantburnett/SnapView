#include "stdafx.h"
#include "ActivationIpc.h"
#include "SnapViewBase.h"

namespace
{
    constexpr wchar_t ActivationPipeName[] = L"\\\\.\\pipe\\SnapView-cb2d15e2-d0f2-4ecd-892f-3177231b33b9";
    constexpr char ShowOptionsCommand[] = "show-options";

    HANDLE activationThread = NULL;
    volatile LONG activationIpcStopping = FALSE;
    HWND activationTargetWindow = NULL;

    bool SendCommand(DWORD timeout)
    {
        if (!WaitNamedPipe(ActivationPipeName, timeout))
            return false;

        HANDLE pipe = CreateFile(
            ActivationPipeName,
            GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            0,
            NULL);
        if (pipe == INVALID_HANDLE_VALUE)
            return false;

        ULONG serverProcessId = 0;
        if (!GetNamedPipeServerProcessId(pipe, &serverProcessId) ||
            !AllowSetForegroundWindow(serverProcessId))
        {
            CloseHandle(pipe);
            return false;
        }

        DWORD bytesWritten = 0;
        const BOOL succeeded = WriteFile(
            pipe,
            ShowOptionsCommand,
            sizeof(ShowOptionsCommand) - 1,
            &bytesWritten,
            NULL);
        CloseHandle(pipe);

        return succeeded && bytesWritten == sizeof(ShowOptionsCommand) - 1;
    }

    DWORD WINAPI ActivationIpcThreadProc(LPVOID)
    {
        while (InterlockedCompareExchange(&activationIpcStopping, FALSE, FALSE) == FALSE)
        {
            HANDLE pipe = CreateNamedPipe(
                ActivationPipeName,
                PIPE_ACCESS_INBOUND,
                PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                1,
                0,
                sizeof(ShowOptionsCommand),
                0,
                NULL);
            if (pipe == INVALID_HANDLE_VALUE)
                break;

            BOOL connected = ConnectNamedPipe(pipe, NULL);
            if (!connected)
            {
                const DWORD error = GetLastError();
                connected = error == ERROR_PIPE_CONNECTED;
                if (!connected && error != ERROR_OPERATION_ABORTED)
                {
                    CloseHandle(pipe);
                    continue;
                }
            }

            if (connected)
            {
                char command[sizeof(ShowOptionsCommand)] = {};
                DWORD bytesRead = 0;
                const BOOL received = ReadFile(pipe, command, sizeof(command), &bytesRead, NULL);
                if (received &&
                    bytesRead == sizeof(ShowOptionsCommand) - 1 &&
                    memcmp(command, ShowOptionsCommand, bytesRead) == 0 &&
                    InterlockedCompareExchange(&activationIpcStopping, FALSE, FALSE) == FALSE)
                {
                    PostMessage(activationTargetWindow, WM_SHOW_OPTIONS, 0, 0);
                }

                DisconnectNamedPipe(pipe);
            }

            CloseHandle(pipe);
        }

        return 0;
    }
}

bool StartActivationIpc(HWND targetWindow)
{
    if (activationThread != NULL)
        return true;

    activationTargetWindow = targetWindow;
    InterlockedExchange(&activationIpcStopping, FALSE);
    activationThread = CreateThread(NULL, 0, ActivationIpcThreadProc, NULL, 0, NULL);
    if (activationThread == NULL)
    {
        activationTargetWindow = NULL;
        return false;
    }

    return true;
}

void StopActivationIpc()
{
    if (activationThread == NULL)
        return;

    InterlockedExchange(&activationIpcStopping, TRUE);
    CancelSynchronousIo(activationThread);
    SendCommand(100);
    WaitForSingleObject(activationThread, INFINITE);
    CloseHandle(activationThread);
    activationThread = NULL;
    activationTargetWindow = NULL;
}

bool RequestShowOptionsFromRunningInstance()
{
    const ULONGLONG deadline = GetTickCount64() + 2000;
    do
    {
        if (SendCommand(100))
            return true;

        Sleep(50);
    } while (GetTickCount64() < deadline);

    return false;
}
