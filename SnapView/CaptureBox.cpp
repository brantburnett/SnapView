#include "stdafx.h"
#include "CaptureBox.h"
#include "SnapView.h"
#include "SnapViewBase.h"
#include "Options.h"
#include "Share.h"
#include "SizeMarks.h"

#include <vector>

using namespace Gdiplus;

#define MAX_LOADSTRING		100
#define MAX_FILTERSTRING	512
#define MIN_CAPTURE_SIZE	15
#define CLOSE_HEIGHT		19
#define CLOSE_WIDTH			24
#define CROP_SIZE			5
#define FRAME_OPACITY		(float)0.4

#define GWLP_INFO			GWLP_USERDATA

TCHAR szCaptureBoxWindowClass[MAX_LOADSTRING];			// the main window class name
TCHAR szSaveFilter[MAX_FILTERSTRING];
PCAPTUREBOXCLOSEINFO prevCaptureBox[MAX_CAPTURE_HISTORY];
int savedCaptureBoxes = 0;
PCAPTUREBOXWINDOW openCaptureBoxes = NULL;
std::vector<std::wstring> dragDropFiles;

const SolidBrush* pTransparentBrush;
const SolidBrush* pCropBrush;
const Pen* pBorderPen;

LRESULT CALLBACK	CaptureBoxWndProc(HWND, UINT, WPARAM, LPARAM);
void				SetTracking(HWND hWnd, PCAPTUREBOXINFO info);
void				AnimateFrameFade(HWND hWnd, PCAPTUREBOXINFO info, float targetOpacity);

class CaptureFormatEnumerator : public IEnumFORMATETC
{
public:
    CaptureFormatEnumerator() : referenceCount(1), index(0)
    {
        format.cfFormat = CF_HDROP;
        format.ptd = NULL;
        format.dwAspect = DVASPECT_CONTENT;
        format.lindex = -1;
        format.tymed = TYMED_HGLOBAL;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID requestedInterface, void** object) override
    {
        if (!object)
            return E_POINTER;

        if (requestedInterface == IID_IUnknown || requestedInterface == IID_IEnumFORMATETC)
        {
            *object = static_cast<IEnumFORMATETC*>(this);
            AddRef();
            return S_OK;
        }

        *object = NULL;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return InterlockedIncrement(&referenceCount);
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        ULONG count = InterlockedDecrement(&referenceCount);
        if (!count)
            delete this;
        return count;
    }

    HRESULT STDMETHODCALLTYPE Next(ULONG count, FORMATETC* formats, ULONG* fetched) override
    {
        if (!formats || (count != 1 && !fetched))
            return E_INVALIDARG;

        if (fetched)
            *fetched = 0;

        ULONG returned = 0;
        while (returned < count && !index)
        {
            formats[returned] = format;
            ++returned;
            ++index;
        }

        if (fetched)
            *fetched = returned;
        return returned == count ? S_OK : S_FALSE;
    }

    HRESULT STDMETHODCALLTYPE Skip(ULONG count) override
    {
        const ULONG remaining = 1 - index;
        if (count > remaining)
        {
            index = 1;
            return S_FALSE;
        }

        index += count;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Reset() override
    {
        index = 0;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Clone(IEnumFORMATETC** clone) override
    {
        if (!clone)
            return E_POINTER;

        CaptureFormatEnumerator* enumerator = new CaptureFormatEnumerator();
        enumerator->index = index;
        *clone = enumerator;
        return S_OK;
    }

private:
    LONG referenceCount;
    ULONG index;
    FORMATETC format;
};

class CaptureFileDataObject : public IDataObject
{
public:
    explicit CaptureFileDataObject(const std::wstring& path) : referenceCount(1), dropFiles(CreateDropFiles(path))
    {
    }

    ~CaptureFileDataObject()
    {
        if (dropFiles)
            GlobalFree(dropFiles);
    }

    bool IsValid() const
    {
        return dropFiles != NULL;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID requestedInterface, void** object) override
    {
        if (!object)
            return E_POINTER;

        if (requestedInterface == IID_IUnknown || requestedInterface == IID_IDataObject)
        {
            *object = static_cast<IDataObject*>(this);
            AddRef();
            return S_OK;
        }

        *object = NULL;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return InterlockedIncrement(&referenceCount);
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        ULONG count = InterlockedDecrement(&referenceCount);
        if (!count)
            delete this;
        return count;
    }

    HRESULT STDMETHODCALLTYPE GetData(FORMATETC* requestedFormat, STGMEDIUM* medium) override
    {
        if (!medium)
            return E_POINTER;

        ZeroMemory(medium, sizeof(*medium));
        if (!SupportsFormat(requestedFormat))
            return DV_E_FORMATETC;

        HGLOBAL copy = CopyGlobal(dropFiles);
        if (!copy)
            return E_OUTOFMEMORY;

        medium->tymed = TYMED_HGLOBAL;
        medium->hGlobal = copy;
        medium->pUnkForRelease = NULL;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC*, STGMEDIUM*) override
    {
        return DATA_E_FORMATETC;
    }

    HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC* requestedFormat) override
    {
        return SupportsFormat(requestedFormat) ? S_OK : DV_E_FORMATETC;
    }

    HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC*, FORMATETC* equivalentFormat) override
    {
        if (!equivalentFormat)
            return E_POINTER;

        equivalentFormat->ptd = NULL;
        return DATA_S_SAMEFORMATETC;
    }

    HRESULT STDMETHODCALLTYPE SetData(FORMATETC*, STGMEDIUM*, BOOL) override
    {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD direction, IEnumFORMATETC** enumerator) override
    {
        if (!enumerator)
            return E_POINTER;
        if (direction != DATADIR_GET)
            return E_NOTIMPL;

        *enumerator = new CaptureFormatEnumerator();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC*, DWORD, IAdviseSink*, DWORD*) override
    {
        return OLE_E_ADVISENOTSUPPORTED;
    }

    HRESULT STDMETHODCALLTYPE DUnadvise(DWORD) override
    {
        return OLE_E_ADVISENOTSUPPORTED;
    }

    HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA**) override
    {
        return OLE_E_ADVISENOTSUPPORTED;
    }

private:
    static HGLOBAL CreateDropFiles(const std::wstring& path)
    {
        const SIZE_T bytes = sizeof(DROPFILES) + (path.length() + 2) * sizeof(wchar_t);
        HGLOBAL memory = GlobalAlloc(GHND, bytes);
        if (!memory)
            return NULL;

        DROPFILES* files = static_cast<DROPFILES*>(GlobalLock(memory));
        if (!files)
        {
            GlobalFree(memory);
            return NULL;
        }

        files->pFiles = sizeof(DROPFILES);
        files->fWide = TRUE;
        memcpy(static_cast<BYTE*>(static_cast<void*>(files)) + files->pFiles,
            path.c_str(), (path.length() + 1) * sizeof(wchar_t));
        GlobalUnlock(memory);
        return memory;
    }

    static HGLOBAL CopyGlobal(HGLOBAL source)
    {
        const SIZE_T bytes = GlobalSize(source);
        HGLOBAL copy = GlobalAlloc(GHND, bytes);
        if (!copy)
            return NULL;

        const void* sourceData = GlobalLock(source);
        void* copyData = GlobalLock(copy);
        if (!sourceData || !copyData)
        {
            if (sourceData)
                GlobalUnlock(source);
            if (copyData)
                GlobalUnlock(copy);
            GlobalFree(copy);
            return NULL;
        }

        memcpy(copyData, sourceData, bytes);
        GlobalUnlock(copy);
        GlobalUnlock(source);
        return copy;
    }

    static bool SupportsFormat(const FORMATETC* format)
    {
        return format && format->cfFormat == CF_HDROP &&
            format->dwAspect == DVASPECT_CONTENT &&
            (format->tymed & TYMED_HGLOBAL);
    }

    LONG referenceCount;
    HGLOBAL dropFiles;
};

class CaptureDropSource : public IDropSource
{
public:
    CaptureDropSource() : referenceCount(1)
    {
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID requestedInterface, void** object) override
    {
        if (!object)
            return E_POINTER;

        if (requestedInterface == IID_IUnknown || requestedInterface == IID_IDropSource)
        {
            *object = static_cast<IDropSource*>(this);
            AddRef();
            return S_OK;
        }

        *object = NULL;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return InterlockedIncrement(&referenceCount);
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        ULONG count = InterlockedDecrement(&referenceCount);
        if (!count)
            delete this;
        return count;
    }

    HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL escapePressed, DWORD keyState) override
    {
        if (escapePressed)
            return DRAGDROP_S_CANCEL;
        return keyState & MK_LBUTTON ? S_OK : DRAGDROP_S_DROP;
    }

    HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD) override
    {
        return DRAGDROP_S_USEDEFAULTCURSORS;
    }

private:
    LONG referenceCount;
};

ATOM RegisterCaptureBoxClass(HINSTANCE hInstance)
{
    LoadString(hInstance, IDC_CAPTUREBOX, szCaptureBoxWindowClass, MAX_LOADSTRING);
    LoadString(hInstance, IDS_FILE_FILTER, szSaveFilter, MAX_FILTERSTRING);

    for (int i=0; i<MAX_FILTERSTRING; i++)
    {
        if (szSaveFilter[i] == _T('\0'))
            break;
        else if (szSaveFilter[i] == _T('\t'))
            szSaveFilter[i] = _T('\0');
    }

    WNDCLASSEX wcex;
    memset(&wcex, 0, sizeof(WNDCLASSEX));

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style			= CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc	= CaptureBoxWndProc;
    wcex.cbClsExtra		= 0;
    wcex.cbWndExtra		= sizeof(PCAPTUREBOXINFO);
    wcex.hInstance		= hInstance;
    wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SNAPVIEW));
    wcex.hCursor		= hCursorMove;
    wcex.lpszClassName	= szCaptureBoxWindowClass;

    pTransparentBrush = new SolidBrush(Color::Transparent);
    pCropBrush = new SolidBrush(Color(0x44, 0, 0, 0));
    pBorderPen = new Pen(Color(0, 0, 0), 1.0);

    return RegisterClassEx(&wcex);
}

void CleanupCaptureBoxResources()
{
    delete pTransparentBrush;
    pTransparentBrush = NULL;

    delete pCropBrush;
    pCropBrush = NULL;

    delete pBorderPen;
    pBorderPen = NULL;

    for (const std::wstring& path : dragDropFiles)
        DeleteFile(path.c_str());
    dragDropFiles.clear();
}

PCAPTUREBOXWINDOW AddCaptureBoxWindow(HWND hWnd)
{
    PCAPTUREBOXWINDOW w = new CAPTUREBOXWINDOW();
    w->hWnd = hWnd;
    w->next = openCaptureBoxes;
    openCaptureBoxes = w;
    return w;
}

HWND CreateCaptureBox(Bitmap *bitmap, RECT r)
{
    HWND hWnd = CreateWindowEx(WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_LAYERED, szCaptureBoxWindowClass, _T("SNAP"), WS_POPUP,
        r.left-1, r.top-1, r.right-r.left+2, r.bottom-r.top+2, 0, 0, hInst, NULL);

    PCAPTUREBOXINFO info = new CAPTUREBOXINFO();
    memset(info, 0, sizeof(CAPTUREBOXINFO));
    info->bitmap = bitmap;
    info->size.x = r.right-r.left;
    info->size.y = r.bottom-r.top;
    //info->cropRect.X = 0;
    //info->cropRect.Y = 0;
    info->cropRect.Width = (REAL)info->size.x;
    info->cropRect.Height = (REAL)info->size.y;
    info->centerMoved = true;
    //info->scaleIndex = 0;
    info->scale = 1.0;
    info->curScale = 1.0;
    //info->frameOpacity = 0.0;
    info->moving = false;
    info->captured = false;
    info->moveType = MOVETYPE_MOVE;
    SetWindowLongPtr(hWnd, GWLP_INFO, (LONG_PTR)info);

    AddCaptureBoxWindow(hWnd);
    ShowWindow(hWnd, SW_NORMAL);

    return hWnd;
}

HWND ReopenPrevCaptureBox() {
    if (!savedCaptureBoxes) return 0;
    PCAPTUREBOXCLOSEINFO p = prevCaptureBox[savedCaptureBoxes-1];

    RECT r = p->rLocation;
    HWND hWnd = CreateWindowEx(WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_LAYERED, szCaptureBoxWindowClass, _T("SNAP"), WS_POPUP,
        r.left, r.top, r.right-r.left, r.bottom-r.top, 0, 0, hInst, NULL);

    SetWindowLongPtr(hWnd, GWLP_INFO, (LONG_PTR)p->info);
    p->info->moveType = MOVETYPE_MOVE;
    p->info->moving = false;
    p->info->captured = false;
    p->info->isInClose = false;
    p->info->trackingMouse = false;
    p->info->frameOpacity = 0.0;

    AddCaptureBoxWindow(hWnd);
    ShowWindow(hWnd, SW_NORMAL);

    POINT pt;
    GetCursorPos(&pt);

    if (SendMessage(hWnd, WM_NCHITTEST, 0, MAKELPARAM(pt.x, pt.y)) != HTNOWHERE)
    {
        SetTracking(hWnd, p->info);
        if (options.showHoverInfo)
            AnimateFrameFade(hWnd, p->info, FRAME_OPACITY);
    }

    delete p;
    savedCaptureBoxes--;

    return hWnd;
}

void CloseAllCaptureBoxes()
{
    while (openCaptureBoxes)
    {
        PCAPTUREBOXWINDOW w = openCaptureBoxes;
        openCaptureBoxes = w->next;

        DestroyWindow(w->hWnd);
        delete w;
    }
}

void ShowAllCaptureBoxes()
{
    stopCaptures = false;

    PCAPTUREBOXWINDOW w = openCaptureBoxes;
    while (w)
    {
        ShowWindow(w->hWnd, SW_SHOW);
        w = w->next;
    }
}

void HideAllCaptureBoxes(bool forDialog)
{
    if (forDialog)
        stopCaptures = true;

    PCAPTUREBOXWINDOW w = openCaptureBoxes;
    while (w)
    {
        ShowWindow(w->hWnd, SW_HIDE);
        w = w->next;
    }
}

void SaveCaptureBox(PCAPTUREBOXCLOSEINFO info)
{
    if (savedCaptureBoxes == options.maxHistory)
    {
        PCAPTUREBOXCLOSEINFO p = prevCaptureBox[0];
        if (p)
        {
            delete p->info->bitmap;
            delete p->info;
            delete p;
        }

        for (int i=0; i<options.maxHistory-1; i++)
            prevCaptureBox[i] = prevCaptureBox[i+1];
        savedCaptureBoxes--;
    }

    prevCaptureBox[savedCaptureBoxes] = info;
    savedCaptureBoxes++;
}

void ClearCaptureHistory()
{
    for (int i=0; i<savedCaptureBoxes; i++)
    {
        PCAPTUREBOXCLOSEINFO p = prevCaptureBox[i];
        if (p)
        {
            delete p->info->bitmap;
            delete p->info;
            delete p;
        }
    }

    savedCaptureBoxes = 0;
}

void TrimCaptureHistory(int maxHistory)
{
    if (maxHistory == 0)
        ClearCaptureHistory();
    else if (maxHistory < savedCaptureBoxes)
    {
        int diff = savedCaptureBoxes - maxHistory;

        for (int i=0; i<diff; i++)
        {
            PCAPTUREBOXCLOSEINFO p = prevCaptureBox[i];
            if (p)
            {
                delete p->info->bitmap;
                delete p->info;
                delete p;
            }
        }

        for (int i=0; i<savedCaptureBoxes-diff; i++)
            prevCaptureBox[i] = prevCaptureBox[i + diff];

        savedCaptureBoxes = maxHistory;
    }
}

void LockClipboardData(HWND hWnd)
{
    OpenClipboard(hWnd);
    int i = 0;
    HANDLE hData;
    do
    {
        i = EnumClipboardFormats(i);
        if (i)
            hData = GetClipboardData(i);
    }
    while (i);
    CloseClipboard();
}

void CopyToClipboard(HWND hWnd)
{
    PCAPTUREBOXINFO info = (PCAPTUREBOXINFO)GetWindowLongPtr(hWnd, GWLP_INFO);

    Graphics *g = new Graphics(hWnd);
    Bitmap *bitmap = new Bitmap((int)ceilf(info->cropRect.Width), (int)ceilf(info->cropRect.Height), g);
    delete g;

    g = new Graphics(bitmap);
    g->DrawImage(info->bitmap, 0.0, 0.0, info->cropRect.X, info->cropRect.Y, info->cropRect.Width, info->cropRect.Height, UnitPixel);
    delete g;

    HBITMAP hBitmap;
    if (bitmap->GetHBITMAP(Color(0xff, 0xff, 0xff), &hBitmap) == Gdiplus::Ok)
    {
        if (!OpenClipboard(hWnd)) return;
        EmptyClipboard();
        SetClipboardData(CF_BITMAP, hBitmap);
        CloseClipboard();
        LockClipboardData(hWnd);
        DeleteObject(hBitmap);
    }

    delete bitmap;
}

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
   UINT  num = 0;          // number of image encoders
   UINT  size = 0;         // size of the image encoder array in bytes

   ImageCodecInfo* pImageCodecInfo = NULL;

   GetImageEncodersSize(&num, &size);
   if(size == 0)
      return -1;  // Failure

   pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
   if(pImageCodecInfo == NULL)
      return -1;  // Failure

   GetImageEncoders(num, size, pImageCodecInfo);

   for(UINT j = 0; j < num; ++j)
   {
      if( wcscmp(pImageCodecInfo[j].MimeType, format) == 0 )
      {
         *pClsid = pImageCodecInfo[j].Clsid;
         free(pImageCodecInfo);
         return j;  // Success
      }
   }

   free(pImageCodecInfo);
   return -1;  // Failure
}

bool SaveFile(HWND hWnd, LPCTSTR szPath, int fileType)
{
    PCAPTUREBOXINFO info = (PCAPTUREBOXINFO)GetWindowLongPtr(hWnd, GWLP_INFO);

    Graphics *g = new Graphics(hWnd);
    Bitmap *bitmap = new Bitmap((int)ceilf(info->cropRect.Width), (int)ceilf(info->cropRect.Height), g);
    delete g;

    g = new Graphics(bitmap);
    g->DrawImage(info->bitmap, 0.0, 0.0, info->cropRect.X, info->cropRect.Y, info->cropRect.Width, info->cropRect.Height, UnitPixel);
    delete g;

    CLSID clsidEncoder;
    switch (fileType)
    {
    case SAVETYPE_PNG:
        GetEncoderClsid(_T("image/png"), &clsidEncoder);
        break;
    case SAVETYPE_BMP:
        GetEncoderClsid(_T("image/bmp"), &clsidEncoder);
        break;
    case SAVETYPE_GIF:
        GetEncoderClsid(_T("image/gif"), &clsidEncoder);
        break;
    case SAVETYPE_JPEG:
        GetEncoderClsid(_T("image/jpeg"), &clsidEncoder);
        break;
    }

    Status status = bitmap->Save(szPath, &clsidEncoder);

    delete bitmap;

    if (status != Ok)
    {
        LPCTSTR message = _T("The image could not be saved.");
        if (status == AccessDenied)
            message = _T("Access to the selected save location was denied.");
        else if (status == OutOfMemory)
            message = _T("There is not enough memory to save the image.");
        else if (status == FileNotFound)
            message = _T("The selected save location could not be found.");

        MessageBox(hWnd, message, _T("Save Error"), MB_OK | MB_ICONERROR);
    }

    return status == Ok;
}

void SaveCaptureBox(HWND hWnd)
{
    COMDLG_FILTERSPEC filters[SAVETYPE_JPEG];
    const TCHAR* filterString = szSaveFilter;
    for (int i = 0; i < SAVETYPE_JPEG; i++)
    {
        filters[i].pszName = filterString;
        filterString += _tcslen(filterString) + 1;
        filters[i].pszSpec = filterString;
        filterString += _tcslen(filterString) + 1;
    }

    const TCHAR* defaultExtensions[] = { _T("png"), _T("bmp"), _T("gif"), _T("jpeg") };

    IFileSaveDialog* dialog = NULL;
    HRESULT hr = CoCreateInstance(CLSID_FileSaveDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
    if (FAILED(hr))
    {
        MessageBox(hWnd, _T("Unable to display the Save As dialog."), _T("Save Error"), MB_OK | MB_ICONERROR);
        return;
    }

    DWORD dialogOptions;
    hr = dialog->GetOptions(&dialogOptions);
    if (SUCCEEDED(hr))
        hr = dialog->SetOptions(dialogOptions | FOS_FORCEFILESYSTEM | FOS_STRICTFILETYPES | FOS_OVERWRITEPROMPT);
    if (SUCCEEDED(hr))
        hr = dialog->SetFileTypes(SAVETYPE_JPEG, filters);
    if (SUCCEEDED(hr))
        hr = dialog->SetFileTypeIndex(options.defaultSaveType);
    if (SUCCEEDED(hr))
        hr = dialog->SetDefaultExtension(defaultExtensions[options.defaultSaveType - 1]);

    HideAllCaptureBoxes(true);
    hForeWindow = hWnd;
    if (SUCCEEDED(hr))
        hr = dialog->Show(hWnd);
    hForeWindow = NULL;
    ShowAllCaptureBoxes();

    if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED))
    {
        dialog->Release();
        return;
    }

    if (FAILED(hr))
    {
        dialog->Release();
        MessageBox(hWnd, _T("Unable to display the Save As dialog."), _T("Save Error"), MB_OK | MB_ICONERROR);
        return;
    }

    IShellItem* result = NULL;
    hr = dialog->GetResult(&result);
    if (FAILED(hr))
    {
        dialog->Release();
        MessageBox(hWnd, _T("Unable to retrieve the selected save location."), _T("Save Error"), MB_OK | MB_ICONERROR);
        return;
    }

    PWSTR filename = NULL;
    hr = result->GetDisplayName(SIGDN_FILESYSPATH, &filename);
    result->Release();
    if (FAILED(hr))
    {
        dialog->Release();
        MessageBox(hWnd, _T("Unable to retrieve the selected save location."), _T("Save Error"), MB_OK | MB_ICONERROR);
        return;
    }

    UINT fileType = 0;
    hr = dialog->GetFileTypeIndex(&fileType);
    dialog->Release();
    if (FAILED(hr) || fileType < SAVETYPE_PNG || fileType > SAVETYPE_JPEG)
    {
        CoTaskMemFree(filename);
        MessageBox(hWnd, _T("Unable to determine the selected file type."), _T("Save Error"), MB_OK | MB_ICONERROR);
        return;
    }

    std::wstring savePath(filename);
    CoTaskMemFree(filename);

    SaveFile(hWnd, savePath.c_str(), fileType);
}

void GetFileName(LPTSTR szPath, int fileType)
{
    SYSTEMTIME time;
    GetSystemTime(&time);
    TCHAR szFile[50];
    _stprintf_s(szFile, 50, _T("\\SnapView_%04d%02d%02d_%02d%02d%02d"), time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
    _tcscat_s(szPath, MAX_PATH, szFile);

    switch (fileType)
    {
    case SAVETYPE_PNG:
        _tcscat_s(szPath, MAX_PATH, _T(".png"));
        break;
    case SAVETYPE_BMP:
        _tcscat_s(szPath, MAX_PATH, _T(".bmp"));
        break;
    case SAVETYPE_GIF:
        _tcscat_s(szPath, MAX_PATH, _T(".gif"));
        break;
    case SAVETYPE_JPEG:
        _tcscat_s(szPath, MAX_PATH, _T(".jpeg"));
        break;
    }
}

void QuickSaveCaptureBox(HWND hWnd)
{
    bool getDefault = true;
    TCHAR szPath[MAX_PATH];
    if (_tcslen(options.quickSavePath) > 0)
    {
        _tcscpy_s(szPath, MAX_PATH, options.quickSavePath);
        if (!_taccess_s(szPath, 0))
            getDefault = false;
    }
    if (getDefault)
        if (!SUCCEEDED(SHGetFolderPath(NULL, CSIDL_MYPICTURES, NULL, SHGFP_TYPE_CURRENT, szPath)))
            return;

    size_t i = _tcslen(szPath)-1;
    if (szPath[i] == _T('\\'))
        szPath[i] = _T('\0');

    GetFileName(szPath, options.defaultSaveType);

    SaveFile(hWnd, szPath, options.defaultSaveType);
}

bool CreateDragDropFile(HWND hWnd, std::wstring& path)
{
    TCHAR tempDirectory[MAX_PATH];
    DWORD length = GetTempPath(MAX_PATH, tempDirectory);
    if (!length || length >= MAX_PATH)
    {
        MessageBox(hWnd, _T("Unable to determine a temporary folder for the dragged image."),
            _T("Copy Error"), MB_OK | MB_ICONERROR);
        return false;
    }

    TCHAR temporaryPath[MAX_PATH];
    if (!GetTempFileName(tempDirectory, _T("SBX"), 0, temporaryPath))
    {
        MessageBox(hWnd, _T("Unable to create a temporary file for the dragged image."),
            _T("Copy Error"), MB_OK | MB_ICONERROR);
        return false;
    }

    if (!DeleteFile(temporaryPath))
    {
        MessageBox(hWnd, _T("Unable to prepare a temporary file for the dragged image."),
            _T("Copy Error"), MB_OK | MB_ICONERROR);
        return false;
    }

    std::wstring reservedPath(temporaryPath);
    const size_t nameStart = reservedPath.find_last_of(_T("\\/")) + 1;
    const size_t extensionStart = reservedPath.find_last_of(_T('.'));
    const std::wstring uniqueSuffix = reservedPath.substr(nameStart, extensionStart - nameStart);

    TCHAR defaultPath[MAX_PATH];
    _tcscpy_s(defaultPath, MAX_PATH, tempDirectory);
    size_t pathLength = _tcslen(defaultPath);
    if (pathLength && defaultPath[pathLength - 1] == _T('\\'))
        defaultPath[pathLength - 1] = _T('\0');
    GetFileName(defaultPath, options.defaultSaveType);

    std::wstring namedPath(defaultPath);
    namedPath.insert(namedPath.find_last_of(_T('.')), _T("_") + uniqueSuffix);
    if (!SaveFile(hWnd, namedPath.c_str(), options.defaultSaveType))
    {
        DeleteFile(namedPath.c_str());
        return false;
    }

    path = namedPath;
    return true;
}

void StartCopyDrag(HWND hWnd)
{
    std::wstring path;
    if (!CreateDragDropFile(hWnd, path))
        return;

    CaptureFileDataObject* dataObject = new CaptureFileDataObject(path);
    if (!dataObject->IsValid())
    {
        dataObject->Release();
        DeleteFile(path.c_str());
        MessageBox(hWnd, _T("Unable to prepare the dragged image."),
            _T("Copy Error"), MB_OK | MB_ICONERROR);
        return;
    }

    dragDropFiles.push_back(path);
    CaptureDropSource* dropSource = new CaptureDropSource();
    DWORD effect = DROPEFFECT_NONE;
    HRESULT result = DoDragDrop(dataObject, dropSource, DROPEFFECT_COPY, &effect);
    dropSource->Release();
    dataObject->Release();

    if (FAILED(result))
    {
        MessageBox(hWnd, _T("Unable to start the copy drag operation."),
            _T("Copy Error"), MB_OK | MB_ICONERROR);
    }
}

void ShareCaptureBox(HWND hWnd)
{
    TCHAR szPath[MAX_PATH];
    if (!GetTempPath(MAX_PATH, szPath))
        return;
    size_t i = _tcslen(szPath)-1;
    if (szPath[i] == _T('\\'))
        szPath[i] = _T('\0');

    GetFileName(szPath, options.defaultSaveType);
    SaveFile(hWnd, szPath, options.defaultSaveType);

    ShareFile(hWnd, szPath);
}

void AdjustRect(const PCAPTUREBOXINFO info, RECT &rect)
{
    if (info->moving && (info->moveType & MOVETYPE_RESIZE))
    {
        RECT newRect;
        memcpy(&newRect, &rect, sizeof(RECT));
        AdjustForSizeMarks(newRect, info->sizeMarks);

        rect.left += max(newRect.left-rect.left, (int)floorf(info->cropRect.X*info->scale));
        rect.top += max(newRect.top-rect.top, (int)floorf(info->cropRect.Y*info->scale));
        rect.right -= max(rect.right-newRect.right, (int)floorf((info->size.x - info->cropRect.GetRight())*info->scale));
        rect.bottom -= max(rect.bottom-newRect.bottom, (int)floorf((info->size.y - info->cropRect.GetBottom())*info->scale));
    }
    else
        AdjustForSizeMarks(rect, info->sizeMarks);
}

void AdjustPoint(const PCAPTUREBOXINFO info, POINT &p)
{
    if (info->moving && (info->moveType & MOVETYPE_RESIZE))
    {
        POINT newP;
        memcpy(&newP, &p, sizeof(POINT));
        AdjustPointForSizeMarks(newP, info->sizeMarks);

        p.x -= max(p.x-newP.x, (int)floorf(info->cropRect.X*info->scale));
        p.y -= max(p.y-newP.y, (int)floorf(info->cropRect.Y*info->scale));
    }
    else
        AdjustPointForSizeMarks(p, info->sizeMarks);
}

void DrawCaptureBox(HWND hWnd, PCAPTUREBOXINFO info, const RECT* rect)
{
    HDC hdcScreen = GetDC(NULL);

    bool drawShade = info->moving && (info->moveType & MOVETYPE_RESIZE);

    RECT frameRect, windowRect, shadeRect;
    ExpandForSizeMarks(rect, &frameRect, info->sizeMarks);
    memcpy(&shadeRect, rect, sizeof(RECT));

    if (drawShade)
    {
        shadeRect.left -= (int)floorf(info->cropRect.X*info->scale);
        shadeRect.top -= (int)floorf(info->cropRect.Y*info->scale);
        shadeRect.right += (int)floorf((info->size.x - info->cropRect.GetRight())*info->scale);
        shadeRect.bottom += (int)floorf((info->size.y - info->cropRect.GetBottom())*info->scale);

        windowRect.left = min(shadeRect.left, frameRect.left);
        windowRect.top = min(shadeRect.top, frameRect.top);
        windowRect.right = max(shadeRect.right, frameRect.right);
        windowRect.bottom = max(shadeRect.bottom, frameRect.bottom);
    }
    else
        memcpy(&windowRect, &frameRect, sizeof(RECT));

    int width = windowRect.right-windowRect.left,
        height = windowRect.bottom-windowRect.top;

    RectF clipRect;
    clipRect.X = (float)(rect->left - windowRect.left + 1);
    clipRect.Y = (float)(rect->top - windowRect.top + 1);
    clipRect.Width = (REAL)(rect->right-rect->left-2);
    clipRect.Height = (REAL)(rect->bottom-rect->top-2);

    HDC hdc = CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);
    HGDIOBJ hBitmapOld = SelectObject(hdc, hBitmap);
    Graphics* g = new Graphics(hdc);
    g->SetCompositingMode(CompositingModeSourceOver);
    g->SetSmoothingMode(SmoothingModeAntiAlias);

    g->FillRectangle(pTransparentBrush, 0, 0, width, height);

    if (drawShade)
        g->FillRectangle(pCropBrush, shadeRect.left-windowRect.left, shadeRect.top-windowRect.top, shadeRect.right-shadeRect.left, shadeRect.bottom-shadeRect.top);

    g->DrawImage(info->bitmap, clipRect, info->cropRect.X, info->cropRect.Y, info->cropRect.Width, info->cropRect.Height, UnitPixel);

    clipRect.X -= 1;
    clipRect.Y -= 1;
    clipRect.Width += 1;
    clipRect.Height += 1;
    g->DrawRectangle(pBorderPen, clipRect);

    GraphicsState gState = g->Save();
    g->TranslateTransform((float)(frameRect.left-windowRect.left), (float)(frameRect.top-windowRect.top));
    SIZEMARKOPTIONS smOptions;
    memset(&smOptions, 0, sizeof(smOptions));
    smOptions.lpRect = &frameRect;
    smOptions.lpCropRect = &info->cropRect;
    smOptions.lpCloseRect = &info->closeRect;
    smOptions.dwLocation = info->sizeMarks;
    smOptions.dwOptions = options.showHoverInfo ? SIZEMARKOPTION_SHOWCLOSE : 0;
    if (info->isInClose)
    {
        smOptions.dwOptions |= SIZEMARKOPTION_HOVERCLOSE;
        if (info->captured)
            smOptions.dwOptions |= SIZEMARKOPTION_CLOSEDOWN;
    }
    smOptions.fOpacity = info->frameOpacity;
    DrawSizeMarks(g, &smOptions);
    g->Restore(gState);

    BLENDFUNCTION blend;
    blend.BlendOp = AC_SRC_OVER;
    blend.AlphaFormat = AC_SRC_ALPHA;
    blend.SourceConstantAlpha = 255;
    blend.BlendFlags = 0;

    POINT ptPos = {windowRect.left, windowRect.top};
    SIZE sizeWnd = {width, height};
    POINT ptSrc = {0, 0};

    UpdateLayeredWindow(hWnd, hdcScreen, &ptPos, &sizeWnd, hdc, &ptSrc, 0, &blend, ULW_ALPHA);

    delete g;
    SelectObject(hdc, hBitmapOld);
    DeleteObject(hBitmap);
    DeleteDC(hdc);

    ReleaseDC(NULL, hdcScreen);
}

void CALLBACK AnimateTimer(HWND hWnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
    UNREFERENCED_PARAMETER(uMsg);
    UNREFERENCED_PARAMETER(idEvent);
    UNREFERENCED_PARAMETER(dwTime);

    PCAPTUREBOXINFO info = (PCAPTUREBOXINFO)GetWindowLongPtr(hWnd, GWLP_INFO);

    ULONG diff = (GetTickCount() - info->animateStart) / 30;

    RECT rect;
    GetWindowRect(hWnd, &rect);
    AdjustRect(info, rect);

    if (info->animateType & ANIMATETYPE_FRAME)
    {
        if (info->targetFrameOpacity > info->frameOpacity)
            info->frameOpacity = min(info->startFrameOpacity + 0.1f * static_cast<float>(diff), info->targetFrameOpacity);
        else
            info->frameOpacity = max(info->startFrameOpacity - 0.1f * static_cast<float>(diff), info->targetFrameOpacity);

        if (info->frameOpacity == info->targetFrameOpacity)
            info->animateType &= ~ANIMATETYPE_FRAME;
    }

    if (info->animateType & ANIMATETYPE_SCALE)
    {
        if (info->curScale < info->scale)
        {
            float temp = info->curScale * powf(1.05f, static_cast<float>(diff));
            info->curScale = min(temp, info->scale);
        }
        else
        {
            float temp = info->curScale * powf(0.95f, static_cast<float>(diff));
            info->curScale = max(temp, info->scale);
        }

        PointF center;
        if (info->centerMoved)
        {
            center.X = (float)(rect.right - rect.left - 1) / 2 + rect.left;
            center.Y = (float)(rect.bottom - rect.top - 1) / 2 + rect.top;
            info->center = center;
            info->centerMoved = false;
        }
        else
            center = info->center;

        int width = (int)floorf(info->cropRect.Width * info->curScale) + 2,
            height = (int)floorf(info->cropRect.Height * info->curScale) + 2;

        PointF newCenter;
        newCenter.X = ((float)width / 2) + rect.left;
        newCenter.Y = ((float)height / 2) + rect.top;

        rect.left -= (int)floorf(newCenter.X - center.X);
        rect.top -= (int)floorf(newCenter.Y - center.Y);
        rect.right = rect.left + width;
        rect.bottom = rect.top + height;
        DrawCaptureBox(hWnd, info, &rect);

        if (info->curScale == info->scale)
            info->animateType &= ~ANIMATETYPE_SCALE;
    }
    else
        DrawCaptureBox(hWnd, info, &rect);

    if (!info->animateType)
    {
        KillTimer(hWnd, info->animateTimer);
        info->animateTimer = (UINT_PTR)0;
    }
}

void AnimateZoom(HWND hWnd, int newScaleIndex, float newScale)
{
    PCAPTUREBOXINFO info = (PCAPTUREBOXINFO)GetWindowLongPtr(hWnd, GWLP_INFO);
    if (info->moving) return;

    info->scaleIndex = newScaleIndex;
    info->startScale = info->curScale;
    info->startFrameOpacity = info->frameOpacity;
    info->scale = newScale;
    info->animateStart = GetTickCount();
    info->animateType |= ANIMATETYPE_SCALE;

    if (!info->animateTimer)
        info->animateTimer = SetTimer(hWnd, 1, 30, (TIMERPROC)&AnimateTimer);
}

void AnimateFrameFade(HWND hWnd, PCAPTUREBOXINFO info, float targetOpacity)
{
    if (targetOpacity == info->frameOpacity) return;
    if ((info->animateType & ANIMATETYPE_FRAME) && targetOpacity == info->targetFrameOpacity) return;

    info->startScale = info->curScale;
    info->startFrameOpacity = info->frameOpacity;
    info->targetFrameOpacity = targetOpacity;
    info->animateStart = GetTickCount();
    info->animateType |= ANIMATETYPE_FRAME;

    if (!info->animateTimer)
        info->animateTimer = SetTimer(hWnd, 1, 30, (TIMERPROC)&AnimateTimer);
}

void SetTracking(HWND hWnd, PCAPTUREBOXINFO info)
{
    TRACKMOUSEEVENT tme;
    tme.cbSize = sizeof(tme);
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = hWnd;
    TrackMouseEvent(&tme);

    info->trackingMouse = true;
}

void CaptureMouseDown(HWND hWnd, LPARAM lParam)
{
    SetCapture(hWnd);

    PCAPTUREBOXINFO info = (PCAPTUREBOXINFO)GetWindowLongPtr(hWnd, GWLP_INFO);

    info->captured = true;
    if (!info->isInClose)
    {
        if (info->moveType == MOVETYPE_FRAME)
            info->moveType = MOVETYPE_MOVE;

        info->moved = false;
        info->movePoint.x = GET_X_LPARAM(lParam);
        info->movePoint.y = GET_Y_LPARAM(lParam);
        AdjustPoint(info, info->movePoint);
        info->screenMovePoint = info->movePoint;
        ClientToScreen(hWnd, &info->screenMovePoint);

        if (info->moveType & MOVETYPE_RESIZE)
            AnimateFrameFade(hWnd, info, 1.0);

        RECT rect;
        GetWindowRect(hWnd, &rect);
        AdjustRect(info, rect);

        if (info->moveType & MOVETYPE_LEFT)
            info->sizeMarks = SIZEMARKLOCATION_RIGHT;
        else
            info->sizeMarks = SIZEMARKLOCATION_LEFT;
        if (info->moveType & MOVETYPE_TOP)
            info->sizeMarks |= SIZEMARKLOCATION_BOTTOM;
        else
            info->sizeMarks |= SIZEMARKLOCATION_TOP;
        info->moving = true;

        DrawCaptureBox(hWnd, info, &rect);
    }
    else
    {
        RECT rect;
        GetWindowRect(hWnd, &rect);
        AdjustRect(info, rect);
        DrawCaptureBox(hWnd, info, &rect);
    }
}

void DoMoveWindow(HWND hWnd, PCAPTUREBOXINFO info, RECT &rect, int x, int y, int width, int height)
{
    rect.left += x;
    rect.top += y;
    rect.right += width + x;
    rect.bottom  += height + y;

    DrawCaptureBox(hWnd, info, &rect);
    info->centerMoved = true;
}

void CaptureMouseMove(HWND hWnd, LPARAM lParam)
{
    PCAPTUREBOXINFO info = (PCAPTUREBOXINFO)GetWindowLongPtr(hWnd, GWLP_INFO);
    RECT rect;
    POINT p;
    bool alreadySized = false;

    p.x = GET_X_LPARAM(lParam);
    p.y = GET_Y_LPARAM(lParam);
    GetWindowRect(hWnd, &rect);

    if (!info->moving)
    {
        bool newInClose = !!PtInRect(&info->closeRect, p);
        if (newInClose != info->isInClose)
        {
            info->isInClose = newInClose;
            AdjustRect(info, rect);
            DrawCaptureBox(hWnd, info, &rect);
            alreadySized = true;
        }
    }
    else
        info->isInClose = false;

    if (!alreadySized)
        AdjustRect(info, rect);
    AdjustPoint(info, p);

    if (info->moving)
    {
        if ((info->movePoint.x != p.x) || (info->movePoint.y != p.y))
        {
            if (!info->moved)
            {
                POINT pScreen = p;
                ClientToScreen(hWnd, &pScreen);

                if ((abs(pScreen.x-info->screenMovePoint.x) >= dragMin.x) || (abs(pScreen.y-info->screenMovePoint.y) >= dragMin.y))
                    info->moved = true;
            }

            int offsetX = p.x - info->movePoint.x,
                offsetY = p.y - info->movePoint.y;
            RectF cropRect = info->cropRect;
            float scale = info->scale;

            if (info->moveType & MOVETYPE_LEFT)
            {
                int temp = (int)ceilf(cropRect.X*scale),
                    width = rect.right-rect.left;
                if (offsetX + temp < 0)
                    offsetX = -temp;
                else if (width - offsetX < MIN_CAPTURE_SIZE)
                    offsetX = width - MIN_CAPTURE_SIZE;
            }
            if (info->moveType & MOVETYPE_RIGHT)
            {
                int temp = (int)ceilf((info->size.x - cropRect.X)*scale)+2,
                    width = rect.right-rect.left;
                if (width + offsetX > temp)
                    offsetX = temp - width;
                else if (width + offsetX < MIN_CAPTURE_SIZE)
                    offsetX = MIN_CAPTURE_SIZE - width;
            }
            if (info->moveType & MOVETYPE_TOP)
            {
                int temp = (int)ceilf(cropRect.Y*scale),
                    height = rect.bottom-rect.top;
                if (offsetY + temp < 0)
                    offsetY = -temp;
                else if (height - offsetY < MIN_CAPTURE_SIZE)
                    offsetY = height - MIN_CAPTURE_SIZE;
            }
            if (info->moveType & MOVETYPE_BOTTOM)
            {
                int temp = (int)ceilf((info->size.y - cropRect.Y)*scale)+2,
                    height = rect.bottom-rect.top;
                if (height + offsetY > temp)
                    offsetY = temp - height;
                else if (height + offsetY < MIN_CAPTURE_SIZE)
                    offsetY = MIN_CAPTURE_SIZE - height;
            }

            switch (info->moveType)
            {
            case MOVETYPE_MOVE:
                DoMoveWindow(hWnd, info, rect, offsetX, offsetY, 0, 0);
                break;
            case MOVETYPE_LEFT:
                info->cropRect.X += offsetX / info->scale;
                info->cropRect.Width -= offsetX / info->scale;
                DoMoveWindow(hWnd, info, rect, offsetX, 0, -offsetX, 0);
                break;
            case MOVETYPE_RIGHT:
                info->cropRect.Width += offsetX / info->scale;
                info->movePoint.x += offsetX;
                DoMoveWindow(hWnd, info, rect, 0, 0, offsetX, 0);
                break;
            case MOVETYPE_TOP:
                info->cropRect.Y += offsetY / info->scale;
                info->cropRect.Height -= offsetY / info->scale;
                DoMoveWindow(hWnd, info, rect, 0, offsetY, 0, -offsetY);
                break;
            case MOVETYPE_BOTTOM:
                info->cropRect.Height += offsetY / info->scale;
                info->movePoint.y += offsetY;
                DoMoveWindow(hWnd, info, rect, 0, 0, 0, offsetY);
                break;
            case MOVETYPE_TOPLEFT:
                info->cropRect.X += offsetX / info->scale;
                info->cropRect.Width -= offsetX / info->scale;
                info->cropRect.Y += offsetY / info->scale;
                info->cropRect.Height -= offsetY / info->scale;
                DoMoveWindow(hWnd, info, rect, offsetX, offsetY, -offsetX, -offsetY);
                break;
            case MOVETYPE_TOPRIGHT:
                info->cropRect.Width += offsetX / info->scale;
                info->movePoint.x += offsetX;
                info->cropRect.Y += offsetY / info->scale;
                info->cropRect.Height -= offsetY / info->scale;
                DoMoveWindow(hWnd, info, rect, 0, offsetY, offsetX, -offsetY);
                break;
            case MOVETYPE_BOTTOMLEFT:
                info->cropRect.X += offsetX / info->scale;
                info->cropRect.Width -= offsetX / info->scale;
                info->cropRect.Height += offsetY / info->scale;
                info->movePoint.y += offsetY;
                DoMoveWindow(hWnd, info, rect, offsetX, 0, -offsetX, offsetY);
                break;
            case MOVETYPE_BOTTOMRIGHT:
                info->cropRect.Width += offsetX / info->scale;
                info->movePoint.x += offsetX;
                info->cropRect.Height += offsetY / info->scale;
                info->movePoint.y += offsetY;
                DoMoveWindow(hWnd, info, rect, 0, 0, offsetX, offsetY);
                break;
            }
        }
    } else {
        int moveType;
        int width = rect.right-rect.left,
            height = rect.bottom-rect.top;

        if (p.x < 0 || p.x > width || p.y < 0 || p.y > height)
            moveType = MOVETYPE_FRAME;
        else
        {
            moveType = MOVETYPE_NONE;
            if (info->size.x >= MIN_CAPTURE_SIZE)
            {
                if (p.x < CROP_SIZE)
                    moveType |= MOVETYPE_LEFT;
                else if (p.x > width-CROP_SIZE)
                    moveType |= MOVETYPE_RIGHT;
            }
            if (info->size.y >= MIN_CAPTURE_SIZE)
            {
                if (p.y < CROP_SIZE)
                    moveType |= MOVETYPE_TOP;
                else if (p.y > height-CROP_SIZE)
                    moveType |= MOVETYPE_BOTTOM;
            }
            if (moveType == MOVETYPE_NONE)
                moveType = MOVETYPE_MOVE;
        }

        if (!info->captured) {
            switch (moveType)
            {
            case MOVETYPE_FRAME:
                SetCursor(hCursorArrow);
                break;
            case MOVETYPE_MOVE:
                SetCursor(hCursorMove);
                break;
            case MOVETYPE_LEFT:
            case MOVETYPE_RIGHT:
                SetCursor(hCursorEW);
                break;
            case MOVETYPE_TOP:
            case MOVETYPE_BOTTOM:
                SetCursor(hCursorNS);
                break;
            case MOVETYPE_TOPLEFT:
            case MOVETYPE_BOTTOMRIGHT:
                SetCursor(hCursorNWSE);
                break;
            case MOVETYPE_TOPRIGHT:
            case MOVETYPE_BOTTOMLEFT:
                SetCursor(hCursorNESW);
                break;
            }
        }

        info->moveType = moveType;

        if (!info->captured && options.showHoverInfo)
        {
            if (moveType == MOVETYPE_FRAME)
                AnimateFrameFade(hWnd, info, 1.0);
            else
                AnimateFrameFade(hWnd, info, FRAME_OPACITY);
        }
    }

    if (!info->trackingMouse)
        SetTracking(hWnd, info);
}

void CaptureMouseUp(HWND hWnd)
{
    PCAPTUREBOXINFO info = (PCAPTUREBOXINFO)GetWindowLongPtr(hWnd, GWLP_INFO);
    if (info->captured)
    {
        ReleaseCapture();

        if (info->moving)
        {
            if (info->moveType == MOVETYPE_MOVE && !info->moved)
            {
                info->captured = false;
                info->moving = false;
                DestroyWindow(hWnd);
                return;
            }
        }
        else
        {
            if ((info->moveType == MOVETYPE_FRAME) && info->isInClose)
            {
                info->captured = false;
                info->moving = false;
                DestroyWindow(hWnd);
                return;
            }
        }

        if (info->moveType & MOVETYPE_RESIZE)
            AnimateFrameFade(hWnd, info, options.showHoverInfo ? FRAME_OPACITY : (float)0.0);

        RECT rect;
        GetWindowRect(hWnd, &rect);
        AdjustRect(info, rect);
        info->sizeMarks = SIZEMARKLOCATION_TOPLEFT;
        info->captured = false;
        info->moving = false;
        DrawCaptureBox(hWnd, info, &rect);
    }
}

void CaptureMouseLeave(HWND hWnd)
{
    PCAPTUREBOXINFO info = (PCAPTUREBOXINFO)GetWindowLongPtr(hWnd, GWLP_INFO);
    if (info->moveType == MOVETYPE_FRAME)
        info->moveType = MOVETYPE_NONE;
    AnimateFrameFade(hWnd, info, 0.0);

    info->trackingMouse = false;
}

void CaptureMouseWheel(HWND hWnd, short wheelDelta)
{
    PCAPTUREBOXINFO info = (PCAPTUREBOXINFO)GetWindowLongPtr(hWnd, GWLP_INFO);
    if (info->moving) return;

    int scaleIndex = info->scaleIndex + (wheelDelta/120);
    float scale = 1.0f;
    if (scaleIndex > 0)
        scale = powf(1.05f, static_cast<float>(scaleIndex));
    else if (scaleIndex < 0)
        scale = powf(0.95f, static_cast<float>(-scaleIndex));

    int width = (int)(floorf(info->cropRect.Width * scale) + 2),
        height = (int)(floorf(info->cropRect.Height * scale) + 2);

    int screenHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN),
        screenWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);

    if ((height < MIN_CAPTURE_SIZE || width < MIN_CAPTURE_SIZE) ||
        (height > screenHeight || width > screenWidth))
    {
        if (wheelDelta > 0)
            wheelDelta = ((wheelDelta / 120) - 1) * 120;
        else
            wheelDelta = ((wheelDelta / 120) + 1) * 120;

        if (wheelDelta)
            CaptureMouseWheel(hWnd, wheelDelta);
        return;
    }

    AnimateZoom(hWnd, scaleIndex, scale);
}

bool ProcessMouseWheel(POINT pt, short wheelDelta)
{
    HWND hWnd = WindowFromPoint(pt);

    if (hWnd)
    {
        PCAPTUREBOXWINDOW w = openCaptureBoxes;
        while (w)
        {
            if (hWnd == w->hWnd)
            {
                CaptureMouseWheel(hWnd, wheelDelta);
                return true;
            }

            w = w->next;
        }
    }

    return false;
}

LRESULT CALLBACK CaptureBoxWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int wmId, wmEvent;
    RECT rect;
    RectF clipRect, srcRect;
    PCAPTUREBOXINFO info;
    POINT p;

    switch (message)
    {
    case WM_COMMAND:
        wmId    = LOWORD(wParam);
        wmEvent = HIWORD(wParam);
        // Parse the menu selections:
        switch (wmId)
        {
        case IDM_COPY:
            CopyToClipboard(hWnd);
            break;
        case IDM_SAVE:
            SaveCaptureBox(hWnd);
            break;
        case IDM_QUICKSAVE:
            QuickSaveCaptureBox(hWnd);
            break;
        case IDM_SHARE:
            ShareCaptureBox(hWnd);
            break;
        case IDM_CLOSE:
            DestroyWindow(hWnd);
            break;
        case IDM_CLOSEALL:
            CloseAllCaptureBoxes();
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;

    case WM_RBUTTONUP:
        HMENU hPopup;
        hPopup = GetSubMenu(hCaptureMenu, 0);

        p.x = GET_X_LPARAM(lParam);
        p.y = GET_Y_LPARAM(lParam);
        ClientToScreen(hWnd, &p);
        TrackPopupMenu(hPopup, TPM_RIGHTBUTTON, p.x, p.y, 0, hWnd, NULL);
        break;
    case WM_LBUTTONDOWN:
        if (GetKeyState(VK_CONTROL) & 0x8000)
            StartCopyDrag(hWnd);
        else
            CaptureMouseDown(hWnd, lParam);
        break;
    case WM_MOUSEMOVE:
        CaptureMouseMove(hWnd, lParam);
        break;
    case WM_MOUSELEAVE:
        CaptureMouseLeave(hWnd);
        break;
    case WM_LBUTTONUP:
        CaptureMouseUp(hWnd);
        break;
    case WM_MBUTTONUP:
        AnimateZoom(hWnd, 0, 1.0);
        break;
    case WM_SHOWWINDOW:
        if (wParam)
        {
            info = (PCAPTUREBOXINFO)GetWindowLongPtr(hWnd, GWLP_INFO);
            GetWindowRect(hWnd, &rect);
            AdjustRect(info, rect);
            info->sizeMarks = SIZEMARKLOCATION_TOPLEFT;
            DrawCaptureBox(hWnd, info, &rect);
        }
        break;
    case WM_DPICHANGED:
        {
            RECT* suggestedRect = (RECT*)lParam;
            SetWindowPos(hWnd, NULL, suggestedRect->left, suggestedRect->top,
                suggestedRect->right - suggestedRect->left,
                suggestedRect->bottom - suggestedRect->top,
                SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);

            info = (PCAPTUREBOXINFO)GetWindowLongPtr(hWnd, GWLP_INFO);
            GetWindowRect(hWnd, &rect);
            AdjustRect(info, rect);
            DrawCaptureBox(hWnd, info, &rect);
        }
        return 0;
    case WM_DESTROY:
        {
            PCAPTUREBOXCLOSEINFO closeInfo = new CAPTUREBOXCLOSEINFO();
            closeInfo->info = (PCAPTUREBOXINFO)GetWindowLongPtr(hWnd, GWLP_INFO);
            GetWindowRect(hWnd, &closeInfo->rLocation);
            SaveCaptureBox(closeInfo);

            PCAPTUREBOXWINDOW w = openCaptureBoxes;
            PCAPTUREBOXWINDOW *captureBoxLink = &openCaptureBoxes;
            while (w)
            {
                if (w->hWnd == hWnd)
                {
                    *captureBoxLink = w->next;
                    delete w;
                    break;
                }

                captureBoxLink = &w->next;
                w = w->next;
            }
        }
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
