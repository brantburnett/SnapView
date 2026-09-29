#pragma once

#include "resource.h"

extern HICON hIconLarge;
extern bool stopCaptures;
extern HWND hForeWindow;

#define WM_SHOW_OPTIONS (WM_APP + 2)

ATOM BaseRegisterClass(HINSTANCE hInstance);
HWND CreateBaseWindow();
void ShowOptions();