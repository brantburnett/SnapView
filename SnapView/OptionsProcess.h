#pragma once

#include "stdafx.h"

// The Options window is a WinUI 3 app in a separate process,
// SnapViewOptions.exe. The tray process never loads XAML.

void LaunchOrActivateOptions();
void OnOptionsProcessExited(DWORD processId);
void OnOptionsSettingsChanged();
void CloseOptionsForExit();
