#include "stdafx.h"
#include "Options.h"
#include "SnapView.h"
#include "CaptureBox.h"
#include "SnapViewBase.h"

#include <string>

#define SETTINGS_MAXHISTORY			L"MaxHistory"
#define SETTINGS_QUICKSAVEPATH		L"QuickSavePath"
#define SETTINGS_DEFAULTSAVETYPE	L"DefaultSaveType"
#define SETTINGS_HIDEONNEWSNAP		L"HideOnNewSnap"
#define SETTINGS_SHOWHOVERINFO		L"ShowHoverInfo"

#define MAXHISTORYWNDPROC_SETTING	_T("MaxHistoryWndProc")
#define STARTUP_TASK_ID				L"SnapViewStartupTask"
#define WM_STARTUPTASKRESULT		(WM_APP + 1)

OPTIONS options;

INT_PTR OptionsDialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

namespace
{
    enum class StartupTaskOperation
    {
        Refresh,
        Update
    };

    struct StartupTaskResult
    {
        StartupTaskOperation operation;
        winrt::Windows::ApplicationModel::StartupTaskState state;
        bool succeeded;
    };

    bool TryGetInt32(
        const winrt::Windows::Foundation::Collections::IPropertySet& values,
        const wchar_t* key,
        int32_t& result)
    {
        const auto value = values.TryLookup(key).try_as<winrt::Windows::Foundation::IPropertyValue>();
        if (!value || value.Type() != winrt::Windows::Foundation::PropertyType::Int32)
        {
            return false;
        }

        result = value.GetInt32();
        return true;
    }

    bool TryGetBoolean(
        const winrt::Windows::Foundation::Collections::IPropertySet& values,
        const wchar_t* key,
        bool& result)
    {
        const auto value = values.TryLookup(key).try_as<winrt::Windows::Foundation::IPropertyValue>();
        if (!value || value.Type() != winrt::Windows::Foundation::PropertyType::Boolean)
        {
            return false;
        }

        result = value.GetBoolean();
        return true;
    }

    bool TryGetString(
        const winrt::Windows::Foundation::Collections::IPropertySet& values,
        const wchar_t* key,
        winrt::hstring& result)
    {
        const auto value = values.TryLookup(key).try_as<winrt::Windows::Foundation::IPropertyValue>();
        if (!value || value.Type() != winrt::Windows::Foundation::PropertyType::String)
        {
            return false;
        }

        result = value.GetString();
        return true;
    }

    void SetStartupTaskUnavailable(HWND hDlg, const wchar_t* message)
    {
        CheckDlgButton(hDlg, IDC_STARTWITHWINDOWS, BST_UNCHECKED);
        EnableWindow(GetDlgItem(hDlg, IDC_STARTWITHWINDOWS), FALSE);
        SetDlgItemText(hDlg, IDC_STARTUPTASKSTATUS, message);
    }

    void SetStartupTaskState(
        HWND hDlg,
        winrt::Windows::ApplicationModel::StartupTaskState state)
    {
        switch (state)
        {
        case winrt::Windows::ApplicationModel::StartupTaskState::Enabled:
            CheckDlgButton(hDlg, IDC_STARTWITHWINDOWS, BST_CHECKED);
            EnableWindow(GetDlgItem(hDlg, IDC_STARTWITHWINDOWS), TRUE);
            SetDlgItemText(hDlg, IDC_STARTUPTASKSTATUS, _T("SnapView will start when you sign in."));
            break;

        case winrt::Windows::ApplicationModel::StartupTaskState::Disabled:
            CheckDlgButton(hDlg, IDC_STARTWITHWINDOWS, BST_UNCHECKED);
            EnableWindow(GetDlgItem(hDlg, IDC_STARTWITHWINDOWS), TRUE);
            SetDlgItemText(hDlg, IDC_STARTUPTASKSTATUS, _T("SnapView will not start when you sign in."));
            break;

        case winrt::Windows::ApplicationModel::StartupTaskState::DisabledByUser:
            SetStartupTaskUnavailable(
                hDlg,
                _T("Startup was disabled in Windows. Re-enable SnapView in Task Manager's Startup apps tab or Settings > Apps > Startup."));
            break;

        case winrt::Windows::ApplicationModel::StartupTaskState::DisabledByPolicy:
            SetStartupTaskUnavailable(
                hDlg,
                _T("Startup is disabled by your organization. Contact your administrator to change it."));
            break;

        case winrt::Windows::ApplicationModel::StartupTaskState::EnabledByPolicy:
            CheckDlgButton(hDlg, IDC_STARTWITHWINDOWS, BST_CHECKED);
            EnableWindow(GetDlgItem(hDlg, IDC_STARTWITHWINDOWS), FALSE);
            SetDlgItemText(
                hDlg,
                IDC_STARTUPTASKSTATUS,
                _T("Startup is enabled by your organization and cannot be changed here."));
            break;

        default:
            SetStartupTaskUnavailable(hDlg, _T("The startup task is unavailable. Reinstall the SnapView MSIX package."));
            break;
        }
    }

    void PostStartupTaskResult(
        HWND hDlg,
        StartupTaskOperation operation,
        bool succeeded,
        winrt::Windows::ApplicationModel::StartupTaskState state =
            winrt::Windows::ApplicationModel::StartupTaskState::Disabled)
    {
        auto result = std::make_unique<StartupTaskResult>(
            StartupTaskResult{ operation, state, succeeded });
        if (PostMessage(hDlg, WM_STARTUPTASKRESULT, 0, reinterpret_cast<LPARAM>(result.get())))
            result.release();
    }

    void SetStartupTaskPending(HWND hDlg, const wchar_t* message)
    {
        EnableWindow(GetDlgItem(hDlg, IDC_STARTWITHWINDOWS), FALSE);
        SetDlgItemText(hDlg, IDC_STARTUPTASKSTATUS, message);
    }

    void RefreshStartupTaskControls(HWND hDlg)
    {
        SetStartupTaskPending(hDlg, _T("Checking startup setting..."));

        try
        {
            const auto operation = winrt::Windows::ApplicationModel::StartupTask::GetAsync(STARTUP_TASK_ID);
            operation.Completed(
                [hDlg](
                    const winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::ApplicationModel::StartupTask>& operation,
                    winrt::Windows::Foundation::AsyncStatus status)
                {
                    try
                    {
                        if (status != winrt::Windows::Foundation::AsyncStatus::Completed)
                            throw winrt::hresult_error(E_FAIL);

                        PostStartupTaskResult(
                            hDlg,
                            StartupTaskOperation::Refresh,
                            true,
                            operation.GetResults().State());
                    }
                    catch (const winrt::hresult_error&)
                    {
                        PostStartupTaskResult(hDlg, StartupTaskOperation::Refresh, false);
                    }
                });
        }
        catch (const winrt::hresult_error&)
        {
            SetStartupTaskUnavailable(hDlg, _T("The startup task is unavailable. Reinstall the SnapView MSIX package."));
        }
    }

    void UpdateStartupTask(HWND hDlg)
    {
        const bool enableStartup = IsDlgButtonChecked(hDlg, IDC_STARTWITHWINDOWS) == BST_CHECKED;
        SetStartupTaskPending(hDlg, _T("Updating startup setting..."));

        try
        {
            const auto operation = winrt::Windows::ApplicationModel::StartupTask::GetAsync(STARTUP_TASK_ID);
            operation.Completed(
                [hDlg, enableStartup](
                    const winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::ApplicationModel::StartupTask>& operation,
                    winrt::Windows::Foundation::AsyncStatus status)
                {
                    try
                    {
                        if (status != winrt::Windows::Foundation::AsyncStatus::Completed)
                            throw winrt::hresult_error(E_FAIL);

                        const auto startupTask = operation.GetResults();
                        if (!enableStartup)
                        {
                            startupTask.Disable();
                            PostStartupTaskResult(
                                hDlg,
                                StartupTaskOperation::Update,
                                true,
                                startupTask.State());
                            return;
                        }

                        const auto enableOperation = startupTask.RequestEnableAsync();
                        enableOperation.Completed(
                            [hDlg](
                                const winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::ApplicationModel::StartupTaskState>& operation,
                                winrt::Windows::Foundation::AsyncStatus status)
                            {
                                try
                                {
                                    if (status != winrt::Windows::Foundation::AsyncStatus::Completed)
                                        throw winrt::hresult_error(E_FAIL);

                                    PostStartupTaskResult(
                                        hDlg,
                                        StartupTaskOperation::Update,
                                        true,
                                        operation.GetResults());
                                }
                                catch (const winrt::hresult_error&)
                                {
                                    PostStartupTaskResult(hDlg, StartupTaskOperation::Update, false);
                                }
                            });
                    }
                    catch (const winrt::hresult_error&)
                    {
                        PostStartupTaskResult(hDlg, StartupTaskOperation::Update, false);
                    }
                });
        }
        catch (const winrt::hresult_error&)
        {
            SetStartupTaskUnavailable(hDlg, _T("SnapView could not update the startup setting. Try again after reinstalling the MSIX package."));
        }
    }
}

void SetDefaultOptions(POPTIONS defaults)
{
    memset(defaults, 0, sizeof(OPTIONS));

    defaults->maxHistory = 5;
    defaults->quickSavePath[0] = _T('\0');
    defaults->defaultSaveType = SAVETYPE_PNG;
    defaults->hideOnNewSnap = true;
    defaults->showHoverInfo = true;
}

void LoadOptions()
{
    SetDefaultOptions(&options);

    const auto values = winrt::Windows::Storage::ApplicationData::Current().LocalSettings().Values();
    int32_t value;
    if (TryGetInt32(values, SETTINGS_MAXHISTORY, value))
    {
        if (value > MAX_CAPTURE_HISTORY)
            options.maxHistory = MAX_CAPTURE_HISTORY;
        else if (value >= 0)
            options.maxHistory = value;
    }

    winrt::hstring quickSavePath;
    if (TryGetString(values, SETTINGS_QUICKSAVEPATH, quickSavePath) &&
        quickSavePath.size() < MAX_PATH)
    {
        wcscpy_s(options.quickSavePath, MAX_PATH, quickSavePath.c_str());
    }

    if (TryGetInt32(values, SETTINGS_DEFAULTSAVETYPE, value))
    {
        options.defaultSaveType = value >= SAVETYPE_PNG && value <= SAVETYPE_JPEG
            ? value
            : SAVETYPE_PNG;
    }

    bool boolValue;
    if (TryGetBoolean(values, SETTINGS_HIDEONNEWSNAP, boolValue))
        options.hideOnNewSnap = boolValue;

    if (TryGetBoolean(values, SETTINGS_SHOWHOVERINFO, boolValue))
        options.showHoverInfo = boolValue;
}

bool SaveOptions(const POPTIONS newOptions)
{
    try
    {
        const auto values = winrt::Windows::Storage::ApplicationData::Current().LocalSettings().Values();
        values.Insert(SETTINGS_MAXHISTORY, winrt::box_value(newOptions->maxHistory));
        values.Insert(SETTINGS_QUICKSAVEPATH, winrt::box_value(winrt::hstring(newOptions->quickSavePath)));
        values.Insert(SETTINGS_DEFAULTSAVETYPE, winrt::box_value(newOptions->defaultSaveType));
        values.Insert(SETTINGS_HIDEONNEWSNAP, winrt::box_value(newOptions->hideOnNewSnap));
        values.Insert(SETTINGS_SHOWHOVERINFO, winrt::box_value(newOptions->showHoverInfo));
    }
    catch (const winrt::hresult_error&)
    {
        return false;
    }

    options = *newOptions;
    TrimCaptureHistory(options.maxHistory);
    return true;
}

void ShowOptionError(HWND hWnd, const LPTSTR message)
{
    MessageBox(hWnd, message, _T("Error"), MB_OK | MB_ICONERROR);
}

bool SaveOptionsFromDialog(HWND hDlg)
{
    OPTIONS newOptions;
    memset(&newOptions, 0, sizeof(newOptions));

    HWND hWnd = GetDlgItem(hDlg, IDC_MAXHISTORYUPDWN);
    BOOL bError;
    newOptions.maxHistory = (int)SendMessage(hWnd, UDM_GETPOS32, 0, (LPARAM)&bError);
    if (bError || newOptions.maxHistory > MAX_CAPTURE_HISTORY)
    {
        ShowOptionError(hDlg, _T("Invalid Number Of Snaps To Keep In History"));
        SetFocus(GetDlgItem(hDlg, IDC_MAXHISTORY));
        return false;
    }

    newOptions.defaultSaveType = ComboBox_GetCurSel(GetDlgItem(hDlg, IDC_DEFAULTSAVETYPE))+1;

    hWnd = GetDlgItem(hDlg, IDC_QUICKSAVEPATH);
    Edit_GetText(hWnd, newOptions.quickSavePath, MAX_PATH);
    if (_taccess(newOptions.quickSavePath, 0))
    {
        ShowOptionError(hDlg, _T("Invalid Quick Save Folder"));
        SetFocus(hWnd);
        return false;
    }

    newOptions.hideOnNewSnap = IsDlgButtonChecked(hDlg, IDC_HIDEONNEWSNAP) == BST_CHECKED;
    newOptions.showHoverInfo = IsDlgButtonChecked(hDlg, IDC_SHOWHOVERINFO) == BST_CHECKED;

    if (!SaveOptions(&newOptions))
    {
        ShowOptionError(hDlg, _T("Unable to save settings."));
        return false;
    }

    return true;
}

int CALLBACK BrowseCallbackProc(HWND hWnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
    UNREFERENCED_PARAMETER(lParam);

    switch (uMsg)
    {
    case BFFM_INITIALIZED:
        SendMessage(hWnd, BFFM_SETSELECTION, TRUE, lpData);
        break;
    }

    return 0;
}

BOOL BrowseForFolder(HWND hWnd, LPTSTR szFolderName)
{
    BROWSEINFO bi;
    memset(&bi, 0, sizeof(bi));

    bi.hwndOwner = hWnd;
    bi.lpszTitle = _T("Select the folder where you would like your quick saves to be placed");
    bi.lpfn = (BFFCALLBACK)&BrowseCallbackProc;
    bi.lParam = (LPARAM)szFolderName;
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;

    PIDLIST_ABSOLUTE idList = SHBrowseForFolder(&bi);
    if (!idList) return false;

    BOOL res = SHGetPathFromIDList(idList, szFolderName);
    CoTaskMemFree(idList);
    return res;
}

LRESULT CALLBACK MaxHistoryWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    HWND hDlg = GetAncestor(hWnd, GA_ROOT);
    PMAXHISTORYDATA maxHistoryData = (PMAXHISTORYDATA)GetProp(hDlg, MAXHISTORYWNDPROC_SETTING);

    switch (uMsg)
    {
    case WM_KILLFOCUS:
        HWND hUpDwn = GetDlgItem(hDlg, IDC_MAXHISTORYUPDWN);
        BOOL bError;
        int maxHistory = (int)SendMessage(hUpDwn, UDM_GETPOS32, 0, (LPARAM)&bError);
        if (bError)
            SendMessage(hUpDwn, UDM_SETPOS32, 0, maxHistoryData->prevValue);
        else if (maxHistory < 0)
            SendMessage(hUpDwn, UDM_SETPOS32, 0, 0);
        else if (maxHistory > MAX_CAPTURE_HISTORY)
            SendMessage(hUpDwn, UDM_SETPOS32, 0, MAX_CAPTURE_HISTORY);
        else
            maxHistoryData->prevValue = maxHistory;
    }

    return CallWindowProc(maxHistoryData->lpfnWndProc, hWnd, uMsg, wParam, lParam);
}

void InitOptionsDialog(HWND hDlg)
{
    TCHAR szTemp[MAX_PATH];

    HWND hWnd = GetDlgItem(hDlg, IDC_MAXHISTORY);
    Edit_LimitText(hWnd, 2);

    PMAXHISTORYDATA maxHistoryData = new MAXHISTORYDATA();
    maxHistoryData->lpfnWndProc = (WNDPROC)SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)&MaxHistoryWndProc);
    maxHistoryData->prevValue = options.maxHistory;
    SetProp(hDlg, MAXHISTORYWNDPROC_SETTING, (HANDLE)maxHistoryData);

    hWnd = GetDlgItem(hDlg, IDC_MAXHISTORYUPDWN);
    SendMessage(hWnd, UDM_SETRANGE32, 0, MAX_CAPTURE_HISTORY);
    SendMessage(hWnd, UDM_SETPOS32, 0, options.maxHistory);
    //_stprintf_s(szTemp, MAX_PATH, _T("%d"), options.maxHistory);
    //Edit_SetText(hWnd, szTemp);

    hWnd = GetDlgItem(hDlg, IDC_DEFAULTSAVETYPE);
    ComboBox_AddItemData(hWnd, (LPARAM)_T("Portable Network Graphics (PNG)"));
    ComboBox_AddItemData(hWnd, (LPARAM)_T("Windows Bitmap (BMP)"));
    ComboBox_AddItemData(hWnd, (LPARAM)_T("Graphics Interchange Format (GIF)"));
    ComboBox_AddItemData(hWnd, (LPARAM)_T("JPEG"));
    ComboBox_SetCurSel(hWnd, options.defaultSaveType-1);

    hWnd = GetDlgItem(hDlg, IDC_QUICKSAVEPATH);
    Edit_LimitText(hWnd, MAX_PATH);
    if (options.quickSavePath[0] != _T('\0'))
        Edit_SetText(hWnd, options.quickSavePath);
    else
    {
        if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_MYPICTURES, NULL, SHGFP_TYPE_CURRENT, szTemp)))
            Edit_SetText(hWnd, szTemp);
    }

    CheckDlgButton(hDlg, IDC_HIDEONNEWSNAP, options.hideOnNewSnap ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_SHOWHOVERINFO, options.showHoverInfo ? BST_CHECKED : BST_UNCHECKED);
    RefreshStartupTaskControls(hDlg);
}

INT_PTR ShowOptionsDialog(HWND hWnd)
{
    return DialogBox(hInst, MAKEINTRESOURCE(IDD_OPTIONS), hWnd, (DLGPROC)&OptionsDialogProc);
}

INT_PTR OptionsDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    switch (message)
    {
    case WM_INITDIALOG:
        {
            RECT rect;
            GetWindowRect(hDlg, &rect);

            POINT cursor;
            GetCursorPos(&cursor);

            MONITORINFO monitorInfo;
            monitorInfo.cbSize = sizeof(monitorInfo);
            GetMonitorInfo(MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST), &monitorInfo);

            int x = monitorInfo.rcWork.left + ((monitorInfo.rcWork.right - monitorInfo.rcWork.left) - (rect.right - rect.left)) / 2;
            int y = monitorInfo.rcWork.top + ((monitorInfo.rcWork.bottom - monitorInfo.rcWork.top) - (rect.bottom - rect.top)) / 2;
            SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOOWNERZORDER);

            InitOptionsDialog(hDlg);

            SendMessage(hDlg, WM_SETICON, ICON_BIG, (LPARAM)hIconLarge);

            hForeWindow = hDlg;
            SetForegroundWindow(hDlg);

            return (INT_PTR)TRUE;
        }
    case WM_STARTUPTASKRESULT:
        {
            std::unique_ptr<StartupTaskResult> result(
                reinterpret_cast<StartupTaskResult*>(lParam));
            if (result->succeeded)
            {
                SetStartupTaskState(hDlg, result->state);
            }
            else if (result->operation == StartupTaskOperation::Refresh)
            {
                SetStartupTaskUnavailable(
                    hDlg,
                    _T("The startup task is unavailable. Reinstall the SnapView MSIX package."));
            }
            else
            {
                SetStartupTaskUnavailable(
                    hDlg,
                    _T("SnapView could not update the startup setting. Try again after reinstalling the MSIX package."));
            }

            return (INT_PTR)TRUE;
        }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            if (LOWORD(wParam) == IDOK)
                if (!SaveOptionsFromDialog(hDlg)) return (INT_PTR)TRUE;

            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        else if (LOWORD(wParam) == IDC_CLEARHISTORY)
        {
            ClearCaptureHistory();
            MessageBox(hDlg, _T("History Cleared"), _T("Information"), MB_OK | MB_ICONINFORMATION);
            return (INT_PTR)TRUE;
        }
        else if (LOWORD(wParam) == IDC_QUICKSAVEPATHBROWSE)
        {
            TCHAR szFolder[MAX_PATH];
            HWND hWnd = GetDlgItem(hDlg, IDC_QUICKSAVEPATH);
            Edit_GetText(hWnd, szFolder, MAX_PATH);
            if (BrowseForFolder(hDlg, szFolder))
                Edit_SetText(hWnd, szFolder);
            return (INT_PTR)TRUE;
        }
        else if (LOWORD(wParam) == IDC_STARTWITHWINDOWS && HIWORD(wParam) == BN_CLICKED)
        {
            UpdateStartupTask(hDlg);
            return (INT_PTR)TRUE;
        }
        break;

    case WM_NCDESTROY:
        hForeWindow = NULL;

        PMAXHISTORYDATA maxHistoryData = (PMAXHISTORYDATA)GetProp(hDlg, MAXHISTORYWNDPROC_SETTING);
        delete maxHistoryData;
        RemoveProp(hDlg, MAXHISTORYWNDPROC_SETTING);

        MSG pendingMessage;
        while (PeekMessage(
            &pendingMessage,
            hDlg,
            WM_STARTUPTASKRESULT,
            WM_STARTUPTASKRESULT,
            PM_REMOVE))
        {
            delete reinterpret_cast<StartupTaskResult*>(pendingMessage.lParam);
        }
        break;
    }

    return (INT_PTR)FALSE;
}
