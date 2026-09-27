#pragma once

#include "resource.h"

extern HINSTANCE hInst;
extern HWND hWndApp;
extern HMENU hNotifyMenu;
extern HMENU hCaptureMenu;
extern HCURSOR hCursorArrow;
extern HCURSOR hCursorMove;
extern HCURSOR hCursorNS;
extern HCURSOR hCursorEW;
extern HCURSOR hCursorNESW;
extern HCURSOR hCursorNWSE;
extern POINT dragMin;

void DoCapture();