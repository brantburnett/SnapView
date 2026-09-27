#pragma once

#include "resource.h"

extern HICON hIconLarge;
extern bool stopCaptures;
extern HWND hForeWindow;

ATOM BaseRegisterClass(HINSTANCE hInstance);
HWND CreateBaseWindow();