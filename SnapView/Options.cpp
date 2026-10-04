#include "stdafx.h"
#include "Options.h"

OPTIONS options;

namespace
{
    bool optionsLoaded = false;

    void SetDefaultOptions(POPTIONS defaults)
    {
        memset(defaults, 0, sizeof(OPTIONS));

        const SnapViewSettings::Settings settings;
        defaults->maxHistory = settings.maxHistory;
        defaults->quickSavePath[0] = _T('\0');
        defaults->defaultSaveType = settings.defaultSaveType;
        defaults->hideOnNewSnap = settings.hideOnNewSnap;
        defaults->showHoverInfo = settings.showHoverInfo;
    }
}

// Settings are edited by SnapViewOptions.exe. The tray process only reads
// them, at startup and whenever the Options process reports a change.
void LoadOptions()
{
    SnapViewSettings::Settings settings;
    try
    {
        settings = SnapViewSettings::Load();
    }
    catch (const winrt::hresult_error&)
    {
        // Keep the last loaded values; use defaults only on the first load.
        if (!optionsLoaded)
            SetDefaultOptions(&options);
        optionsLoaded = true;
        return;
    }

    optionsLoaded = true;
    options.maxHistory = settings.maxHistory;
    wcscpy_s(options.quickSavePath, MAX_PATH, settings.quickSavePath.c_str());
    options.defaultSaveType = settings.defaultSaveType;
    options.hideOnNewSnap = settings.hideOnNewSnap;
    options.showHoverInfo = settings.showHoverInfo;
}
