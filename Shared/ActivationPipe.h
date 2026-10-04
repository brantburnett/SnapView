#pragma once

// Named-pipe protocol between SnapView.exe (server) and its clients: a second
// SnapView.exe instance and SnapViewOptions.exe. Each connection carries one
// fixed command string.

#include <windows.h>

#include <cstring>
#include <string>

namespace ActivationPipe
{
    inline constexpr char ShowOptionsCommand[] = "show-options";
    inline constexpr char SettingsChangedCommand[] = "settings-changed";
    inline constexpr char ClearHistoryCommand[] = "clear-history";

    // Longest command plus one byte so oversized messages are detected.
    inline constexpr DWORD MaxCommandBuffer = sizeof(SettingsChangedCommand);

    enum class Command
    {
        Unknown,
        ShowOptions,
        SettingsChanged,
        ClearHistory
    };

    inline Command ParseCommand(const char* buffer, DWORD length) noexcept
    {
        const auto matches = [buffer, length](const char* command, size_t size)
        {
            return length == size - 1 && memcmp(buffer, command, length) == 0;
        };

        if (matches(ShowOptionsCommand, sizeof(ShowOptionsCommand)))
            return Command::ShowOptions;
        if (matches(SettingsChangedCommand, sizeof(SettingsChangedCommand)))
            return Command::SettingsChanged;
        if (matches(ClearHistoryCommand, sizeof(ClearHistoryCommand)))
            return Command::ClearHistory;

        return Command::Unknown;
    }

    // The pipe name is scoped to the Windows session so each signed-in user
    // gets a separate server.
    inline std::wstring GetPipeName()
    {
        DWORD sessionId = 0;
        if (!ProcessIdToSessionId(GetCurrentProcessId(), &sessionId))
            sessionId = 0;

        return L"\\\\.\\pipe\\SnapView-cb2d15e2-d0f2-4ecd-892f-3177231b33b9-" + std::to_wstring(sessionId);
    }

    // Sends one command. When expectedServerProcessId is non-zero, the command
    // is sent only if that process owns the pipe. When allowForeground is
    // true, the server is allowed to take the foreground first.
    inline bool SendCommand(
        const char* command,
        DWORD timeout,
        bool allowForeground,
        DWORD expectedServerProcessId = 0) noexcept
    {
        const std::wstring pipeName = GetPipeName();
        if (!WaitNamedPipeW(pipeName.c_str(), timeout))
            return false;

        HANDLE pipe = CreateFileW(
            pipeName.c_str(),
            GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION,
            NULL);
        if (pipe == INVALID_HANDLE_VALUE)
            return false;

        ULONG serverProcessId = 0;
        if (!GetNamedPipeServerProcessId(pipe, &serverProcessId) ||
            (expectedServerProcessId != 0 && serverProcessId != expectedServerProcessId) ||
            (allowForeground && !AllowSetForegroundWindow(serverProcessId)))
        {
            CloseHandle(pipe);
            return false;
        }

        const DWORD length = static_cast<DWORD>(strlen(command));
        DWORD bytesWritten = 0;
        const BOOL succeeded = WriteFile(pipe, command, length, &bytesWritten, NULL);
        CloseHandle(pipe);

        return succeeded && bytesWritten == length;
    }
}
