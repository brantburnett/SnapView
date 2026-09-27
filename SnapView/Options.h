#pragma once

#include "stdafx.h"
#include "resource.h"

#define MAX_CAPTURE_HISTORY 20
#define SAVETYPE_PNG		1
#define SAVETYPE_BMP		2
#define SAVETYPE_GIF		3
#define SAVETYPE_JPEG		4

typedef struct
{
    int maxHistory;
    TCHAR quickSavePath[MAX_PATH];
    int defaultSaveType;
    bool hideOnNewSnap;
    bool showHoverInfo;
} OPTIONS, *POPTIONS;

typedef struct
{
    WNDPROC lpfnWndProc;
    int prevValue;
} MAXHISTORYDATA, *PMAXHISTORYDATA;

extern OPTIONS options;

ATOM RegisterOptionsClass(HINSTANCE hInst);
void LoadOptions();
bool SaveOptions();
INT_PTR ShowOptionsDialog(HWND hWnd);
