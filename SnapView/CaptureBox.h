#pragma once

#include "resource.h"

#define MOVETYPE_NONE			0
#define MOVETYPE_MOVE			1
#define MOVETYPE_LEFT			2
#define MOVETYPE_RIGHT			4
#define MOVETYPE_TOP			8
#define MOVETYPE_BOTTOM			16
#define MOVETYPE_FRAME			32

#define MOVETYPE_TOPLEFT		10
#define MOVETYPE_TOPRIGHT		12
#define MOVETYPE_BOTTOMLEFT		18
#define MOVETYPE_BOTTOMRIGHT	20
#define MOVETYPE_RESIZE			30

#define ANIMATETYPE_SCALE		1
#define ANIMATETYPE_FRAME		2

typedef struct CAPTUREBOXINFO
{
    Gdiplus::Bitmap *bitmap;

    POINT size, movePoint, screenMovePoint;
    Gdiplus::RectF cropRect;
    Gdiplus::PointF center;
    bool centerMoved;

    int scaleIndex;
    float scale, startScale, curScale;
    float frameOpacity, startFrameOpacity, targetFrameOpacity;
    int sizeMarks;

    ULONG animateStart;
    UINT_PTR animateTimer;
    int animateType;

    bool moving, moved, captured, trackingMouse, isInClose;
    int moveType;

    RECT closeRect;
} *PCAPTUREBOXINFO;

typedef struct CAPTUREBOXCLOSEINFO
{
    PCAPTUREBOXINFO info;
    RECT rLocation;
} *PCAPTUREBOXCLOSEINFO;

typedef struct CAPTUREBOXWINDOW
{
    HWND hWnd;
    CAPTUREBOXWINDOW *next;
} *PCAPTUREBOXWINDOW;

ATOM RegisterCaptureBoxClass(HINSTANCE hInstance);
void CleanupCaptureBoxResources();
HWND CreateCaptureBox(Gdiplus::Bitmap *bitmap, RECT rect);
HWND ReopenPrevCaptureBox();
void CloseAllCaptureBoxes();
void ShowAllCaptureBoxes();
void HideAllCaptureBoxes(bool forDialog = false);
void ClearCaptureHistory();
void TrimCaptureHistory(int maxHistory);
void QuickSaveCaptureBox(HWND hWnd);
bool ProcessMouseWheel(POINT pt, short wheelDelta);

extern int savedCaptureBoxes;
