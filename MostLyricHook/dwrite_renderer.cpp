#include "dwrite_renderer.h"
#include "hook_log.h"

#include <d2d1.h>
#include <dwrite.h>
#include <stdint.h>

#pragma comment(lib, "D2d1.lib")
#pragma comment(lib, "Dwrite.lib")

static ID2D1Factory* g_d2dFactory = nullptr;
static IDWriteFactory* g_dwriteFactory = nullptr;

static void SafeRelease(IUnknown* unknown)
{
    if (unknown)
        unknown->Release();
}

static BOOL EnsureFactories(void)
{
    if (!g_d2dFactory)
    {
        HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_d2dFactory);
        if (FAILED(hr))
        {
            Log("D2D1CreateFactory failed: 0x%08lx\n", hr);
            return FALSE;
        }
    }

    if (!g_dwriteFactory)
    {
        HRESULT hr = DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(&g_dwriteFactory));
        if (FAILED(hr))
        {
            Log("DWriteCreateFactory failed: 0x%08lx\n", hr);
            return FALSE;
        }
    }

    return TRUE;
}

static BOOL RenderText(HWND hwndOverlay, int width, int height, const ML_CONFIG* cfg, const wchar_t* text, const ML_LYRIC_RENDER_FRAME* frame)
{
    if (!hwndOverlay || !cfg || !text || width <= 0 || height <= 0)
        return FALSE;

    LONG_PTR exStyle = GetWindowLongPtrW(hwndOverlay, GWL_EXSTYLE);
    if (!(exStyle & WS_EX_LAYERED))
    {
        SetWindowLongPtrW(hwndOverlay, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
        SetWindowPos(hwndOverlay, NULL, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }

    if (!EnsureFactories())
        return FALSE;

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hBitmap || !bits)
    {
        Log("CreateDIBSection failed: %lu\n", GetLastError());
        if (hBitmap)
            DeleteObject(hBitmap);
        return FALSE;
    }

    memset(bits, 0, (size_t)width * (size_t)height * 4);

    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HGDIOBJ oldBitmap = SelectObject(hdcMem, hBitmap);

    ID2D1DCRenderTarget* renderTarget = nullptr;
    D2D1_RENDER_TARGET_PROPERTIES rtProps = {};
    rtProps.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
    rtProps.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    rtProps.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    rtProps.usage = D2D1_RENDER_TARGET_USAGE_NONE;
    rtProps.minLevel = D2D1_FEATURE_LEVEL_DEFAULT;

    HRESULT hr = g_d2dFactory->CreateDCRenderTarget(&rtProps, &renderTarget);
    if (SUCCEEDED(hr))
    {
        RECT bindRect = {0, 0, width, height};
        hr = renderTarget->BindDC(hdcMem, &bindRect);
    }

    IDWriteTextFormat* textFormat = nullptr;
    if (SUCCEEDED(hr))
    {
        hr = g_dwriteFactory->CreateTextFormat(
            cfg->font_name,
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            (FLOAT)cfg->font_size,
            L"",
            &textFormat);
    }

    ID2D1SolidColorBrush* brush = nullptr;
    if (SUCCEEDED(hr))
    {
        textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        textFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

        D2D1_COLOR_F color = {
            cfg->font_r / 255.0f,
            cfg->font_g / 255.0f,
            cfg->font_b / 255.0f,
            1.0f
        };
        hr = renderTarget->CreateSolidColorBrush(color, &brush);
    }

    ID2D1SolidColorBrush* highlightBrush = nullptr;
    UINT32 highlightChars = frame ? frame->highlight_chars : 0;
    UINT32 highlightEndChars = frame ? frame->highlight_end_chars : highlightChars;
    UINT32 highlightProgress = frame ? frame->highlight_progress : 0;

    if (SUCCEEDED(hr) && (highlightChars > 0 || highlightEndChars > highlightChars))
    {
        D2D1_COLOR_F highlight = {
            cfg->highlight_r / 255.0f,
            cfg->highlight_g / 255.0f,
            cfg->highlight_b / 255.0f,
            1.0f
        };
        hr = renderTarget->CreateSolidColorBrush(highlight, &highlightBrush);
    }

    if (SUCCEEDED(hr))
    {
        D2D1_RECT_F layoutRect = D2D1::RectF(0.0f, 0.0f, (FLOAT)width, (FLOAT)height);
        UINT32 textLen = (UINT32)wcslen(text);
        if (highlightChars > textLen)
            highlightChars = textLen;
        if (highlightEndChars > textLen)
            highlightEndChars = textLen;
        if (highlightEndChars < highlightChars)
            highlightEndChars = highlightChars;
        if (highlightProgress > 10000)
            highlightProgress = 10000;

        renderTarget->BeginDraw();
        renderTarget->Clear(D2D1::ColorF(0, 0.0f));
        renderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        renderTarget->DrawTextW(
            text,
            textLen,
            textFormat,
            layoutRect,
            brush,
            D2D1_DRAW_TEXT_OPTIONS_CLIP);
        if (highlightBrush && (highlightChars > 0 || highlightEndChars > highlightChars))
        {
            IDWriteTextLayout* fullLayout = nullptr;
            IDWriteTextLayout* prefixLayout = nullptr;
            IDWriteTextLayout* endLayout = nullptr;
            DWRITE_TEXT_METRICS fullMetrics = {};
            DWRITE_TEXT_METRICS prefixMetrics = {};
            DWRITE_TEXT_METRICS endMetrics = {};
            HRESULT layoutHr = g_dwriteFactory->CreateTextLayout(
                text, textLen, textFormat, (FLOAT)width, (FLOAT)height, &fullLayout);
            if (SUCCEEDED(layoutHr))
                layoutHr = g_dwriteFactory->CreateTextLayout(
                    text, highlightChars, textFormat, (FLOAT)width, (FLOAT)height, &prefixLayout);
            if (SUCCEEDED(layoutHr))
                layoutHr = g_dwriteFactory->CreateTextLayout(
                    text, highlightEndChars, textFormat, (FLOAT)width, (FLOAT)height, &endLayout);
            if (SUCCEEDED(layoutHr))
                layoutHr = fullLayout->GetMetrics(&fullMetrics);
            if (SUCCEEDED(layoutHr))
                layoutHr = prefixLayout->GetMetrics(&prefixMetrics);
            if (SUCCEEDED(layoutHr))
                layoutHr = endLayout->GetMetrics(&endMetrics);
            if (SUCCEEDED(layoutHr))
            {
                FLOAT textLeft = ((FLOAT)width - fullMetrics.widthIncludingTrailingWhitespace) * 0.5f;
                if (textLeft < 0.0f)
                    textLeft = 0.0f;
                FLOAT prefixWidth = prefixMetrics.widthIncludingTrailingWhitespace;
                FLOAT activeWidth = endMetrics.widthIncludingTrailingWhitespace - prefixWidth;
                if (activeWidth < 0.0f)
                    activeWidth = 0.0f;
                FLOAT highlightWidth = prefixWidth + activeWidth * ((FLOAT)highlightProgress / 10000.0f);
                D2D1_RECT_F clipRect = D2D1::RectF(
                    textLeft,
                    0.0f,
                    textLeft + highlightWidth,
                    (FLOAT)height);
                renderTarget->PushAxisAlignedClip(clipRect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
                renderTarget->DrawTextW(
                    text,
                    textLen,
                    textFormat,
                    layoutRect,
                    highlightBrush,
                    D2D1_DRAW_TEXT_OPTIONS_CLIP);
                renderTarget->PopAxisAlignedClip();
            }
            SafeRelease(endLayout);
            SafeRelease(prefixLayout);
            SafeRelease(fullLayout);
        }
        hr = renderTarget->EndDraw();
    }

    BOOL updated = FALSE;
    if (SUCCEEDED(hr))
    {
        SIZE size = {width, height};
        POINT src = {0, 0};
        BLENDFUNCTION blend = {};
        blend.BlendOp = AC_SRC_OVER;
        blend.SourceConstantAlpha = 255;
        blend.AlphaFormat = AC_SRC_ALPHA;

        updated = UpdateLayeredWindow(
            hwndOverlay,
            nullptr,
            nullptr,
            &size,
            hdcMem,
            &src,
            0,
            &blend,
            ULW_ALPHA);
        if (!updated)
            Log("UpdateLayeredWindow failed: %lu\n", GetLastError());
    }
    else
    {
        Log("DirectWrite overlay render failed: 0x%08lx\n", hr);
    }

    SafeRelease(brush);
    SafeRelease(highlightBrush);
    SafeRelease(textFormat);
    SafeRelease(renderTarget);

    SelectObject(hdcMem, oldBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
    DeleteObject(hBitmap);

    return updated;
}

BOOL dwrite_render_overlay(HWND hwndOverlay, int width, int height, const ML_CONFIG* cfg)
{
    if (!cfg)
        return FALSE;
    return RenderText(hwndOverlay, width, height, cfg, cfg->text_content, nullptr);
}

BOOL dwrite_render_overlay_frame(HWND hwndOverlay, int width, int height, const ML_CONFIG* cfg, const ML_LYRIC_RENDER_FRAME* frame)
{
    if (!cfg || !frame || !frame->active || frame->text[0] == L'\0')
        return dwrite_render_overlay(hwndOverlay, width, height, cfg);
    return RenderText(hwndOverlay, width, height, cfg, frame->text, frame);
}

void dwrite_renderer_shutdown(void)
{
    SafeRelease(g_dwriteFactory);
    SafeRelease(g_d2dFactory);
    g_dwriteFactory = nullptr;
    g_d2dFactory = nullptr;
}
