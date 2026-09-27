#pragma once

// SnapView requires Windows 10 version 1703 or later for Per-Monitor V2 DPI awareness.

// Modify the following defines if you have to target a platform prior to the ones specified below.
// Refer to MSDN for the latest info on corresponding values for different platforms.
#ifndef WINVER
#define WINVER 0x0A00
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#ifndef NTDDI_VERSION
#define NTDDI_VERSION NTDDI_WIN10_RS2
#endif
