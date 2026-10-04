// SnapView.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "SnapView.h"
#include "SnapViewBase.h"
#include "CaptureBox.h"
#include "Options.h"
#include "OptionsProcess.h"
#include "Share.h"
#include "SnapHook.h"
#include "SizeMarks.h"
#include "LimitSingleInstance.h"
#include "ActivationIpc.h"
#include <initguid.h>
#include <string>

using namespace Gdiplus;

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;
ULONG_PTR gdiplusToken;
TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name
CLimitSingleInstance singleInstance(_T("Local\\cb2d15e2-d0f2-4ecd-892f-3177231b33b9"));

HMENU hNotifyMenu, hCaptureMenu;
HCURSOR hCursorArrow;
HCURSOR hCursorMove;
HCURSOR hCursorNS;
HCURSOR hCursorEW;
HCURSOR hCursorNESW;
HCURSOR hCursorNWSE;
Bitmap *bScreen;
HBITMAP hScreen;
POINT dragMin;

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
HWND				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);

namespace
{
    bool IsStartupTaskActivation()
    {
        try
        {
            const auto activatedEventArgs =
                winrt::Windows::ApplicationModel::AppInstance::GetActivatedEventArgs();
            return activatedEventArgs.Kind() ==
                winrt::Windows::ApplicationModel::Activation::ActivationKind::StartupTask;
        }
        catch (const winrt::hresult_error&)
        {
            return false;
        }
    }
}

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    if (singleInstance.IsAnotherInstanceRunning())
    {
        if (!RequestShowOptionsFromRunningInstance())
        {
            MessageBox(
                NULL,
                _T("SnapView is already running, but its Options window could not be opened."),
                _T("SnapView"),
                MB_ICONERROR | MB_OK);
            return 1;
        }

        return 0;
    }

    INITCOMMONCONTROLSEX initCtrls;
    initCtrls.dwSize = sizeof(initCtrls);
    initCtrls.dwICC = ICC_STANDARD_CLASSES | ICC_UPDOWN_CLASS | ICC_LINK_CLASS;
    InitCommonControlsEx(&initCtrls);

    HRESULT oleInitializeResult = OleInitialize(NULL);
    if (FAILED(oleInitializeResult))
    {
        MessageBox(NULL, _T("Unable to initialize OLE support."), _T("SnapView"), MB_ICONERROR | MB_OK);
        return 1;
    }

    HRESULT initializeResult = RoInitialize(RO_INIT_SINGLETHREADED);
    if (FAILED(initializeResult))
    {
        MessageBox(NULL, _T("Unable to initialize Windows Runtime support."), _T("SnapView"), MB_ICONERROR | MB_OK);
        OleUninitialize();
        return 1;
    }

    const bool startupTaskActivation = IsStartupTaskActivation();

    GdiplusStartupInput input;
    GdiplusStartup(&gdiplusToken, &input, NULL);

    dragMin.x = GetSystemMetrics(SM_CXDRAG);
    dragMin.y = GetSystemMetrics(SM_CYDRAG);

    // TODO: Place code here.
    MSG msg;

    // Initialize global strings and window classes
    LoadString(hInstance, IDC_SNAPVIEW, szWindowClass, MAX_LOADSTRING);
    BaseRegisterClass(hInstance);
    MyRegisterClass(hInstance);
    RegisterCaptureBoxClass(hInstance);
    InitSizeMarks(hInstance);

    LoadOptions();
    SetHistoryCapacity(options.maxHistory);

    // Perform application initialization:
    hWndApp = InitInstance (hInstance, nCmdShow);
    if (!hWndApp)
    {
        ShutdownSizeMarks();
        CleanupCaptureBoxResources();
        GdiplusShutdown(gdiplusToken);
        CleanupShare();
        RoUninitialize();
        OleUninitialize();
        return FALSE;
    }

    if (!StartActivationIpc(hWndApp))
    {
        MessageBox(
            NULL,
            _T("SnapView could not initialize its activation service."),
            _T("SnapView"),
            MB_ICONERROR | MB_OK);
        DestroyWindow(hWndApp);
        ShutdownSizeMarks();
        CleanupCaptureBoxResources();
        GdiplusShutdown(gdiplusToken);
        CleanupShare();
        RoUninitialize();
        OleUninitialize();
        return FALSE;
    }

    hNotifyMenu = LoadMenu(hInst, MAKEINTRESOURCE(IDC_NOTIFYICONMENU));
    hCaptureMenu = LoadMenu(hInst, MAKEINTRESOURCE(IDC_CAPTUREMENU));

    hCursorArrow = LoadCursor(NULL, IDC_ARROW);
    hCursorMove	= LoadCursor(NULL, IDC_SIZEALL);
    hCursorNS = LoadCursor(NULL, IDC_SIZENS);
    hCursorEW = LoadCursor(NULL, IDC_SIZEWE);
    hCursorNESW = LoadCursor(NULL, IDC_SIZENESW);
    hCursorNWSE = LoadCursor(NULL, IDC_SIZENWSE);

    SnapHookSetHooks();

    if (!startupTaskActivation)
        LaunchOrActivateOptions();

    // Main message loop:
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    StopActivationIpc();

    DestroyMenu(hNotifyMenu);
    DestroyMenu(hCaptureMenu);

    DestroyCursor(hCursorMove);
    DestroyCursor(hCursorNS);
    DestroyCursor(hCursorEW);
    DestroyCursor(hCursorNESW);
    DestroyCursor(hCursorNWSE);

    SnapHookClearHooks();

    ShutdownSizeMarks();
    CleanupCaptureBoxResources();
    GdiplusShutdown(gdiplusToken);

    CleanupShare();
    RoUninitialize();
    OleUninitialize();

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    This function and its usage are only necessary if you want this code
//    to be compatible with Win32 systems prior to the 'RegisterClassEx'
//    function that was added to Windows 95. It is important to call this function
//    so that the application will get 'well formed' small icons associated
//    with it.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEX wcex;
    memset(&wcex, 0, sizeof(WNDCLASSEX));

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style			= CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc	= WndProc;
    wcex.cbClsExtra		= 0;
    wcex.cbWndExtra		= 0;
    wcex.hInstance		= hInstance;
    wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SNAPVIEW));
    wcex.hCursor		= LoadCursor(NULL, IDC_CROSS);
    wcex.hbrBackground	= CreateSolidBrush(RGB(0, 0, 0));
    wcex.lpszClassName	= szWindowClass;

    return RegisterClassEx(&wcex);
}

// Capture routines

bool capturing = false;
POINT startPoint, endPoint;

void CalcPoints(RECT &rect, int &sizeMarks)
{
    if (startPoint.x <= endPoint.x)
    {
        rect.left = startPoint.x;
        rect.right = endPoint.x;
        sizeMarks = SIZEMARKLOCATION_LEFT;
    }
    else
    {
        rect.left = endPoint.x;
        rect.right = startPoint.x;
        sizeMarks = SIZEMARKLOCATION_RIGHT;
    }

    if (startPoint.y <= endPoint.y)
    {
        rect.top = startPoint.y;
        rect.bottom = endPoint.y;
        sizeMarks |= SIZEMARKLOCATION_TOP;
    }
    else
    {
        rect.top = endPoint.y;
        rect.bottom = startPoint.y;
        sizeMarks |= SIZEMARKLOCATION_BOTTOM;
    }
}

void DoCapture() {
    if (options.hideOnNewSnap)
        HideAllCaptureBoxes();
    Sleep(50);

    HWND hDesktop = GetDesktopWindow();
    HDC hdcSrc = GetDC(hDesktop);
    HDC hdcDest = CreateCompatibleDC(hdcSrc);

    int x = GetSystemMetrics(SM_XVIRTUALSCREEN),
        y = GetSystemMetrics(SM_YVIRTUALSCREEN),
        cx = GetSystemMetrics(SM_CXVIRTUALSCREEN),
        cy = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    HBITMAP hBitmap = CreateCompatibleBitmap(hdcSrc, cx, cy);
    HGDIOBJ hOld = SelectObject(hdcDest, hBitmap);
    BitBlt(hdcDest, 0, 0, cx, cy, hdcSrc, x, y, SRCCOPY | CAPTUREBLT);
    SelectObject(hdcDest, hOld);

    ReleaseDC(hDesktop, hdcSrc);
    DeleteDC(hdcDest);

    bScreen = new Bitmap(hBitmap, NULL);
    hScreen = hBitmap;

    HWND hWnd = CreateWindowEx(WS_EX_TOPMOST, szWindowClass, NULL, WS_POPUP,
      x, y, cx, cy, hWndApp, NULL, hInst, NULL);

    SetWindowPos(hWnd, HWND_TOPMOST, x, y, cx, cy, 0);
    ShowWindow(hWnd, SW_SHOWNORMAL);
}

void StopCapture(HWND hWnd)
{
    capturing = false;
    ShowWindow(hWnd, SW_HIDE);
    if (options.hideOnNewSnap)
        ShowAllCaptureBoxes();

    if (bScreen)
    {
        delete bScreen;
        bScreen = NULL;

        DeleteObject(hScreen);
        hScreen = 0;
    }

    DestroyWindow(hWnd);
}

void MouseDown(HWND hWnd, POINT p)
{
    UNREFERENCED_PARAMETER(hWnd);

    capturing = true;
    startPoint = p;
    endPoint = p;
}

void MouseMove(HWND hWnd, POINT p)
{
    if (!capturing) return;

    RECT oldRect, oldBounds, newRect, newBounds, dirtyRect;
    int oldSizeMarks, newSizeMarks;

    CalcPoints(oldRect, oldSizeMarks);
    ExpandForSizeMarks(&oldRect, &oldBounds, oldSizeMarks);

    endPoint = p;

    CalcPoints(newRect, newSizeMarks);
    ExpandForSizeMarks(&newRect, &newBounds, newSizeMarks);
    UnionRect(&dirtyRect, &oldBounds, &newBounds);

    InvalidateRect(hWnd, &dirtyRect, FALSE);
}

void MouseUp(HWND hWnd, POINT p)
{
    if (!capturing) return;

    endPoint = p;
    RECT rect;
    int sizeMarks;
    CalcPoints(rect, sizeMarks);

    if ((abs(rect.right-rect.left) < dragMin.x) && (abs(rect.bottom-rect.top) < dragMin.y))
    {
        StopCapture(hWnd);
        return;
    }

    Graphics *gSrc = new Graphics(bScreen);
    Bitmap *bitmap = new Bitmap(rect.right-rect.left, rect.bottom-rect.top, gSrc);
    delete gSrc;
    Graphics *gDest = new Graphics(bitmap);

    gDest->DrawImage(bScreen, 0, 0, rect.left, rect.top, rect.right-rect.left, rect.bottom-rect.top, UnitPixel);
    delete gDest;

    MapWindowPoints(hWnd, NULL, (LPPOINT)&rect, 2);
    StopCapture(hWnd);
    CreateCaptureBox(bitmap, rect);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
HWND InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    UNREFERENCED_PARAMETER(nCmdShow);

    hInst = hInstance;

    return CreateBaseWindow();
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT ps;
    HDC hdc;
    POINT p;
    PMINMAXINFO info;

    switch (message)
    {
    case WM_LBUTTONDOWN:
        p.x = GET_X_LPARAM(lParam);
        p.y = GET_Y_LPARAM(lParam);
        MouseDown(hWnd, p);
        break;
    case WM_MOUSEMOVE:
        p.x = GET_X_LPARAM(lParam);
        p.y = GET_Y_LPARAM(lParam);
        MouseMove(hWnd, p);
        break;
    case WM_LBUTTONUP:
        p.x = GET_X_LPARAM(lParam);
        p.y = GET_Y_LPARAM(lParam);
        MouseUp(hWnd, p);
        break;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
            StopCapture(hWnd);
        break;
    case WM_PAINT:
        hdc = BeginPaint(hWnd, &ps);

        if (bScreen)
        {
            Rect paintRect(ps.rcPaint.left, ps.rcPaint.top,
                ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top);
            HDC bufferDC = CreateCompatibleDC(hdc);
            HBITMAP bufferBitmap = CreateCompatibleBitmap(hdc, paintRect.Width, paintRect.Height);
            HGDIOBJ oldBufferBitmap = SelectObject(bufferDC, bufferBitmap);

            {
                Graphics g(bufferDC);
                g.DrawImage(bScreen, Rect(0, 0, paintRect.Width, paintRect.Height),
                    paintRect.X, paintRect.Y, paintRect.Width, paintRect.Height, UnitPixel);

                if (capturing)
                {
                    RECT rect;
                    int sizeMarks;
                    CalcPoints(rect, sizeMarks);

                    GraphicsState paintState = g.Save();
                    g.TranslateTransform((float)-paintRect.X, (float)-paintRect.Y);

                    SolidBrush b(Color(0x7f, 0, 0, 0));
                    g.FillRectangle(&b, rect.left, rect.top, rect.right-rect.left, rect.bottom-rect.top);

                    RECT newRect;
                    ExpandForSizeMarks(&rect, &newRect, sizeMarks);

                    RectF clipRect;
                    clipRect.X = 0;
                    clipRect.Y = 0;
                    clipRect.Width = (float)(rect.right-rect.left);
                    clipRect.Height = (float)(rect.bottom-rect.top);

                    GraphicsState gState = g.Save();
                    g.TranslateTransform((float)newRect.left, (float)newRect.top);

                    SIZEMARKOPTIONS sizeMarkOptions;
                    memset(&sizeMarkOptions, 0, sizeof(sizeMarkOptions));
                    sizeMarkOptions.lpRect = &newRect;
                    sizeMarkOptions.lpCropRect = &clipRect;
                    sizeMarkOptions.dwLocation = sizeMarks;
                    sizeMarkOptions.fOpacity = 1.0;
                    DrawSizeMarks(&g, &sizeMarkOptions);

                    g.Restore(gState);
                    g.Restore(paintState);
                }
            }

            BitBlt(hdc, paintRect.X, paintRect.Y, paintRect.Width, paintRect.Height,
                bufferDC, 0, 0, SRCCOPY);
            SelectObject(bufferDC, oldBufferBitmap);
            DeleteObject(bufferBitmap);
            DeleteDC(bufferDC);
        }

        EndPaint(hWnd, &ps);
        break;
    case WM_ERASEBKGND:
        break;
    case WM_GETMINMAXINFO:
        info = (PMINMAXINFO)lParam;
        info->ptMinTrackSize.x = 0;
        info->ptMinTrackSize.y = 0;
        break;
    case WM_NCCALCSIZE:
        return 0;
        break;
    case WM_NCPAINT:
        return 0;
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
