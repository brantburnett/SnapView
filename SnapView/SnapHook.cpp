// SnapHook.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "SnapHook.h"
#include "SnapViewBase.h"
#include "CaptureBox.h"
#include "SnapView.h"

bool lShiftDown = false, rShiftDown = false, lCtrlDown = false, rCtrlDown = false;
HHOOK hKeyboardHook, hMouseHook;

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
     if (nCode == HC_ACTION)
    {
        switch (wParam)
        {
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
            PKBDLLHOOKSTRUCT p = (PKBDLLHOOKSTRUCT) lParam;
            switch (p->vkCode)
            {
            case VK_SNAPSHOT:
                if (lCtrlDown || rCtrlDown)
                {
                    if ((wParam == WM_KEYUP) && !stopCaptures)
                    {
                        if (lShiftDown || rShiftDown)
                            ReopenPrevCaptureBox();
                        else
                            DoCapture();
                    }
                }
                break;
            case VK_LSHIFT:
                lShiftDown = wParam == WM_KEYDOWN;
                break;
            case VK_RSHIFT:
                rShiftDown = wParam == WM_KEYDOWN;
                break;
            case VK_LCONTROL:
                lCtrlDown = wParam == WM_KEYDOWN;
                break;
            case VK_RCONTROL:
                rCtrlDown = wParam == WM_KEYDOWN;
                break;
            }
        }
    }

    return CallNextHookEx(hKeyboardHook, nCode, wParam, lParam);
}

LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION && wParam == WM_MOUSEWHEEL)
    {
        PMSLLHOOKSTRUCT pInfo = (PMSLLHOOKSTRUCT)lParam;
        if (ProcessMouseWheel(pInfo->pt, GET_WHEEL_DELTA_WPARAM(pInfo->mouseData)))
            return TRUE;
    }

    return CallNextHookEx(hMouseHook, nCode, wParam, lParam);
}

void SnapHookSetHooks()
{
    hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, &KeyboardProc, hInst, NULL);
    hMouseHook = SetWindowsHookEx(WH_MOUSE_LL, &MouseProc, hInst, NULL);
}

void SnapHookClearHooks()
{
    if (hKeyboardHook)
        UnhookWindowsHookEx(hKeyboardHook);
    if (hMouseHook)
        UnhookWindowsHookEx(hMouseHook);
}
