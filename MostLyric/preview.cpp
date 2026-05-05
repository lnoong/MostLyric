#include "preview.h"
#include <Windows.h>
#include "../config/config.h"

#define TRANSPARENT_COLOR RGB(1, 1, 1)

static HWND g_hwndPreview = NULL;

static LRESULT CALLBACK PreviewWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            const ML_CONFIG* cfg = (const ML_CONFIG*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
            if (cfg)
            {
                RECT rc;
                GetClientRect(hwnd, &rc);

                HDC hdcScreen = GetDC(NULL);
                HDC hdcMem = CreateCompatibleDC(hdcScreen);

                BITMAPINFO bmi = {0};
                bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                int w = rc.right - rc.left;
                int h = rc.bottom - rc.top;
                bmi.bmiHeader.biWidth = w;
                bmi.bmiHeader.biHeight = h;
                bmi.bmiHeader.biPlanes = 1;
                bmi.bmiHeader.biBitCount = 32;
                bmi.bmiHeader.biCompression = BI_RGB;
                bmi.bmiHeader.biSizeImage = w * h * 4;

                PVOID pBits = NULL;
                HBITMAP hBmp = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
                ReleaseDC(NULL, hdcScreen);

                if (hBmp && pBits)
                {
                    HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, hBmp);

                    HBRUSH hBg = CreateSolidBrush(TRANSPARENT_COLOR);
                    FillRect(hdcMem, &rc, hBg);
                    DeleteObject(hBg);

                    SetTextColor(hdcMem, RGB(cfg->font_r, cfg->font_g, cfg->font_b));
                    SetBkMode(hdcMem, TRANSPARENT);

                    HFONT hFont = CreateFontW(cfg->font_size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, cfg->font_name);
                    if (hFont)
                    {
                        HFONT hOldFont = (HFONT)SelectObject(hdcMem, hFont);
                        DrawTextW(hdcMem, cfg->text_content, -1, &rc,
                            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                        SelectObject(hdcMem, hOldFont);
                        DeleteObject(hFont);
                    }

                    POINT ptSrc = {0, 0};
                    SIZE sz = {w, h};
                    UpdateLayeredWindow(hwnd, NULL, NULL, &sz, hdcMem, &ptSrc, TRANSPARENT_COLOR, NULL, ULW_COLORKEY);

                    SelectObject(hdcMem, hOldBmp);
                    DeleteObject(hBmp);
                }
                DeleteDC(hdcMem);
            }
            EndPaint(hwnd, &ps);
        }
        break;

    case WM_LBUTTONDOWN:
        DestroyWindow(hwnd);
        g_hwndPreview = NULL;
        break;

    case WM_ERASEBKGND:
        return 1;

    case WM_DESTROY:
        g_hwndPreview = NULL;
        break;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

void ShowPreview(const ML_CONFIG* cfg, HINSTANCE hInst)
{
    ClosePreview();

    static BOOL registered = FALSE;
    if (!registered)
    {
        WNDCLASSW wc = {0};
        wc.lpfnWndProc = PreviewWndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = L"MostLyricPreview";
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = NULL;
        RegisterClassW(&wc);
        registered = TRUE;
    }

    int x, y;
    if (cfg->pos_x >= 0 && cfg->pos_y >= 0)
    {
        x = cfg->pos_x;
        y = cfg->pos_y;
    }
    else
    {
        HWND hwndTaskbar = FindWindowW(L"Shell_TrayWnd", NULL);
        RECT rcTaskbar = {};
        if (hwndTaskbar) GetWindowRect(hwndTaskbar, &rcTaskbar);
        x = rcTaskbar.right - cfg->width - 320;
        y = rcTaskbar.top + (rcTaskbar.bottom - rcTaskbar.top - cfg->height) / 2;
    }

    g_hwndPreview = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED,
        L"MostLyricPreview", L"MostLyric Preview",
        WS_POPUP | WS_VISIBLE,
        x, y, cfg->width, cfg->height,
        NULL, NULL, hInst, NULL
    );

    if (g_hwndPreview)
    {
        SetWindowLongPtr(g_hwndPreview, GWLP_USERDATA, (LONG_PTR)cfg);
    }
}

void ClosePreview()
{
    if (g_hwndPreview)
    {
        DestroyWindow(g_hwndPreview);
        g_hwndPreview = NULL;
    }
}
