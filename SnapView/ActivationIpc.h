#pragma once

#include "stdafx.h"

bool StartActivationIpc(HWND targetWindow);
void StopActivationIpc();
void SetActivationOptionsProcessId(DWORD processId);
bool RequestShowOptionsFromRunningInstance();
