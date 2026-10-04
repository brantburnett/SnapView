#pragma once

// Settings shared by SnapView.exe and SnapViewOptions.exe. Both processes run
// with the MSIX package identity and read and write the same
// ApplicationData.LocalSettings store.

#include <windows.h>

#include <cstdint>
#include <string>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.h>

namespace SnapViewSettings
{
    inline constexpr wchar_t MaxHistoryKey[] = L"MaxHistory";
    inline constexpr wchar_t QuickSavePathKey[] = L"QuickSavePath";
    inline constexpr wchar_t DefaultSaveTypeKey[] = L"DefaultSaveType";
    inline constexpr wchar_t HideOnNewSnapKey[] = L"HideOnNewSnap";
    inline constexpr wchar_t ShowHoverInfoKey[] = L"ShowHoverInfo";

    inline constexpr wchar_t StartupTaskId[] = L"SnapViewStartupTask";

    inline constexpr int32_t MaxCaptureHistory = 20;
    inline constexpr int32_t DefaultMaxHistory = 5;

    // Quick save appends "\SnapView_YYYYMMDD_HHMMSS.jpeg" to the folder inside
    // a MAX_PATH buffer, so the folder must leave room for the file name.
    inline constexpr size_t MaxQuickSavePathLength = MAX_PATH - 32;

    inline constexpr int32_t SaveTypePng = 1;
    inline constexpr int32_t SaveTypeBmp = 2;
    inline constexpr int32_t SaveTypeGif = 3;
    inline constexpr int32_t SaveTypeJpeg = 4;

    struct Settings
    {
        int32_t maxHistory = DefaultMaxHistory;
        std::wstring quickSavePath;
        int32_t defaultSaveType = SaveTypePng;
        bool hideOnNewSnap = true;
        bool showHoverInfo = true;
    };

    namespace details
    {
        inline winrt::Windows::Foundation::Collections::IPropertySet Values()
        {
            return winrt::Windows::Storage::ApplicationData::Current().LocalSettings().Values();
        }

        inline winrt::Windows::Foundation::IPropertyValue TryGetValue(
            const winrt::Windows::Foundation::Collections::IPropertySet& values,
            const wchar_t* key,
            winrt::Windows::Foundation::PropertyType type)
        {
            const auto value = values.TryLookup(key).try_as<winrt::Windows::Foundation::IPropertyValue>();
            if (!value || value.Type() != type)
                return nullptr;

            return value;
        }

        template <typename T>
        bool Write(const wchar_t* key, const T& value) noexcept
        {
            try
            {
                Values().Insert(key, winrt::box_value(value));
                return true;
            }
            catch (const winrt::hresult_error&)
            {
                return false;
            }
        }
    }

    inline bool IsValidMaxHistory(int32_t value) noexcept
    {
        return value >= 0 && value <= MaxCaptureHistory;
    }

    inline bool IsValidSaveType(int32_t value) noexcept
    {
        return value >= SaveTypePng && value <= SaveTypeJpeg;
    }

    // An empty path means the user's Pictures folder.
    inline bool IsValidQuickSavePath(const std::wstring& path) noexcept
    {
        if (path.empty())
            return true;

        if (path.size() > MaxQuickSavePathLength)
            return false;

        const DWORD attributes = GetFileAttributesW(path.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES &&
            (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }

    // Throws winrt::hresult_error when the settings store is unavailable.
    inline Settings Load()
    {
        using winrt::Windows::Foundation::PropertyType;

        Settings settings;
        const auto values = details::Values();

        if (const auto value = details::TryGetValue(values, MaxHistoryKey, PropertyType::Int32))
        {
            const int32_t maxHistory = value.GetInt32();
            if (maxHistory > MaxCaptureHistory)
                settings.maxHistory = MaxCaptureHistory;
            else if (maxHistory >= 0)
                settings.maxHistory = maxHistory;
        }

        if (const auto value = details::TryGetValue(values, QuickSavePathKey, PropertyType::String))
        {
            const winrt::hstring path = value.GetString();
            if (path.size() <= MaxQuickSavePathLength)
                settings.quickSavePath = path;
        }

        if (const auto value = details::TryGetValue(values, DefaultSaveTypeKey, PropertyType::Int32))
        {
            const int32_t saveType = value.GetInt32();
            settings.defaultSaveType = IsValidSaveType(saveType) ? saveType : SaveTypePng;
        }

        if (const auto value = details::TryGetValue(values, HideOnNewSnapKey, PropertyType::Boolean))
            settings.hideOnNewSnap = value.GetBoolean();

        if (const auto value = details::TryGetValue(values, ShowHoverInfoKey, PropertyType::Boolean))
            settings.showHoverInfo = value.GetBoolean();

        return settings;
    }

    inline bool WriteMaxHistory(int32_t value) noexcept
    {
        return IsValidMaxHistory(value) && details::Write(MaxHistoryKey, value);
    }

    inline bool WriteQuickSavePath(const std::wstring& value) noexcept
    {
        return IsValidQuickSavePath(value) &&
            details::Write(QuickSavePathKey, winrt::hstring(value));
    }

    inline bool WriteDefaultSaveType(int32_t value) noexcept
    {
        return IsValidSaveType(value) && details::Write(DefaultSaveTypeKey, value);
    }

    inline bool WriteHideOnNewSnap(bool value) noexcept
    {
        return details::Write(HideOnNewSnapKey, value);
    }

    inline bool WriteShowHoverInfo(bool value) noexcept
    {
        return details::Write(ShowHoverInfoKey, value);
    }
}
