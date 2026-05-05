#include "karaoke_composition_renderer.h"

#include "hook_log.h"

#include <d2d1_1.h>
#include <d3d11.h>
#include <dcomp.h>
#include <dwrite.h>
#include <dxgi1_2.h>
#include <string>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dcomp.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "dxgi.lib")

struct DCompState {
    HWND hwnd = nullptr;
    int width = 0;
    int height = 0;
    bool initialized = false;
    bool unavailable = false;

    ID3D11Device* d3dDevice = nullptr;
    IDXGIDevice* dxgiDevice = nullptr;
    ID2D1Factory1* d2dFactory = nullptr;
    ID2D1Device* d2dDevice = nullptr;
    IDWriteFactory* dwriteFactory = nullptr;
    IDCompositionDevice* dcompDevice = nullptr;
    IDCompositionTarget* target = nullptr;
    IDCompositionVisual* rootVisual = nullptr;
    IDCompositionVisual* normalVisual = nullptr;
    IDCompositionVisual* highlightVisual = nullptr;
    IDCompositionRectangleClip* highlightClip = nullptr;
    IDCompositionSurface* normalSurface = nullptr;
    IDCompositionSurface* highlightSurface = nullptr;
    IDWriteTextFormat* textFormat = nullptr;
    IDWriteTextLayout* textLayout = nullptr;

    std::wstring cachedText;
    std::wstring cachedFont;
    int cachedFontSize = 0;
    int cachedFontR = -1;
    int cachedFontG = -1;
    int cachedFontB = -1;
    int cachedHighlightR = -1;
    int cachedHighlightG = -1;
    int cachedHighlightB = -1;
};

static DCompState g_dcomp;

static void SafeRelease(IUnknown*& value)
{
    if (value)
    {
        value->Release();
        value = nullptr;
    }
}

template <typename T>
static void SafeReleaseTyped(T*& value)
{
    if (value)
    {
        value->Release();
        value = nullptr;
    }
}

static void ReleaseSurfaces()
{
    SafeReleaseTyped(g_dcomp.normalSurface);
    SafeReleaseTyped(g_dcomp.highlightSurface);
    SafeReleaseTyped(g_dcomp.textLayout);
    SafeReleaseTyped(g_dcomp.textFormat);
}

void karaoke_dcomp_shutdown(void)
{
    ReleaseSurfaces();
    SafeReleaseTyped(g_dcomp.highlightClip);
    SafeReleaseTyped(g_dcomp.highlightVisual);
    SafeReleaseTyped(g_dcomp.normalVisual);
    SafeReleaseTyped(g_dcomp.rootVisual);
    SafeReleaseTyped(g_dcomp.target);
    SafeReleaseTyped(g_dcomp.dcompDevice);
    SafeReleaseTyped(g_dcomp.dwriteFactory);
    SafeReleaseTyped(g_dcomp.d2dDevice);
    SafeReleaseTyped(g_dcomp.d2dFactory);
    SafeReleaseTyped(g_dcomp.dxgiDevice);
    SafeReleaseTyped(g_dcomp.d3dDevice);
    g_dcomp = DCompState{};
}

static void MarkUnavailable(const char* where, HRESULT hr)
{
    Log("DComp fallback reason: %s failed hr=0x%08lx\n", where, hr);
    g_dcomp.unavailable = true;
    karaoke_dcomp_shutdown();
    g_dcomp.unavailable = true;
}

static BOOL EnsureInitialized(HWND hwndOverlay, int width, int height)
{
    if (g_dcomp.unavailable)
        return FALSE;

    if (g_dcomp.initialized && g_dcomp.hwnd == hwndOverlay && g_dcomp.width == width && g_dcomp.height == height)
        return TRUE;

    karaoke_dcomp_shutdown();
    g_dcomp.hwnd = hwndOverlay;
    g_dcomp.width = width;
    g_dcomp.height = height;

    Log("DComp init begin hwnd=%p size=%dx%d\n", hwndOverlay, width, height);

    LONG_PTR exStyle = GetWindowLongPtrW(hwndOverlay, GWL_EXSTYLE);
    if (exStyle & WS_EX_LAYERED)
    {
        SetWindowLongPtrW(hwndOverlay, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED);
        SetWindowPos(hwndOverlay, NULL, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        Log("DComp removed WS_EX_LAYERED from overlay\n");
    }

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0
    };
    D3D_FEATURE_LEVEL actualLevel = D3D_FEATURE_LEVEL_10_0;
    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        featureLevels,
        ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION,
        &g_dcomp.d3dDevice,
        &actualLevel,
        nullptr);
    if (FAILED(hr))
    {
        hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            featureLevels,
            ARRAYSIZE(featureLevels),
            D3D11_SDK_VERSION,
            &g_dcomp.d3dDevice,
            &actualLevel,
            nullptr);
    }
    if (FAILED(hr))
    {
        MarkUnavailable("D3D11CreateDevice", hr);
        return FALSE;
    }

    hr = g_dcomp.d3dDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&g_dcomp.dxgiDevice));
    if (FAILED(hr))
    {
        MarkUnavailable("Query IDXGIDevice", hr);
        return FALSE;
    }

    D2D1_FACTORY_OPTIONS options = {};
    hr = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        __uuidof(ID2D1Factory1),
        &options,
        reinterpret_cast<void**>(&g_dcomp.d2dFactory));
    if (FAILED(hr))
    {
        MarkUnavailable("D2D1CreateFactory", hr);
        return FALSE;
    }

    hr = g_dcomp.d2dFactory->CreateDevice(g_dcomp.dxgiDevice, &g_dcomp.d2dDevice);
    if (FAILED(hr))
    {
        MarkUnavailable("Create D2D device", hr);
        return FALSE;
    }

    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(&g_dcomp.dwriteFactory));
    if (FAILED(hr))
    {
        MarkUnavailable("DWriteCreateFactory", hr);
        return FALSE;
    }

    hr = DCompositionCreateDevice(
        g_dcomp.dxgiDevice,
        __uuidof(IDCompositionDevice),
        reinterpret_cast<void**>(&g_dcomp.dcompDevice));
    if (FAILED(hr))
    {
        MarkUnavailable("DCompositionCreateDevice", hr);
        return FALSE;
    }

    hr = g_dcomp.dcompDevice->CreateTargetForHwnd(hwndOverlay, TRUE, &g_dcomp.target);
    if (FAILED(hr))
    {
        MarkUnavailable("CreateTargetForHwnd", hr);
        return FALSE;
    }

    hr = g_dcomp.dcompDevice->CreateVisual(&g_dcomp.rootVisual);
    if (SUCCEEDED(hr))
        hr = g_dcomp.dcompDevice->CreateVisual(&g_dcomp.normalVisual);
    if (SUCCEEDED(hr))
        hr = g_dcomp.dcompDevice->CreateVisual(&g_dcomp.highlightVisual);
    if (SUCCEEDED(hr))
        hr = g_dcomp.dcompDevice->CreateRectangleClip(&g_dcomp.highlightClip);
    if (FAILED(hr))
    {
        MarkUnavailable("CreateVisual/Clip", hr);
        return FALSE;
    }

    g_dcomp.highlightClip->SetLeft(0.0f);
    g_dcomp.highlightClip->SetTop(0.0f);
    g_dcomp.highlightClip->SetRight(0.0f);
    g_dcomp.highlightClip->SetBottom((float)height);
    g_dcomp.highlightVisual->SetClip(g_dcomp.highlightClip);

    g_dcomp.rootVisual->AddVisual(g_dcomp.normalVisual, FALSE, nullptr);
    g_dcomp.rootVisual->AddVisual(g_dcomp.highlightVisual, TRUE, g_dcomp.normalVisual);
    g_dcomp.target->SetRoot(g_dcomp.rootVisual);
    hr = g_dcomp.dcompDevice->Commit();
    if (FAILED(hr))
    {
        MarkUnavailable("Initial Commit", hr);
        return FALSE;
    }

    g_dcomp.initialized = true;
    Log("DComp init ok feature=0x%x\n", actualLevel);
    return TRUE;
}

static HRESULT CreateSurface(int width, int height, IDCompositionSurface** surface)
{
    return g_dcomp.dcompDevice->CreateSurface(
        width,
        height,
        DXGI_FORMAT_B8G8R8A8_UNORM,
        DXGI_ALPHA_MODE_PREMULTIPLIED,
        surface);
}

static HRESULT CreateLyricTextFormat(const ML_CONFIG* cfg, IDWriteTextFormat** format)
{
    HRESULT hr = g_dcomp.dwriteFactory->CreateTextFormat(
        cfg->font_name,
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        (FLOAT)cfg->font_size,
        L"",
        format);
    if (SUCCEEDED(hr))
    {
        (*format)->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        (*format)->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        (*format)->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    }
    return hr;
}

static HRESULT RenderSurface(IDCompositionSurface* surface, int width, int height, const ML_CONFIG* cfg,
    const wchar_t* text, IDWriteTextFormat* format, bool highlight)
{
    RECT updateRect = {0, 0, width, height};
    POINT offset = {};
    IDXGISurface* dxgiSurface = nullptr;
    HRESULT hr = surface->BeginDraw(&updateRect, __uuidof(IDXGISurface), reinterpret_cast<void**>(&dxgiSurface), &offset);
    if (FAILED(hr))
        return hr;

    ID2D1RenderTarget* renderTarget = nullptr;
    D2D1_RENDER_TARGET_PROPERTIES rtProps = {};
    rtProps.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
    rtProps.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    rtProps.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    rtProps.usage = D2D1_RENDER_TARGET_USAGE_NONE;
    rtProps.minLevel = D2D1_FEATURE_LEVEL_DEFAULT;

    hr = g_dcomp.d2dFactory->CreateDxgiSurfaceRenderTarget(dxgiSurface, &rtProps, &renderTarget);

    ID2D1SolidColorBrush* brush = nullptr;
    if (SUCCEEDED(hr))
    {
        D2D1_COLOR_F color = highlight
            ? D2D1::ColorF(cfg->highlight_r / 255.0f, cfg->highlight_g / 255.0f, cfg->highlight_b / 255.0f, 1.0f)
            : D2D1::ColorF(cfg->font_r / 255.0f, cfg->font_g / 255.0f, cfg->font_b / 255.0f, 1.0f);
        hr = renderTarget->CreateSolidColorBrush(color, &brush);
    }

    if (SUCCEEDED(hr))
    {
        renderTarget->BeginDraw();
        renderTarget->SetTransform(D2D1::Matrix3x2F::Translation((FLOAT)offset.x, (FLOAT)offset.y));
        renderTarget->Clear(D2D1::ColorF(0, 0.0f));
        renderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        renderTarget->DrawTextW(
            text,
            (UINT32)wcslen(text),
            format,
            D2D1::RectF(0.0f, 0.0f, (FLOAT)width, (FLOAT)height),
            brush,
            D2D1_DRAW_TEXT_OPTIONS_CLIP);
        hr = renderTarget->EndDraw();
    }

    SafeReleaseTyped(brush);
    SafeReleaseTyped(renderTarget);
    SafeReleaseTyped(dxgiSurface);

    HRESULT endHr = surface->EndDraw();
    return FAILED(hr) ? hr : endHr;
}

static bool LineCacheInvalid(int width, int height, const ML_CONFIG* cfg, const wchar_t* text)
{
    return !g_dcomp.normalSurface ||
        !g_dcomp.highlightSurface ||
        !g_dcomp.textFormat ||
        !g_dcomp.textLayout ||
        g_dcomp.width != width ||
        g_dcomp.height != height ||
        g_dcomp.cachedText != text ||
        g_dcomp.cachedFont != cfg->font_name ||
        g_dcomp.cachedFontSize != cfg->font_size ||
        g_dcomp.cachedFontR != cfg->font_r ||
        g_dcomp.cachedFontG != cfg->font_g ||
        g_dcomp.cachedFontB != cfg->font_b ||
        g_dcomp.cachedHighlightR != cfg->highlight_r ||
        g_dcomp.cachedHighlightG != cfg->highlight_g ||
        g_dcomp.cachedHighlightB != cfg->highlight_b;
}

static BOOL RebuildLineSurfaces(int width, int height, const ML_CONFIG* cfg, const wchar_t* text)
{
    ReleaseSurfaces();

    HRESULT hr = CreateSurface(width, height, &g_dcomp.normalSurface);
    if (SUCCEEDED(hr))
        hr = CreateSurface(width, height, &g_dcomp.highlightSurface);
    if (SUCCEEDED(hr))
        hr = CreateLyricTextFormat(cfg, &g_dcomp.textFormat);
    if (SUCCEEDED(hr))
        hr = g_dcomp.dwriteFactory->CreateTextLayout(
            text,
            (UINT32)wcslen(text),
            g_dcomp.textFormat,
            (FLOAT)width,
            (FLOAT)height,
            &g_dcomp.textLayout);
    if (FAILED(hr))
    {
        Log("DComp line resource create failed hr=0x%08lx\n", hr);
        ReleaseSurfaces();
        return FALSE;
    }

    hr = RenderSurface(g_dcomp.normalSurface, width, height, cfg, text, g_dcomp.textFormat, false);
    if (SUCCEEDED(hr))
        hr = RenderSurface(g_dcomp.highlightSurface, width, height, cfg, text, g_dcomp.textFormat, true);
    if (FAILED(hr))
    {
        Log("DComp surface render failed hr=0x%08lx\n", hr);
        MarkUnavailable("RenderSurface", hr);
        return FALSE;
    }

    g_dcomp.normalVisual->SetContent(g_dcomp.normalSurface);
    g_dcomp.highlightVisual->SetContent(g_dcomp.highlightSurface);
    g_dcomp.cachedText = text;
    g_dcomp.cachedFont = cfg->font_name;
    g_dcomp.cachedFontSize = cfg->font_size;
    g_dcomp.cachedFontR = cfg->font_r;
    g_dcomp.cachedFontG = cfg->font_g;
    g_dcomp.cachedFontB = cfg->font_b;
    g_dcomp.cachedHighlightR = cfg->highlight_r;
    g_dcomp.cachedHighlightG = cfg->highlight_g;
    g_dcomp.cachedHighlightB = cfg->highlight_b;

    Log("DComp frame line changed text_len=%zu size=%dx%d\n", wcslen(text), width, height);
    return TRUE;
}

static FLOAT MeasureCaretWidth(UINT32 charPosition)
{
    if (!g_dcomp.textLayout)
        return 0.0f;

    UINT32 len = (UINT32)g_dcomp.cachedText.size();
    if (charPosition > len)
        charPosition = len;

    FLOAT x = 0.0f;
    FLOAT y = 0.0f;
    DWRITE_HIT_TEST_METRICS metrics = {};
    HRESULT hr = g_dcomp.textLayout->HitTestTextPosition(charPosition, FALSE, &x, &y, &metrics);
    return SUCCEEDED(hr) ? x : 0.0f;
}

BOOL karaoke_dcomp_set_frame(HWND hwndOverlay, int width, int height, const ML_CONFIG* cfg, const ML_LYRIC_RENDER_FRAME* frame)
{
    if (!hwndOverlay || !cfg || width <= 0 || height <= 0)
        return FALSE;

    if (!EnsureInitialized(hwndOverlay, width, height))
        return FALSE;

    const wchar_t* text = cfg->text_content;
    ML_LYRIC_RENDER_FRAME emptyFrame = {};
    if (frame && frame->active && frame->text[0])
        text = frame->text;
    else
        frame = &emptyFrame;

    if (LineCacheInvalid(width, height, cfg, text) && !RebuildLineSurfaces(width, height, cfg, text))
        return FALSE;

    if (g_dcomp.unavailable)
        return FALSE;

    UINT32 startChars = frame ? frame->highlight_chars : 0;
    UINT32 endChars = frame ? frame->highlight_end_chars : startChars;
    UINT32 progress = frame ? frame->highlight_progress : 0;
    if (progress > 10000)
        progress = 10000;

    FLOAT startWidth = MeasureCaretWidth(startChars);
    FLOAT endWidth = MeasureCaretWidth(endChars);
    if (endWidth < startWidth)
        endWidth = startWidth;
    FLOAT clipRight = startWidth + (endWidth - startWidth) * ((FLOAT)progress / 10000.0f);
    if (clipRight < 0.0f)
        clipRight = 0.0f;
    if (clipRight > (FLOAT)width)
        clipRight = (FLOAT)width;

    g_dcomp.highlightClip->SetLeft(0.0f);
    g_dcomp.highlightClip->SetTop(0.0f);
    g_dcomp.highlightClip->SetRight(clipRight);
    g_dcomp.highlightClip->SetBottom((FLOAT)height);

    HRESULT hr = g_dcomp.dcompDevice->Commit();
    if (FAILED(hr))
    {
        Log("DComp commit failed hr=0x%08lx\n", hr);
        return FALSE;
    }

    static int s_clipLogCount = 0;
    if (s_clipLogCount < 20)
    {
        Log("DComp clip update width=%.2f chars=%u-%u progress=%u\n", clipRight, startChars, endChars, progress);
        ++s_clipLogCount;
    }
    return TRUE;
}
