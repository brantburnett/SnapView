#include "stdafx.h"
#include "SizeMarks.h"

using namespace Gdiplus;

#define SIZEMARK_PADDING		2
#define SIZEMARK_HORIZONTALPADDING	3
#define SIZEMARK_ROUNDING		4
#define SIZEMARK_TRAPEZOID		3
#define SIZEMARK_FONTSIZE		8
#define SIZEMARK_CLOSEFONTSIZE	8
#define SIZEMARK_CLOSEPADDING	3
#define SIZEMARK_CLOSESPACING	2
#define SIZEMARK_CLOSERIGHTPADDING	3
#define SIZEMARK_CLOSECORNER	3

Font* sizeFont = NULL;
Font* closeFont = NULL;
StringFormat* sizeStringFormat = NULL;

UINT sizeHeight = 0, closeGlyphHeight = 0, closeGlyphWidth = 0;
RectF closeGlyphBounds;
float fSizeHeight = 0;

const Color SizeMarkColor1(0x14, 0x7d, 0xe2);
const Color SizeMarkColor2(0x35, 0xc2, 0xf1);
const Color SizeMarkBorderColor(0xdd, 0xf5, 0xff);
const Color SizeMarkTextColor(0, 0, 0);
const Color CloseButtonHoverColor(0x35, 0xc2, 0xf1);
const Color CloseButtonPressedColor(0x14, 0x7d, 0xe2);
const WCHAR CloseGlyph[] = L"\xE8BB";

void AddRoundedRectangle(GraphicsPath* path, const RectF& rect, REAL radius)
{
    path->AddArc(rect.X, rect.Y, radius * 2, radius * 2, 180, 90);
    path->AddArc(rect.GetRight() - radius * 2, rect.Y, radius * 2, radius * 2, 270, 90);
    path->AddArc(rect.GetRight() - radius * 2, rect.GetBottom() - radius * 2, radius * 2, radius * 2, 0, 90);
    path->AddArc(rect.X, rect.GetBottom() - radius * 2, radius * 2, radius * 2, 90, 90);
    path->CloseFigure();
}

void ShutdownSizeMarks()
{
    delete sizeFont;
    sizeFont = NULL;
    delete closeFont;
    closeFont = NULL;
    delete sizeStringFormat;
    sizeStringFormat = NULL;

    sizeHeight = 0;
    closeGlyphHeight = 0;
    closeGlyphWidth = 0;
    memset(&closeGlyphBounds, 0, sizeof(closeGlyphBounds));
    fSizeHeight = 0;
}

void InitSizeMarks(HINSTANCE)
{
    ShutdownSizeMarks();

    Graphics* g = new Graphics(GetDesktopWindow());

    sizeFont = new Font(_T("Segoe UI"), SIZEMARK_FONTSIZE);
    closeFont = new Font(_T("Segoe MDL2 Assets"), SIZEMARK_CLOSEFONTSIZE);

    sizeStringFormat = new StringFormat();
    if (!g || g->GetLastStatus() != Ok || !sizeFont || sizeFont->GetLastStatus() != Ok || !closeFont || closeFont->GetLastStatus() != Ok || !sizeStringFormat || sizeStringFormat->GetLastStatus() != Ok)
    {
        delete g;
        ShutdownSizeMarks();
        return;
    }

    sizeStringFormat->SetTrimming(StringTrimmingNone);
    sizeStringFormat->SetAlignment(StringAlignmentCenter);
    sizeStringFormat->SetLineAlignment(StringAlignmentCenter);

    RectF boundingBox;
    g->MeasureString(_T("Mg"), -1, sizeFont, PointF(0, 0), &boundingBox);
    sizeHeight = (int)ceilf(boundingBox.Height) + SIZEMARK_PADDING*2;
    g->MeasureString(CloseGlyph, -1, closeFont, PointF(0, 0), StringFormat::GenericTypographic(), &closeGlyphBounds);
    closeGlyphHeight = (UINT)ceilf(closeGlyphBounds.Height);
    closeGlyphWidth = (UINT)ceilf(closeGlyphBounds.Width);
    sizeHeight = max(sizeHeight, closeGlyphHeight + SIZEMARK_CLOSEPADDING*2 + SIZEMARK_PADDING*2);
    fSizeHeight = (float)sizeHeight;

    delete g;
}

void DrawSizeMarks(Graphics* g, PCSIZEMARKOPTIONS options)
{
    if (!g || !options || !options->lpRect || !options->lpCropRect || !sizeFont || !closeFont || !sizeStringFormat)
        return;

    g->SetTextRenderingHint(TextRenderingHintAntiAlias);
    if (options->lpCloseRect)
        memset(options->lpCloseRect, 0, sizeof(RECT));

    BYTE opacityByte = (DWORD)ceilf(options->fOpacity * 255) & 0xff;
    ARGB opacityArgb = (DWORD)opacityByte << 24;

    TCHAR sizeStr[20];
    RectF boundingBox;
    GraphicsPath* path;
    float width = (float)(options->lpRect->right - options->lpRect->left),
        height = (float)(options->lpRect->bottom - options->lpRect->top);

    if (options->dwLocation & (SIZEMARKLOCATION_TOP | SIZEMARKLOCATION_BOTTOM))
        height -= fSizeHeight;
    if (options->dwLocation & (SIZEMARKLOCATION_LEFT | SIZEMARKLOCATION_RIGHT))
        width -= fSizeHeight;

    if (options->dwLocation & (SIZEMARKLOCATION_TOP | SIZEMARKLOCATION_BOTTOM))
    {
        bool drawClose = !!(options->dwOptions & SIZEMARKOPTION_SHOWCLOSE);
        float closeButtonWidth = (float)(closeGlyphWidth + SIZEMARK_CLOSEPADDING*2);
        float closeButtonHeight = (float)(closeGlyphHeight + SIZEMARK_CLOSEPADDING*2);

        _itot_s((int)ceilf(options->lpCropRect->Width), sizeStr, 20, 10);
        _tcscat_s(sizeStr, 20, _T("px"));

        g->MeasureString(sizeStr, -1, sizeFont, PointF(0, 0), &boundingBox);
        boundingBox.Width += SIZEMARK_HORIZONTALPADDING*2;
        if (drawClose)
            boundingBox.Width += closeButtonWidth + SIZEMARK_CLOSESPACING + SIZEMARK_CLOSERIGHTPADDING;

        if (boundingBox.Width > width)
        {
            sizeStr[_tcslen(sizeStr)-2] = _T('\0');
            g->MeasureString(sizeStr, -1, sizeFont, PointF(0, 0), &boundingBox);
            boundingBox.Width += SIZEMARK_HORIZONTALPADDING*2 + SIZEMARK_CLOSESPACING;
            if (drawClose)
                boundingBox.Width += closeButtonWidth + SIZEMARK_CLOSESPACING + SIZEMARK_CLOSERIGHTPADDING;
        }
        if (drawClose && boundingBox.Width > width)
            drawClose = false;

        if (boundingBox.Width <= width)
        {
            boundingBox.Height = (float)sizeHeight;
            boundingBox.X = width/2 - boundingBox.Width/2;
            boundingBox.Y = 0;

            if (options->dwLocation & SIZEMARKLOCATION_LEFT)
                boundingBox.X += (float)sizeHeight;

            path = new GraphicsPath();
            Brush* brush;
            GraphicsState gState = g->Save();
            if (options->dwLocation & SIZEMARKLOCATION_TOP)
            {
                brush = new LinearGradientBrush(PointF(0,0), PointF(0, (float)sizeHeight), Color((SizeMarkColor1.GetValue()&0xffffff) | opacityArgb), Color((SizeMarkColor2.GetValue()&0xffffff) | opacityArgb));

                path->AddLine(boundingBox.X, boundingBox.Height, boundingBox.X, (float)SIZEMARK_ROUNDING);
                path->AddArc(boundingBox.X, 0.0, (float)SIZEMARK_ROUNDING*2, (float)SIZEMARK_ROUNDING*2, 180, 90);
                path->AddLine(boundingBox.X + SIZEMARK_ROUNDING, 0.0, boundingBox.GetRight() - SIZEMARK_ROUNDING, 0.0);
                path->AddArc(boundingBox.GetRight() - SIZEMARK_ROUNDING*2, 0.0, (float)SIZEMARK_ROUNDING*2, (float)SIZEMARK_ROUNDING*2, 270, 90);
                path->AddLine(boundingBox.GetRight(), (float)SIZEMARK_ROUNDING, boundingBox.GetRight(), boundingBox.Height);
                path->CloseFigure();
            }
            else
            {
                g->TranslateTransform(0, (float)(options->lpRect->bottom - options->lpRect->top - sizeHeight - 1));
                brush = new LinearGradientBrush(PointF(0,0), PointF(0, (float)sizeHeight), Color((SizeMarkColor2.GetValue()&0xffffff) | opacityArgb), Color((SizeMarkColor1.GetValue()&0xffffff) | opacityArgb));

                path->AddLine(boundingBox.X, 0.0, boundingBox.X, boundingBox.Height-SIZEMARK_ROUNDING);
                path->AddArc(boundingBox.X, boundingBox.Height-SIZEMARK_ROUNDING*2, (float)SIZEMARK_ROUNDING*2, (float)SIZEMARK_ROUNDING*2, 90, 90);
                path->AddLine(boundingBox.X + SIZEMARK_ROUNDING, boundingBox.Height, boundingBox.GetRight() - SIZEMARK_ROUNDING, boundingBox.Height);
                path->AddArc(boundingBox.GetRight() - SIZEMARK_ROUNDING*2, boundingBox.Height-SIZEMARK_ROUNDING*2, (float)SIZEMARK_ROUNDING*2, (float)SIZEMARK_ROUNDING*2, 0, 90);
                path->AddLine(boundingBox.GetRight(), boundingBox.Height-SIZEMARK_ROUNDING, boundingBox.GetRight(), 0.0);
                path->CloseFigure();
            }

            g->FillPath(brush, path);
            delete brush;

            Pen* pen = new Pen(Color(SizeMarkBorderColor.GetValue() & 0xffffff | opacityArgb), 1.0);
            g->DrawPath(pen, path);
            delete pen;

            if (drawClose)
                boundingBox.Width -= closeButtonWidth + SIZEMARK_CLOSESPACING + SIZEMARK_CLOSERIGHTPADDING;

            brush = new SolidBrush(Color((SizeMarkTextColor.GetValue() & 0xffffff) | opacityArgb));
            g->DrawString(sizeStr, -1, sizeFont, boundingBox, sizeStringFormat, brush);
            delete brush;

            delete path;

            if (drawClose)
            {
                RectF closeButtonRect(
                    boundingBox.X + boundingBox.Width + SIZEMARK_CLOSESPACING,
                    boundingBox.Height/2 - closeButtonHeight/2,
                    closeButtonWidth,
                    closeButtonHeight);

                if (options->lpCloseRect)
                {
                    float closeRectOffsetY = options->dwLocation & SIZEMARKLOCATION_BOTTOM
                        ? (float)(options->lpRect->bottom - options->lpRect->top - sizeHeight - 1)
                        : 0;
                    options->lpCloseRect->left = (int)floorf(closeButtonRect.X);
                    options->lpCloseRect->top = (int)floorf(closeButtonRect.Y + closeRectOffsetY);
                    options->lpCloseRect->right = (int)ceilf(closeButtonRect.GetRight());
                    options->lpCloseRect->bottom = (int)ceilf(closeButtonRect.GetBottom() + closeRectOffsetY);
                }

                if (options->dwOptions & SIZEMARKOPTION_HOVERCLOSE)
                {
                    GraphicsPath closePath;
                    AddRoundedRectangle(&closePath, closeButtonRect, SIZEMARK_CLOSECORNER);
                    Color closeButtonColor = options->dwOptions & SIZEMARKOPTION_CLOSEDOWN ? CloseButtonPressedColor : CloseButtonHoverColor;
                    SolidBrush closeButtonBrush(Color((closeButtonColor.GetValue() & 0xffffff) | opacityArgb));
                    g->FillPath(&closeButtonBrush, &closePath);
                }

                SolidBrush closeGlyphBrush(Color((SizeMarkTextColor.GetValue() & 0xffffff) | opacityArgb));
                PointF closeGlyphOrigin(
                    closeButtonRect.X + (closeButtonRect.Width - closeGlyphBounds.Width)/2 - closeGlyphBounds.X,
                    closeButtonRect.Y + (closeButtonRect.Height - closeGlyphBounds.Height)/2 - closeGlyphBounds.Y);
                g->DrawString(CloseGlyph, -1, closeFont, closeGlyphOrigin, StringFormat::GenericTypographic(), &closeGlyphBrush);
            }

            g->Restore(gState);
        }
    }

    if (options->dwLocation & (SIZEMARKLOCATION_LEFT | SIZEMARKLOCATION_RIGHT))
    {
        _itot_s((int)ceilf(options->lpCropRect->Height), sizeStr, 20, 10);
        _tcscat_s(sizeStr, 20, _T("px"));

        g->MeasureString(sizeStr, -1, sizeFont, PointF(0, 0), &boundingBox);
        boundingBox.Width += SIZEMARK_HORIZONTALPADDING*2;

        if (boundingBox.Width > height)
        {
            sizeStr[_tcslen(sizeStr)-2] = _T('\0');
            g->MeasureString(sizeStr, -1, sizeFont, PointF(0, 0), &boundingBox);
            boundingBox.Width += SIZEMARK_HORIZONTALPADDING*2;
        }

        if (boundingBox.Width <= height)
        {
            boundingBox.Height = (float)sizeHeight;
            boundingBox.X = -boundingBox.Width/2;
            boundingBox.Y = -(float)sizeHeight/2;

            path = new GraphicsPath();
            path->AddLine(boundingBox.X, boundingBox.GetBottom(), boundingBox.X, (float)boundingBox.GetTop()+SIZEMARK_ROUNDING);
            path->AddArc(boundingBox.X, boundingBox.GetTop(), (float)SIZEMARK_ROUNDING*2, (float)SIZEMARK_ROUNDING*2, 180, 90);
            path->AddLine(boundingBox.X + SIZEMARK_ROUNDING, boundingBox.GetTop(), boundingBox.GetRight() - SIZEMARK_ROUNDING, boundingBox.GetTop());
            path->AddArc(boundingBox.GetRight() - SIZEMARK_ROUNDING*2, boundingBox.GetTop(), (float)SIZEMARK_ROUNDING*2, boundingBox.GetTop()+SIZEMARK_ROUNDING*2, 270, 90);
            path->AddLine(boundingBox.GetRight(), boundingBox.GetTop()+SIZEMARK_ROUNDING, boundingBox.GetRight(), boundingBox.GetBottom());
            path->CloseFigure();

            GraphicsState gState = g->Save();
            if (options->dwLocation & SIZEMARKLOCATION_LEFT)
            {
                g->TranslateTransform(-boundingBox.Y, height/2);
                if (options->dwLocation & SIZEMARKLOCATION_TOP)
                    g->TranslateTransform(0, (float)sizeHeight);
                g->RotateTransform(270);
            }
            else
            {
                g->TranslateTransform(options->lpRect->right-options->lpRect->left+boundingBox.Y-1, height/2);
                if (options->dwLocation & SIZEMARKLOCATION_TOP)
                    g->TranslateTransform(0, (float)sizeHeight);
                g->RotateTransform(90);
            }

            Brush* brush = new LinearGradientBrush(PointF(0,-(float)sizeHeight/2), PointF(0, (float)sizeHeight/2), Color((SizeMarkColor1.GetValue()&0xffffff) | opacityArgb), Color((SizeMarkColor2.GetValue()&0xffffff) | opacityArgb));
            g->FillPath(brush, path);
            delete brush;

            Pen* pen = new Pen(Color(SizeMarkBorderColor.GetValue() & 0xffffff | opacityArgb), 1.0);
            g->DrawPath(pen, path);
            delete pen;

            brush = new SolidBrush(Color((SizeMarkTextColor.GetValue() & 0xffffff) | opacityArgb));
            g->DrawString(sizeStr, -1, sizeFont, boundingBox, sizeStringFormat, brush);
            delete brush;

            delete path;
            g->Restore(gState);
        }
    }
}

void ExpandForSizeMarks(const RECT* rect, RECT* newRect, int location)
{
    memcpy(newRect, rect, sizeof(RECT));

    if (location & SIZEMARKLOCATION_LEFT)
        newRect->left -= sizeHeight;
    else if (location & SIZEMARKLOCATION_RIGHT)
        newRect->right += sizeHeight;

    if (location & SIZEMARKLOCATION_TOP)
        newRect->top -= sizeHeight;
    else if (location & SIZEMARKLOCATION_BOTTOM)
        newRect->bottom += sizeHeight;
}

void AdjustForSizeMarks(RECT &rect, int location)
{
    if (location & SIZEMARKLOCATION_LEFT)
        rect.left += sizeHeight;
    else if (location & SIZEMARKLOCATION_RIGHT)
        rect.right -= sizeHeight;

    if (location & SIZEMARKLOCATION_TOP)
        rect.top += sizeHeight;
    else if (location & SIZEMARKLOCATION_BOTTOM)
        rect.bottom -= sizeHeight;
}

void AdjustPointForSizeMarks(POINT &p, int location)
{
    if (location & SIZEMARKLOCATION_LEFT)
        p.x -= sizeHeight;

    if (location & SIZEMARKLOCATION_TOP)
        p.y -= sizeHeight;
}
