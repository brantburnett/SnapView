#include "stdafx.h"
#include "SnapViewBase.h"
#include "SnapView.h"
#include "CaptureBox.h"
#include "Options.h"
#include <initguid.h>

#define MAX_LOADSTRING 100
#define WM_NOTIFYICON WM_USER
DEFINE_GUID(NOTIFYICONGUID, 0xcb2d15e2, 0xd0f2, 0x4ecd, 0x89, 0x2f, 0x31, 0x77, 0x23, 0x1b, 0x33, 0xb9);

HWND hWndApp;
TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
TCHAR szBaseWindowClass[MAX_LOADSTRING];		// the main window class name

HICON hIconLarge;
HWND hForeWindow = NULL;

bool menuUp = false;
bool stopCaptures = false;

LRESULT CALLBACK	BaseWndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

ATOM BaseRegisterClass(HINSTANCE hInstance)
{
    LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadString(hInstance, IDC_SNAPVIEWBASE, szBaseWindowClass, MAX_LOADSTRING);
    hIconLarge = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SNAPVIEW));

    WNDCLASSEX wcex;
    memset(&wcex, 0, sizeof(WNDCLASSEX));

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style			= CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc	= BaseWndProc;
    wcex.cbClsExtra		= 0;
    wcex.cbWndExtra		= 0;
    wcex.hInstance		= hInstance;
    wcex.hIcon			= hIconLarge;
    wcex.hbrBackground	= CreateSolidBrush(RGB(0, 0, 0));
    wcex.lpszClassName	= szBaseWindowClass;

    return RegisterClassEx(&wcex);
}

BOOL IsWin7OrLater()
{
    // Initialize the OSVERSIONINFOEX structure.
    OSVERSIONINFOEX osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    osvi.dwMajorVersion = 6;
    osvi.dwMinorVersion = 1;

    // Initialize the condition mask.
    DWORDLONG dwlConditionMask = 0;
    VER_SET_CONDITION(dwlConditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(dwlConditionMask, VER_MINORVERSION, VER_GREATER_EQUAL);

    // Perform the test.
    return VerifyVersionInfo(&osvi,
                             VER_MAJORVERSION | VER_MINORVERSION,
                             dwlConditionMask);
}

DLLVERSIONINFO GetShell32Version()
{
    DLLVERSIONINFO info;
    info.cbSize = sizeof(DLLVERSIONINFO);

    HMODULE module = LoadLibrary(_T("shell32.dll"));
    DLLGETVERSIONPROC proc = (DLLGETVERSIONPROC)GetProcAddress(module, "DllGetVersion");

    proc(&info);
    return info;
}

void FillNotifyIconData(HWND hWnd, PNOTIFYICONDATA data)
{
    DLLVERSIONINFO version = GetShell32Version();

    memset(data, 0, sizeof(NOTIFYICONDATA));
    if (version.dwMajorVersion < 5)
        data->cbSize = NOTIFYICONDATA_V1_SIZE;
    else if (version.dwMajorVersion < 6)
        data->cbSize = NOTIFYICONDATA_V2_SIZE;
    else if (version.dwMajorVersion == 6 && version.dwMinorVersion == 0 && version.dwBuildNumber < 6)
        data->cbSize = NOTIFYICONDATA_V3_SIZE;
    else
        data->cbSize = sizeof(NOTIFYICONDATA);

    data->hWnd = hWnd;
    data->uID = 1;
    data->uFlags = NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP | NIF_ICON;
    data->uCallbackMessage = WM_NOTIFYICON;
    _tcscpy_s(data->szTip, 128, _T("SnapView - Click To Capture"));

    if (IsWin7OrLater())
        data->guidItem = NOTIFYICONGUID;
}

void LoadTrayIcon(HWND hWnd)
{
    NOTIFYICONDATA data;
    FillNotifyIconData(hWnd, &data);

    data.uFlags |= NIF_ICON;
    data.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_SNAPVIEW));

    Shell_NotifyIcon(NIM_ADD, &data);

    DLLVERSIONINFO version = GetShell32Version();
    if (version.dwMajorVersion >= 5)
    {
        data.uVersion = NOTIFYICON_VERSION;
        Shell_NotifyIcon(NIM_SETVERSION, &data);
    }
}

void RemoveTrayIcon(HWND hWnd)
{
    NOTIFYICONDATA data;
    FillNotifyIconData(hWnd, &data);

    Shell_NotifyIcon(NIM_DELETE, &data);
}

void ParseNotifyIconCommand(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(wParam);

    HMENU hPopup;
    POINT pos;

    switch (lParam) {
        case WM_RBUTTONUP:
            if (hForeWindow) {
                SetForegroundWindow(hForeWindow);
                break;
            }
            if (stopCaptures) break;

            hPopup = GetSubMenu(hNotifyMenu, 0);

            EnableMenuItem(hPopup, IDM_REOPEN, savedCaptureBoxes > 0 ? MF_ENABLED : MF_GRAYED);

            GetCursorPos(&pos);

            menuUp = true;
            SetForegroundWindow(hWnd);
            TrackPopupMenu(hPopup, TPM_RIGHTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON, pos.x, pos.y, 0, hWnd, NULL);
            menuUp = false;

            break;

        case WM_LBUTTONUP:
            if (hForeWindow) {
                SetForegroundWindow(hForeWindow);
                break;
            }

            if (!stopCaptures && !menuUp)
                DoCapture();
    }
}

HWND CreateBaseWindow()
{
    HWND hWnd;

    hWnd = CreateWindowEx(0, szBaseWindowClass, szTitle, WS_POPUP,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInst, NULL);

    if (!hWnd)
    {
      return FALSE;
    }

    LoadTrayIcon(hWnd);

    return hWnd;
}

void ShowOptions()
{
    if (hForeWindow != NULL)
    {
        SetForegroundWindow(hForeWindow);
        return;
    }

    HideAllCaptureBoxes(true);
    ShowOptionsDialog(hWndApp);
    ShowAllCaptureBoxes();
}

LRESULT CALLBACK BaseWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int wmId, wmEvent;

    switch (message)
    {
    case WM_COMMAND:
        wmId    = LOWORD(wParam);
        wmEvent = HIWORD(wParam);
        // Parse the menu selections:
        switch (wmId)
        {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_OPTIONS:
            ShowOptions();
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        case IDM_SNAP:
            DoCapture();
            break;
        case IDM_REOPEN:
            ReopenPrevCaptureBox();
            break;
        case IDM_CLOSEALL:
            CloseAllCaptureBoxes();
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;
    case WM_NOTIFYICON:
        ParseNotifyIconCommand(hWnd, wParam, lParam);
        break;
    case WM_SHOW_OPTIONS:
        ShowOptions();
        break;
    case WM_DESTROY:
        RemoveTrayIcon(hWnd);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    int cx, cy;

    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        cx = GetSystemMetrics(SM_CXSCREEN);
        cy = GetSystemMetrics(SM_CYSCREEN);

        RECT rect;
        GetWindowRect(hDlg, &rect);

        cx = (cx - (rect.right - rect.left)) / 2;
        cy = (cy - (rect.bottom - rect.top)) / 2;
        SetWindowPos(hDlg, NULL, cx, cy, 0, 0, SWP_NOSIZE | SWP_NOOWNERZORDER);
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
