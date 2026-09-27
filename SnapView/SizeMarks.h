#pragma once

#include "stdafx.h"

#define SIZEMARKLOCATION_NONE		0
#define SIZEMARKLOCATION_LEFT		1
#define SIZEMARKLOCATION_RIGHT		2
#define SIZEMARKLOCATION_TOP		4
#define SIZEMARKLOCATION_BOTTOM		8

#define SIZEMARKLOCATION_TOPLEFT	5

#define SIZEMARKOPTION_SHOWCLOSE	1
#define SIZEMARKOPTION_HOVERCLOSE	2
#define SIZEMARKOPTION_CLOSEDOWN	4

typedef struct {
    DWORD dwLocation;
    DWORD dwOptions;
    float fOpacity;
    RECT* lpRect;
    Gdiplus::RectF* lpCropRect;
    RECT* lpCloseRect;
} SIZEMARKOPTIONS, *PSIZEMARKOPTIONS;
typedef const PSIZEMARKOPTIONS PCSIZEMARKOPTIONS;

void InitSizeMarks(HINSTANCE hInstance);
void ShutdownSizeMarks();
void DrawSizeMarks(Gdiplus::Graphics* g, const PSIZEMARKOPTIONS options);
void ExpandForSizeMarks(const RECT* rect, RECT* newRect, int location);
void AdjustForSizeMarks(RECT &rect, int location);
void AdjustPointForSizeMarks(POINT &p, int location);
