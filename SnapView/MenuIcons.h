#pragma once

#include "stdafx.h"

// Menu icons are Segoe Fluent Icons glyphs (Segoe MDL2 Assets on Windows 10)
// rendered in the SnapView blue. GDI+ must be started before InitMenuIcons,
// and the menus must be destroyed before CleanupMenuIcons.

void InitMenuIcons();
void ApplyMenuIcons(HMENU hMenu);
void CleanupMenuIcons();
