#include "stdafx.h"
#include "MenuIcons.h"
#include "resource.h"

using namespace Gdiplus;

namespace
{
    constexpr wchar_t FluentIconsFont[] = L"Segoe Fluent Icons";
    constexpr wchar_t Mdl2AssetsFont[] = L"Segoe MDL2 Assets";

    // The dark end of the SnapView blue gradient (size marks and app icon).
    // The light end, #35C2F1, is too faint against the menu background.
    const Color MenuIconColor(0x14, 0x7d, 0xe2);

    struct MenuIcon
    {
        UINT commandId;
        wchar_t fluentGlyph;
        wchar_t mdl2Glyph;
        HBITMAP hBitmap;
    };

    // Icons by command, shared by the tray and capture menus. All glyphs
    // except ClippingTool share codepoints in both fonts; ClippingTool is
    // Fluent only, so Snap falls back to Camera. Close and Close All have
    // no icon.
    MenuIcon menuIcons[] =
    {
        { IDM_SNAP, 0xF406, 0xE722, NULL },       // ClippingTool, Camera
        { IDM_REOPEN, 0xE81C, 0xE81C, NULL },     // History
        { IDM_OPTIONS, 0xE713, 0xE713, NULL },    // Settings
        { IDM_ABOUT, 0xE946, 0xE946, NULL },      // Info
        { IDM_COPY, 0xE8C8, 0xE8C8, NULL },       // Copy
        { IDM_SAVE, 0xE792, 0xE792, NULL },       // SaveAs
        { IDM_QUICKSAVE, 0xE74E, 0xE74E, NULL },  // Save
        { IDM_SHARE, 0xE72D, 0xE72D, NULL },      // Share
    };

    int CALLBACK FontExistsProc(const LOGFONT*, const TEXTMETRIC*, DWORD, LPARAM lParam)
    {
        *reinterpret_cast<bool*>(lParam) = true;
        return 0;
    }

    bool FontExists(const wchar_t* faceName)
    {
        LOGFONT lf = {};
        lf.lfCharSet = DEFAULT_CHARSET;
        wcscpy_s(lf.lfFaceName, faceName);

        bool exists = false;
        HDC hdc = GetDC(NULL);
        EnumFontFamiliesEx(hdc, &lf, FontExistsProc, reinterpret_cast<LPARAM>(&exists), 0);
        ReleaseDC(NULL, hdc);

        return exists;
    }

    // Themed menus draw hbmpItem with per-pixel alpha only from a 32bpp
    // premultiplied DIB. Owner-drawn HBMMENU_CALLBACK items make Windows
    // fall back to the classic menu style.
    HBITMAP CreateGlyphBitmap(const FontFamily& fontFamily, wchar_t glyph, int size)
    {
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
        bmi.bmiHeader.biWidth = size;
        bmi.bmiHeader.biHeight = -size;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* bits = nullptr;
        HBITMAP hBitmap = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
        if (!hBitmap)
            return NULL;

        Bitmap bitmap(size, size, size * 4, PixelFormat32bppPARGB, static_cast<BYTE*>(bits));
        Graphics graphics(&bitmap);
        graphics.Clear(Color(0, 0, 0, 0));
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        graphics.SetPixelOffsetMode(PixelOffsetModeHalf);

        // Icon font glyphs fill the em square (ascent equals em, no descent),
        // so a typographic path at the origin fills the bitmap exactly.
        // DrawString pads and recenters the line box, which clips the tops.
        wchar_t text[] = { glyph, L'\0' };
        GraphicsPath path;
        path.AddString(text, 1, &fontFamily, FontStyleRegular, static_cast<REAL>(size),
            PointF(0, 0), StringFormat::GenericTypographic());

        SolidBrush brush(MenuIconColor);
        graphics.FillPath(&brush, &path);

        return hBitmap;
    }
}

void InitMenuIcons()
{
    bool useFluentIcons = FontExists(FluentIconsFont);
    FontFamily fontFamily(useFluentIcons ? FluentIconsFont : Mdl2AssetsFont);

    // SnapView is system DPI aware, so menus are laid out at the system DPI.
    int size = GetSystemMetricsForDpi(SM_CXSMICON, GetDpiForSystem());

    for (auto& icon : menuIcons)
        icon.hBitmap = CreateGlyphBitmap(fontFamily, useFluentIcons ? icon.fluentGlyph : icon.mdl2Glyph, size);
}

void ApplyMenuIcons(HMENU hMenu)
{
    HMENU hPopup = GetSubMenu(hMenu, 0);

    MENUITEMINFO mii = {};
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_BITMAP;

    // Commands that are not in this menu are skipped.
    for (const auto& icon : menuIcons)
    {
        mii.hbmpItem = icon.hBitmap;
        SetMenuItemInfo(hPopup, icon.commandId, FALSE, &mii);
    }
}

void CleanupMenuIcons()
{
    for (auto& icon : menuIcons)
    {
        if (icon.hBitmap)
        {
            DeleteObject(icon.hBitmap);
            icon.hBitmap = NULL;
        }
    }
}
