#include "stdafx.h"
#include "ActivationIpc.h"
#include "SnapViewBase.h"
#include "ActivationPipe.h"

namespace
{
    HANDLE activationThread = NULL;
    volatile LONG activationIpcStopping = FALSE;
    volatile LONG optionsProcessId = 0;
    HWND activationTargetWindow = NULL;
    std::wstring activationPipeName;

    bool IsStopping()
    {
        return InterlockedCompareExchange(&activationIpcStopping, FALSE, FALSE) != FALSE;
    }

    bool IsOptionsProcess(HANDLE pipe)
    {
        const LONG expectedProcessId = InterlockedCompareExchange(&optionsProcessId, 0, 0);
        ULONG clientProcessId = 0;
        return expectedProcessId != 0 &&
            GetNamedPipeClientProcessId(pipe, &clientProcessId) &&
            clientProcessId == static_cast<ULONG>(expectedProcessId);
    }

    void DispatchCommand(HANDLE pipe, const char* buffer, DWORD length)
    {
        switch (ActivationPipe::ParseCommand(buffer, length))
        {
        case ActivationPipe::Command::ShowOptions:
            PostMessage(activationTargetWindow, WM_SHOW_OPTIONS, 0, 0);
            break;

        case ActivationPipe::Command::SettingsChanged:
            if (IsOptionsProcess(pipe))
                PostMessage(activationTargetWindow, WM_SETTINGS_CHANGED, 0, 0);
            break;

        case ActivationPipe::Command::ClearHistory:
            if (IsOptionsProcess(pipe))
                PostMessage(activationTargetWindow, WM_CLEAR_HISTORY, 0, 0);
            break;

        default:
            break;
        }
    }

    DWORD WINAPI ActivationIpcThreadProc(LPVOID)
    {
        while (!IsStopping())
        {
            HANDLE pipe = CreateNamedPipe(
                activationPipeName.c_str(),
                PIPE_ACCESS_INBOUND,
                PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
                1,
                0,
                ActivationPipe::MaxCommandBuffer,
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
                // A message longer than the buffer fails with ERROR_MORE_DATA
                // and is dropped.
                char command[ActivationPipe::MaxCommandBuffer] = {};
                DWORD bytesRead = 0;
                const BOOL received = ReadFile(pipe, command, sizeof(command), &bytesRead, NULL);
                if (received && !IsStopping())
                    DispatchCommand(pipe, command, bytesRead);

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

    activationPipeName = ActivationPipe::GetPipeName();
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
    ActivationPipe::SendCommand(ActivationPipe::ShowOptionsCommand, 100, false);
    WaitForSingleObject(activationThread, INFINITE);
    CloseHandle(activationThread);
    activationThread = NULL;
    activationTargetWindow = NULL;
}

void SetActivationOptionsProcessId(DWORD processId)
{
    InterlockedExchange(&optionsProcessId, static_cast<LONG>(processId));
}

bool RequestShowOptionsFromRunningInstance()
{
    const ULONGLONG deadline = GetTickCount64() + 2000;
    do
    {
        if (ActivationPipe::SendCommand(ActivationPipe::ShowOptionsCommand, 100, true))
            return true;

        Sleep(50);
    } while (GetTickCount64() < deadline);

    return false;
}
