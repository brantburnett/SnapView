#pragma once

#include "resource.h"

extern HICON hIconLarge;
extern bool stopCaptures;
extern HWND hForeWindow;

#define WM_SHOW_OPTIONS (WM_APP + 2)
#define WM_SETTINGS_CHANGED (WM_APP + 3)
#define WM_CLEAR_HISTORY (WM_APP + 4)
#define WM_OPTIONS_CLOSED (WM_APP + 5)

ATOM BaseRegisterClass(HINSTANCE hInstance);
HWND CreateBaseWindow();
