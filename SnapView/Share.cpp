#include "stdafx.h"
#include "Share.h"

using namespace winrt::Windows::ApplicationModel::DataTransfer;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Streams;

namespace
{
    struct ShareSession
    {
        DataTransferManager dataTransferManager{ nullptr };
        winrt::event_token dataRequestedToken{};
        std::wstring filename;
        bool hasDataRequestedHandler = false;

        ~ShareSession()
        {
            if (hasDataRequestedHandler)
                dataTransferManager.DataRequested(dataRequestedToken);

            DeleteFile(filename.c_str());
        }
    };

    std::unique_ptr<ShareSession> activeShare;

    void DisplayShareError(HWND hWnd, const wchar_t* message)
    {
        MessageBox(hWnd, message, _T("Share Error"), MB_ICONERROR | MB_OK);
    }
}

namespace
{
    winrt::fire_and_forget ShowShareSheetAsync(HWND hWnd, std::wstring filename)
    {
        try
        {
            StorageFile file = co_await StorageFile::GetFileFromPathAsync(filename);
            RandomAccessStreamReference image = RandomAccessStreamReference::CreateFromFile(file);

            winrt::com_ptr<IDataTransferManagerInterop> interop =
                winrt::get_activation_factory<DataTransferManager, IDataTransferManagerInterop>();
            constexpr winrt::guid dataTransferManagerIid{
                0xa5caee9b, 0x8708, 0x49d1, { 0x8d, 0x36, 0x67, 0xd2, 0x5a, 0x8d, 0xa0, 0x0c } };

            auto share = std::make_unique<ShareSession>();
            share->filename = filename;
            winrt::check_hresult(interop->GetForWindow(
                hWnd, dataTransferManagerIid, winrt::put_abi(share->dataTransferManager)));

            share->dataRequestedToken = share->dataTransferManager.DataRequested(
                [file, image](const DataTransferManager&, const DataRequestedEventArgs& args)
                {
                    DataPackage data = args.Request().Data();
                    data.Properties().Title(L"SnapView screenshot");
                    data.Properties().Description(L"Screenshot captured with SnapView");
                    data.Properties().Thumbnail(image);
                    data.SetBitmap(image);
                    data.RequestedOperation(DataPackageOperation::Copy);

                    auto items = winrt::single_threaded_vector<IStorageItem>();
                    items.Append(file);
                    data.SetStorageItems(items);
                });
            share->hasDataRequestedHandler = true;

            HRESULT result = interop->ShowShareUIForWindow(hWnd);
            winrt::check_hresult(result);

            activeShare = std::move(share);
        }
        catch (const winrt::hresult_error&)
        {
            DisplayShareError(hWnd, _T("Unable to open the Windows Share Sheet."));
        }
    }
}

void ShareFile(HWND hWnd, LPCTSTR szFilename)
{
    activeShare.reset();
    ShowShareSheetAsync(hWnd, szFilename);
}

void CleanupShare()
{
    activeShare.reset();
}
