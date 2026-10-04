#pragma once

#include "stdafx.h"
#include "resource.h"
#include "SnapViewSettings.h"

#define MAX_CAPTURE_HISTORY 20
#define SAVETYPE_PNG		1
#define SAVETYPE_BMP		2
#define SAVETYPE_GIF		3
#define SAVETYPE_JPEG		4

static_assert(MAX_CAPTURE_HISTORY == SnapViewSettings::MaxCaptureHistory, "History limits must match.");
static_assert(SAVETYPE_PNG == SnapViewSettings::SaveTypePng && SAVETYPE_JPEG == SnapViewSettings::SaveTypeJpeg, "Save types must match.");

typedef struct
{
    int maxHistory;
    TCHAR quickSavePath[MAX_PATH];
    int defaultSaveType;
    bool hideOnNewSnap;
    bool showHoverInfo;
} OPTIONS, *POPTIONS;

extern OPTIONS options;

void LoadOptions();
