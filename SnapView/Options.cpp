#include "stdafx.h"
#include "Options.h"
#include "SnapView.h"
#include "CaptureBox.h"
#include "SnapViewBase.h"

#include <string>

using namespace xercesc;

#define SETTINGS_FILENAME	_T("SnapViewSettings")
#define SETTINGS_EXTENSION	_T(".xml")

#define SETTINGS_NAMESPACE			TEXT("http://www.snapview.com/settings/2009")
#define SETTINGS_ROOT				TEXT("SnapView")
#define SETTINGS_MAXHISTORY			TEXT("MaxHistory")
#define SETTINGS_QUICKSAVEPATH		TEXT("QuickSavePath")
#define SETTINGS_DEFAULTSAVETYPE	TEXT("DefaultSaveType")
#define SETTINGS_HIDEONNEWSNAP		TEXT("HideOnNewSnap")
#define SETTINGS_SHOWHOVERINFO		TEXT("ShowHoverInfo")

#define MAXHISTORYWNDPROC_SETTING	_T("MaxHistoryWndProc")

OPTIONS options;

INT_PTR OptionsDialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

namespace
{
    class XmlString
    {
    public:
        explicit XmlString(const wchar_t* value)
        {
            while (*value)
            {
                value_.push_back(static_cast<XMLCh>(*value++));
            }
        }

        const XMLCh* c_str() const
        {
            return value_.c_str();
        }

    private:
        std::basic_string<XMLCh> value_;
    };

    std::wstring ToWideString(const XMLCh* value)
    {
        std::wstring result;
        if (!value)
        {
            return result;
        }

        while (*value)
        {
            result.push_back(static_cast<wchar_t>(*value++));
        }

        return result;
    }

    bool XmlEquals(const XMLCh* value, const wchar_t* expected)
    {
        if (!value)
        {
            return false;
        }

        XmlString expectedValue(expected);
        return XMLString::equals(value, expectedValue.c_str());
    }
}

bool GetSettingsFileName(LPTSTR szPath, bool createFolder)
{
    GetModuleFileName(NULL, szPath, MAX_PATH);

    TCHAR szDrive[_MAX_DRIVE], szDir[_MAX_DIR];
    _tsplitpath_s(szPath, szDrive, _MAX_DRIVE, szDir, _MAX_DIR, NULL, 0, NULL, 0);

    _tmakepath_s(szPath, MAX_PATH, szDrive, szDir, SETTINGS_FILENAME, SETTINGS_EXTENSION);

    if (_taccess(szPath, 0))
    {
        if (!SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, szPath)))
            return false;

        size_t i = _tcslen(szPath)-1;
        if (szPath[i] == _T('\\'))
            szPath[i] = _T('\0');

        _tcscat_s(szPath, MAX_PATH, _T("\\BurnettSoft"));
        if (createFolder && _taccess(szPath, 0))
            if (_tmkdir(szPath)) return false;

        _tcscat_s(szPath, MAX_PATH, _T("\\SnapView"));
        if (createFolder && _taccess(szPath, 0))
            if (_tmkdir(szPath)) return false;

        _tcscat_s(szPath, MAX_PATH, _T("\\SnapViewSettings.xml"));
    }

    return true;
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

    TCHAR szPath[MAX_PATH];
    if (!GetSettingsFileName(szPath, false)) return;

    XercesDOMParser* parser = new XercesDOMParser();
    parser->setDoNamespaces(true);

    ErrorHandler* errHandler = (ErrorHandler*) new HandlerBase();
    parser->setErrorHandler(errHandler);

    try {
        XmlString settingsPath(szPath);
        parser->parse(settingsPath.c_str());

        xercesc::DOMDocument* doc = parser->getDocument();
        DOMElement* root = doc->getDocumentElement();

        if (!XmlEquals(root->getNamespaceURI(), SETTINGS_NAMESPACE))
            return;

        if (!XmlEquals(root->getLocalName(), SETTINGS_ROOT))
            return;

        DOMNode* child = root->getFirstChild();
        while (child)
        {
            if (child->getNodeType() == DOMNode::ELEMENT_NODE)
            {
                DOMElement *element = (DOMElement*)child;
                if (XmlEquals(element->getNamespaceURI(), SETTINGS_NAMESPACE))
                {
                    const std::wstring value = ToWideString(element->getTextContent());

                    if (XmlEquals(element->getLocalName(), SETTINGS_MAXHISTORY))
                    {
                        options.maxHistory = _wtoi(value.c_str());

                        if (options.maxHistory > MAX_CAPTURE_HISTORY)
                            options.maxHistory = MAX_CAPTURE_HISTORY;
                        else if (options.maxHistory < 0)
                            options.maxHistory = 0;
                    }
                    else if (XmlEquals(element->getLocalName(), SETTINGS_QUICKSAVEPATH))
                    {
                        wcscpy_s(options.quickSavePath, MAX_PATH, value.c_str());
                    }
                    else if (XmlEquals(element->getLocalName(), SETTINGS_DEFAULTSAVETYPE))
                    {
                        options.defaultSaveType = _wtoi(value.c_str());

                        if (options.defaultSaveType > 4)
                            options.defaultSaveType = 1;
                        else if (options.maxHistory < 1)
                            options.defaultSaveType = 1;
                    }
                    else if (XmlEquals(element->getLocalName(), SETTINGS_HIDEONNEWSNAP))
                    {
                        options.hideOnNewSnap = _wtoi(value.c_str()) != 0;
                    }
                    else if (XmlEquals(element->getLocalName(), SETTINGS_SHOWHOVERINFO))
                    {
                        options.showHoverInfo = _wtoi(value.c_str()) != 0;
                    }
                }
            }

            child = child->getNextSibling();
        }
    }
    catch (...) {
    }

    delete parser;
    delete errHandler;
}

 int serializeDOM(DOMImplementation* impl, DOMNode* node, LPTSTR szFilename)
 {
    DOMLSSerializer* theSerializer = ((DOMImplementationLS*)impl)->createLSSerializer();

    if (theSerializer->getDomConfig()->canSetParameter(XMLUni::fgDOMWRTFormatPrettyPrint, true))
         theSerializer->getDomConfig()->setParameter(XMLUni::fgDOMWRTFormatPrettyPrint, true);

    XmlString filename(szFilename);
    XMLFormatTarget *myFormTarget = new LocalFileFormatTarget(filename.c_str());
    DOMLSOutput* theOutput = ((DOMImplementationLS*)impl)->createLSOutput();
    theOutput->setByteStream(myFormTarget);

    try {
        // do the serialization through DOMLSSerializer::write();
        theSerializer->write(node, theOutput);
    }
    catch (...) {
        theOutput->release();
        theSerializer->release();
        delete myFormTarget;
        return -1;
    }

    theOutput->release();
    theSerializer->release();
    delete myFormTarget;
    return 0;
}

bool SaveOptions(const POPTIONS newOptions)
{
    TCHAR szPath[MAX_PATH];
    if (!GetSettingsFileName(szPath, true)) return false;

    XmlString ls(L"LS");
    XmlString settingsNamespace(SETTINGS_NAMESPACE);
    XmlString settingsRoot(SETTINGS_ROOT);
    XmlString xmlVersion(L"1.0");
    DOMImplementation* impl = DOMImplementationRegistry::getDOMImplementation(ls.c_str());
    xercesc::DOMDocument* doc = impl->createDocument(
        settingsNamespace.c_str(),
        settingsRoot.c_str(),
        NULL);

    try
    {
        doc->setXmlVersion(xmlVersion.c_str());

        DOMElement* rootNode = doc->getDocumentElement();

        const auto appendElement = [doc, rootNode, &settingsNamespace](
            const wchar_t* name,
            const wchar_t* value)
        {
            XmlString xmlName(name);
            XmlString xmlValue(value);
            DOMElement* element = doc->createElementNS(
                settingsNamespace.c_str(),
                xmlName.c_str());
            element->setTextContent(xmlValue.c_str());
            rootNode->appendChild(element);
        };

        const std::wstring maxHistory = std::to_wstring(newOptions->maxHistory);
        appendElement(SETTINGS_MAXHISTORY, maxHistory.c_str());
        appendElement(SETTINGS_QUICKSAVEPATH, newOptions->quickSavePath);

        const std::wstring defaultSaveType = std::to_wstring(newOptions->defaultSaveType);
        appendElement(SETTINGS_DEFAULTSAVETYPE, defaultSaveType.c_str());
        appendElement(SETTINGS_HIDEONNEWSNAP, newOptions->hideOnNewSnap ? L"1" : L"0");
        appendElement(SETTINGS_SHOWHOVERINFO, newOptions->showHoverInfo ? L"1" : L"0");

        if (serializeDOM(impl, doc, szPath))
        {
            doc->release();
            //delete impl;
            return false;
        }
    }
    catch (...)
    {
        doc->release();
        //delete impl;
        return false;
    }

    doc->release();
    //delete impl;

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

    return SaveOptions(&newOptions);
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
        break;

    case WM_NCDESTROY:
        hForeWindow = NULL;

        PMAXHISTORYDATA maxHistoryData = (PMAXHISTORYDATA)GetProp(hDlg, MAXHISTORYWNDPROC_SETTING);
        delete maxHistoryData;
        RemoveProp(hDlg, MAXHISTORYWNDPROC_SETTING);
        break;
    }

    return (INT_PTR)FALSE;
}
