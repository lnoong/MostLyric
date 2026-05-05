#include <Windows.h>
#include <stdio.h>
#include <ShlObj.h>
#include <CommCtrl.h>
#include <UxTheme.h>
#include "../config/config.h"
#include "config_reader.h"
#include "pipe_server.h"
#include "dwrite_renderer.h"
#include "desktop_host.h"
#include "hook_log.h"
#include "karaoke_composition_renderer.h"
#include "lyric_engine.h"
#include "taskbar_host.h"
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "UxTheme.lib")

#define ML_HOOK_VERSION "qqmusic-boundary-sync-v28"
#define ML_REBAR_BAND_ID 0x4D4C
#define ML_BAND_CLASS_NAME L"MostLyricBandWindow"
#define ML_OVERLAY_CLASS_NAME L"MostLyricOverlayWindow"
#define ML_WM_PAINT_LYRIC (WM_APP + 0x4D4C)
#define ML_TIMER_REPOSITION 1
#define ML_TIMER_RENDER_FRAME 2
#define ML_REPOSITION_INTERVAL_MS 250
#define ML_RENDER_INTERVAL_MS 16

HINSTANCE g_hInst = NULL;
HWND g_hwndBand = NULL;
HWND g_hwndTaskbar = NULL;
HWND g_hwndRebar = NULL;
HWND g_hwndTaskSw = NULL;
HWND g_hwndVisualHost = NULL;
HWND g_hwndOverlay = NULL;
HWND g_hwndWorkerW = NULL;
HWND g_hwndDesktopDefView = NULL;
HANDLE g_hPipeThread = NULL;
DWORD g_dwBandThreadId = 0;
static BOOL g_cleanupDone = FALSE;
static BOOL g_selfUnloadRequested = FALSE;
static BOOL g_useRebarBand = FALSE;
static UINT g_rebarBandIndex = (UINT)-1;
static RECT g_rebarBandRect = {0};

void Cleanup(void);
static void PositionOverlayFromRebarRect(void);

static BOOL IsDesktopTarget(void)
{
    const ML_CONFIG* cfg = config_reader_get();
    return cfg && cfg->display_target == ML_DISPLAY_TARGET_DESKTOP;
}

static void PaintLyricOnOverlayDC(HDC paintHdc)
{
    static BOOL s_loggedPaint = FALSE;
    static int s_callCount = 0;
    BOOL verbose = s_callCount < 3;
    UNREFERENCED_PARAMETER(paintHdc);

    if (s_callCount < 10)
        Log("RenderLyricOverlay called #%d visual=%p overlay=%p\n",
            ++s_callCount, g_hwndVisualHost, g_hwndOverlay);

    if (!g_hwndOverlay)
    {
        Log("Overlay DC paint skipped: no overlay hwnd\n");
        return;
    }

    RECT rc = {0};
    GetClientRect(g_hwndOverlay, &rc);
    if (verbose)
        Log("Probe: overlay client rect=(%ld,%ld,%ld,%ld) index=%u\n",
            rc.left, rc.top, rc.right, rc.bottom, g_rebarBandIndex);
    if (rc.right <= rc.left || rc.bottom <= rc.top)
    {
        Log("Overlay DC paint skipped: empty client rect=(%ld,%ld,%ld,%ld)\n",
            rc.left, rc.top, rc.right, rc.bottom);
        return;
    }

    if (verbose)
        Log("Probe: before config_reader_get\n");
    const ML_CONFIG* cfg = config_reader_get();
    if (verbose)
        Log("Probe: after config_reader_get cfg=%p\n", cfg);
    if (cfg)
    {
        if (!s_loggedPaint)
        {
            Log("Overlay DC paint index=%u rc=(%ld,%ld,%ld,%ld) text_len=%zu color=(%d,%d,%d)\n",
                g_rebarBandIndex, rc.left, rc.top, rc.right, rc.bottom,
                wcslen(cfg->text_content), cfg->font_r, cfg->font_g, cfg->font_b);
            s_loggedPaint = TRUE;
        }
        if (verbose)
            Log("Probe: before karaoke render\n");
        ML_LYRIC_RENDER_FRAME frame = {0};
        lyric_engine_get_frame(&frame);
        if (!karaoke_dcomp_set_frame(g_hwndOverlay, rc.right - rc.left, rc.bottom - rc.top, cfg, &frame))
            dwrite_render_overlay_frame(g_hwndOverlay, rc.right - rc.left, rc.bottom - rc.top, cfg, &frame);
        if (verbose)
            Log("Probe: after karaoke render\n");
    }
}

static RECT GetBandRect(HWND hwndTaskbar)
{
    const ML_CONFIG* cfg = config_reader_get();
    RECT rcTaskbar = {0};
    GetClientRect(hwndTaskbar, &rcTaskbar);

    int x, y;
    if (cfg->pos_x >= 0 && cfg->pos_y >= 0)
    {
        x = cfg->pos_x;
        y = cfg->pos_y;
    }
    else
    {
        x = rcTaskbar.right - cfg->width - 320;
        y = (rcTaskbar.bottom - rcTaskbar.top - cfg->height) / 2;
    }

    RECT rc = { x, y, x + cfg->width, y + cfg->height };
    return rc;
}

static HWND FindDesktopWorkerW(void)
{
    ML_DESKTOP_HOST desktopHost = desktop_host_find();
    g_hwndDesktopDefView = desktopHost.defView;
    return desktopHost.host;
}

static void UpdateRebarBandMetrics(void)
{
    if (!g_useRebarBand || !g_hwndRebar || !IsWindow(g_hwndRebar) || !g_hwndBand)
        return;

    const ML_CONFIG* cfg = config_reader_get();
    REBARBANDINFO rbi = {0};
    rbi.cbSize = sizeof(rbi);
    rbi.fMask = RBBIM_CHILDSIZE | RBBIM_SIZE | RBBIM_IDEALSIZE;
    rbi.cxMinChild = cfg->width;
    rbi.cyMinChild = cfg->height;
    rbi.cyChild = cfg->height;
    rbi.cyMaxChild = cfg->height;
    rbi.cx = cfg->width;
    rbi.cxIdeal = cfg->width;

    UINT index = (UINT)SendMessageW(g_hwndRebar, RB_IDTOINDEX, ML_REBAR_BAND_ID, 0);
    if (index != (UINT)-1)
    {
        SendMessageW(g_hwndRebar, RB_SETBANDINFO, index, (LPARAM)&rbi);
        g_rebarBandIndex = index;
        SendMessageW(g_hwndRebar, RB_GETRECT, index, (LPARAM)&g_rebarBandRect);
        PositionOverlayFromRebarRect();
    }
}

static void PositionOverlayFromRebarRect(void)
{
    if (!g_hwndOverlay)
        return;

    const ML_CONFIG* cfg = config_reader_get();
    if (!cfg)
        return;

    BOOL desktopTarget = cfg->display_target == ML_DISPLAY_TARGET_DESKTOP;
    HWND hwndHost = desktopTarget ? g_hwndWorkerW : g_hwndVisualHost;
    if (!hwndHost && desktopTarget)
    {
        g_hwndWorkerW = FindDesktopWorkerW();
        hwndHost = g_hwndWorkerW;
    }
    if (!hwndHost)
        return;

    if (GetParent(g_hwndOverlay) != hwndHost)
        SetParent(g_hwndOverlay, hwndHost);

    RECT rc = {0};
    int width = cfg->width;
    int height = cfg->height;
    BOOL rcIsScreen = FALSE;

    if (cfg->pos_x >= 0 && cfg->pos_y >= 0)
    {
        rc.left = cfg->pos_x;
        rc.top = cfg->pos_y;
        rc.right = rc.left + width;
        rc.bottom = rc.top + height;
        rcIsScreen = TRUE;
    }
    else if (desktopTarget)
    {
        RECT hostClient = {0};
        GetClientRect(hwndHost, &hostClient);
        rc.left = (hostClient.right - hostClient.left - width) / 2;
        rc.top = (hostClient.bottom - hostClient.top - height) / 2;
        rc.right = rc.left + width;
        rc.bottom = rc.top + height;
    }
    else if (g_useRebarBand && g_hwndRebar && IsWindow(g_hwndRebar))
    {
        rc = g_rebarBandRect;
        MapWindowPoints(g_hwndRebar, NULL, (POINT*)&rc, 2);
        width = rc.right - rc.left;
        height = rc.bottom - rc.top;
        rcIsScreen = TRUE;
    }
    else if (g_hwndTaskbar)
    {
        rc = GetBandRect(g_hwndTaskbar);
        MapWindowPoints(g_hwndTaskbar, NULL, (POINT*)&rc, 2);
        width = rc.right - rc.left;
        height = rc.bottom - rc.top;
        rcIsScreen = TRUE;
    }
    else
    {
        return;
    }

    if (rcIsScreen)
        MapWindowPoints(NULL, hwndHost, (POINT*)&rc, 2);

    RECT hostClient = {0};
    GetClientRect(hwndHost, &hostClient);
    if (width > hostClient.right - hostClient.left)
        width = hostClient.right - hostClient.left;
    if (height > hostClient.bottom - hostClient.top)
        height = hostClient.bottom - hostClient.top;

    int maxX = hostClient.right - width;
    int maxY = hostClient.bottom - height;
    if (maxX < hostClient.left)
        maxX = hostClient.left;
    if (maxY < hostClient.top)
        maxY = hostClient.top;

    if (rc.left < hostClient.left)
        rc.left = hostClient.left;
    if (rc.left > maxX)
        rc.left = maxX;
    if (rc.top < hostClient.top)
        rc.top = hostClient.top;
    if (rc.top > maxY)
        rc.top = maxY;

    static int s_positionLogCount = 0;
    if (s_positionLogCount < 12)
    {
        Log("Overlay position #%d cfg=(%d,%d %dx%d) hostClient=(%ld,%ld,%ld,%ld) final=(%ld,%ld %dx%d)\n",
            ++s_positionLogCount, cfg->pos_x, cfg->pos_y, cfg->width, cfg->height,
            hostClient.left, hostClient.top, hostClient.right, hostClient.bottom,
            rc.left, rc.top, width, height);
    }

    SetWindowPos(g_hwndOverlay, HWND_TOP, rc.left, rc.top,
        width, height,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
    PaintLyricOnOverlayDC(NULL);
}

static LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static BOOL s_loggedPaint = FALSE;

    switch (uMsg)
    {
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (hdc)
            {
                PaintLyricOnOverlayDC(hdc);
            }
            EndPaint(hwnd, &ps);
        }
        return 0;

    case WM_ERASEBKGND:
        // Keep the overlay transparent so the host taskbar/desktop paints behind it.
        return 1;

    case WM_LBUTTONDOWN:
        MessageBoxW(NULL, config_reader_get()->text_content, L"MostLyric", MB_OK);
        return 0;

    case WM_NCHITTEST:
        return HTTRANSPARENT;

    case ML_WM_PAINT_LYRIC:
        PaintLyricOnOverlayDC(NULL);
        return 0;
    }

    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

static LRESULT CALLBACK BandWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_ERASEBKGND:
        return 1;
    case WM_NCHITTEST:
        return HTTRANSPARENT;
    }

    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

static LRESULT CALLBACK BandSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
    UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    switch (uMsg)
    {
    case WM_PAINT:
    case WM_ERASEBKGND:
    case WM_LBUTTONDOWN:
    case WM_NCHITTEST:
        return BandWndProc(hwnd, uMsg, wParam, lParam);
    }

    return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

static LRESULT CALLBACK RebarSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
    UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    LRESULT result = DefSubclassProc(hwnd, uMsg, wParam, lParam);

    if (uMsg == WM_PAINT || uMsg == WM_PRINTCLIENT ||
        uMsg == WM_WINDOWPOSCHANGED || uMsg == WM_SIZE || uMsg == WM_MOVE)
    {
        PostMessageW(hwnd, ML_WM_PAINT_LYRIC, 0, 0);
    }
    else if (uMsg == ML_WM_PAINT_LYRIC)
    {
        PaintLyricOnOverlayDC(NULL);
        return 0;
    }

    return result;
}

static void InvalidateLyricRect(HWND hwndTaskbar)
{
    if (IsDesktopTarget())
    {
        PositionOverlayFromRebarRect();
        PaintLyricOnOverlayDC(NULL);
        return;
    }

    if (!hwndTaskbar || !g_hwndBand)
        return;

    if (g_useRebarBand && g_hwndRebar && IsWindow(g_hwndRebar))
    {
        UpdateRebarBandMetrics();
    }
    else
    {
        RECT rc = GetBandRect(hwndTaskbar);
        SetWindowPos(g_hwndBand, HWND_TOP, rc.left, rc.top,
            rc.right - rc.left, rc.bottom - rc.top,
            SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    RedrawWindow(g_hwndBand, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);
    if (g_useRebarBand && g_hwndRebar && IsWindow(g_hwndRebar))
    {
        RedrawWindow(g_hwndRebar, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
        PaintLyricOnOverlayDC(NULL);
    }
}

void RepositionBand(HWND hwnd, HWND hwndTaskbar)
{
    UNREFERENCED_PARAMETER(hwnd);
    InvalidateLyricRect(hwndTaskbar);
}

void pipe_on_stop(void)
{
    Log("STOP requested; posting quit to band thread=%lu\n", g_dwBandThreadId);
    g_selfUnloadRequested = TRUE;
    if (g_dwBandThreadId)
        PostThreadMessageW(g_dwBandThreadId, WM_QUIT, 0, 0);
}

LRESULT CALLBACK TaskbarSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
    UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    LRESULT result = DefSubclassProc(hwnd, uMsg, wParam, lParam);

    if (uMsg == WM_PAINT || uMsg == WM_PRINTCLIENT ||
        uMsg == WM_WINDOWPOSCHANGED || uMsg == WM_SIZE || uMsg == WM_MOVE ||
        uMsg == WM_SHOWWINDOW || uMsg == WM_ACTIVATE || uMsg == WM_ACTIVATEAPP)
    {
        RepositionBand(g_hwndBand, hwnd);
    }

    return result;
}

void CALLBACK RepositionTimer(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
    UNREFERENCED_PARAMETER(hwnd);
    UNREFERENCED_PARAMETER(uMsg);
    UNREFERENCED_PARAMETER(dwTime);

    if (!g_hwndTaskbar)
        return;

    if (idEvent == ML_TIMER_RENDER_FRAME)
        PaintLyricOnOverlayDC(NULL);
    else if (idEvent == ML_TIMER_REPOSITION)
        InvalidateLyricRect(g_hwndTaskbar);
}

DWORD WINAPI BandWindowThread(LPVOID lpParam)
{
    UNREFERENCED_PARAMETER(lpParam);
    g_dwBandThreadId = GetCurrentThreadId();
    Log("BandWindowThread started\n");

    HWND hwndTaskbar = NULL;
    for (int i = 0; i < 30; i++) {
        hwndTaskbar = FindWindowW(L"Shell_TrayWnd", NULL);
        if (hwndTaskbar) break;
        Sleep(1000);
    }
    if (!hwndTaskbar) {
        Log("Shell_TrayWnd not found\n");
        return 1;
    }

    g_hwndTaskbar = hwndTaskbar;
    Log("Found Shell_TrayWnd: %p\n", hwndTaskbar);

    config_reader_init();

    const ML_CONFIG* cfg = config_reader_get();
    BOOL desktopTarget = cfg->display_target == ML_DISPLAY_TARGET_DESKTOP;
    if (desktopTarget)
    {
        g_hwndWorkerW = FindDesktopWorkerW();
        Log("Using desktop WorkerW target\n");
    }
    else
    {
        ML_TASKBAR_HOSTS taskbarHosts = taskbar_host_find(hwndTaskbar);
        g_hwndRebar = taskbarHosts.rebar;
        g_hwndTaskSw = taskbarHosts.taskSwitch;
        g_hwndVisualHost = taskbarHosts.visualHost;
        if (g_hwndRebar && (!IsWindow(g_hwndRebar) || SendMessageW(g_hwndRebar, RB_GETBANDCOUNT, 0, 0) <= 0))
        {
            Log("Initial ReBar validation failed; using taskbar overlay fallback\n");
            g_hwndRebar = NULL;
        }
    }

    HWND hwndParent = desktopTarget ? NULL : (g_hwndRebar ? g_hwndRebar : hwndTaskbar);
    RECT bandRect = GetBandRect(hwndTaskbar);

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = BandWndProc;
    wc.hInstance = g_hInst;
    wc.lpszClassName = ML_BAND_CLASS_NAME;
    wc.hbrBackground = NULL;
    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        Log("RegisterClassW failed: %d\n", GetLastError());

    WNDCLASSW overlayWc = {0};
    overlayWc.lpfnWndProc = OverlayWndProc;
    overlayWc.hInstance = g_hInst;
    overlayWc.lpszClassName = ML_OVERLAY_CLASS_NAME;
    overlayWc.hbrBackground = NULL;
    if (!RegisterClassW(&overlayWc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        Log("Register overlay class failed: %d\n", GetLastError());

    // Keep the band in Explorer's taskbar hierarchy; the visible lyric is drawn by the overlay.
    g_hwndBand = CreateWindowExW(
        0,
        ML_BAND_CLASS_NAME,
        L"",
        desktopTarget ? WS_POPUP : (WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS),
        bandRect.left, bandRect.top, cfg->width, cfg->height,
        hwndParent,
        NULL,
        g_hInst,
        NULL
    );

    if (!g_hwndBand) {
        Log("Failed to create band child window: %d\n", GetLastError());
        return 1;
    }

    HWND hwndOverlayParent = desktopTarget ? g_hwndWorkerW : (g_hwndVisualHost ? g_hwndVisualHost : hwndTaskbar);
    g_hwndOverlay = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT,
        ML_OVERLAY_CLASS_NAME,
        L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0, 0, cfg->width, cfg->height,
        hwndOverlayParent,
        NULL,
        g_hInst,
        NULL
    );

    if (!g_hwndOverlay)
        Log("Failed to create overlay window: %d parent=%p\n", GetLastError(), hwndOverlayParent);
    else
    {
        ShowWindow(g_hwndOverlay, SW_SHOWNOACTIVATE);
        Log("Created overlay window: %p parent=%p\n", g_hwndOverlay, hwndOverlayParent);
    }

    if (!desktopTarget && g_hwndRebar && IsWindow(g_hwndRebar))
    {
        SetWindowSubclass(g_hwndRebar, RebarSubclassProc, 0, 0);
        LRESULT bandCount = SendMessageW(g_hwndRebar, RB_GETBANDCOUNT, 0, 0);
        if (bandCount <= 0 || bandCount > 32)
        {
            Log("Rebar unavailable for band insert: count=%Id; using overlay fallback\n", bandCount);
            RemoveWindowSubclass(g_hwndRebar, RebarSubclassProc, 0);
            g_hwndRebar = NULL;
        }
    }

    if (!desktopTarget && g_hwndRebar && IsWindow(g_hwndRebar))
    {
        taskbar_host_dump_rebar_bands(g_hwndRebar, "before insert");

        REBARBANDINFO rbi = {0};
        rbi.cbSize = sizeof(rbi);
        rbi.fMask = RBBIM_STYLE | RBBIM_CHILD | RBBIM_CHILDSIZE |
            RBBIM_SIZE | RBBIM_IDEALSIZE | RBBIM_ID;
        rbi.fStyle = RBBS_FIXEDSIZE | RBBS_NOGRIPPER | RBBS_TOPALIGN;
        rbi.hwndChild = g_hwndBand;
        rbi.cxMinChild = cfg->width;
        rbi.cyMinChild = cfg->height;
        rbi.cyChild = cfg->height;
        rbi.cyMaxChild = cfg->height;
        rbi.cx = cfg->width;
        rbi.cxIdeal = cfg->width;
        rbi.wID = ML_REBAR_BAND_ID;

        if (SendMessageW(g_hwndRebar, RB_INSERTBAND, 0, (LPARAM)&rbi))
        {
            g_useRebarBand = TRUE;
            UINT index = (UINT)SendMessageW(g_hwndRebar, RB_IDTOINDEX, ML_REBAR_BAND_ID, 0);
            RECT rcBand = {0};
            if (index != (UINT)-1)
            {
                SendMessageW(g_hwndRebar, RB_GETRECT, index, (LPARAM)&rcBand);
                g_rebarBandIndex = index;
                g_rebarBandRect = rcBand;
                PositionOverlayFromRebarRect();
            }
            Log("Inserted lyric rebar band index=%u rect=(%ld,%ld,%ld,%ld)\n",
                index, rcBand.left, rcBand.top, rcBand.right, rcBand.bottom);
            UpdateRebarBandMetrics();
            if (index != (UINT)-1)
            {
                SendMessageW(g_hwndRebar, RB_SHOWBAND, index, TRUE);
                MoveWindow(g_hwndBand, rcBand.left, rcBand.top,
                    rcBand.right - rcBand.left, rcBand.bottom - rcBand.top, TRUE);
            }
            taskbar_host_dump_rebar_bands(g_hwndRebar, "after insert");
            PaintLyricOnOverlayDC(NULL);
        }
        else
        {
            Log("RB_INSERTBAND failed: %d\n", GetLastError());
            RemoveWindowSubclass(g_hwndRebar, RebarSubclassProc, 0);
            g_hwndRebar = NULL;
            SetParent(g_hwndBand, hwndTaskbar);
        }
    }

    SetWindowSubclass(g_hwndBand, BandSubclassProc, 0, 0);
    if (!desktopTarget)
    {
        ShowWindow(g_hwndBand, SW_SHOWNOACTIVATE);
        UpdateWindow(g_hwndBand);
    }

    // Subclass the taskbar so we can reposition on resize.
    if (!desktopTarget)
        SetWindowSubclass(hwndTaskbar, TaskbarSubclassProc, 0, 0);

    Log("Created band child window: %p\n", g_hwndBand);
    lyric_engine_start(g_hwndOverlay ? g_hwndOverlay : g_hwndBand);
    RepositionBand(g_hwndBand, hwndTaskbar);
    PaintLyricOnOverlayDC(NULL);
    if (!desktopTarget)
    {
        RedrawWindow(g_hwndBand, NULL, NULL,
            RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE | RDW_ALLCHILDREN);
    }

    SetTimer(g_hwndBand, ML_TIMER_REPOSITION, ML_REPOSITION_INTERVAL_MS, RepositionTimer);
    SetTimer(g_hwndBand, ML_TIMER_RENDER_FRAME, ML_RENDER_INTERVAL_MS, RepositionTimer);

    g_hPipeThread = CreateThread(NULL, 0, PipeServerThread, NULL, 0, NULL);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    Cleanup();
    Log("BandWindowThread exiting\n");
    if (g_selfUnloadRequested)
        FreeLibraryAndExitThread(g_hInst, 0);
    return 0;
}

void Cleanup(void)
{
    if (g_cleanupDone)
        return;
    g_cleanupDone = TRUE;

    Log("Cleanup called\n");

    pipe_server_stop();
    lyric_engine_stop();

    if (g_hPipeThread) {
        WaitForSingleObject(g_hPipeThread, 2000);
        CloseHandle(g_hPipeThread);
        g_hPipeThread = NULL;
    }

    if (g_hwndBand) {
        KillTimer(g_hwndBand, ML_TIMER_REPOSITION);
        KillTimer(g_hwndBand, ML_TIMER_RENDER_FRAME);
        RemoveWindowSubclass(g_hwndBand, BandSubclassProc, 0);
        if (g_useRebarBand && g_hwndRebar && IsWindow(g_hwndRebar))
        {
            RemoveWindowSubclass(g_hwndRebar, RebarSubclassProc, 0);
            UINT index = (UINT)SendMessageW(g_hwndRebar, RB_IDTOINDEX, ML_REBAR_BAND_ID, 0);
            if (index != (UINT)-1)
                SendMessageW(g_hwndRebar, RB_DELETEBAND, index, 0);
        }
        DestroyWindow(g_hwndBand);
        g_hwndBand = NULL;
    }
    if (g_hwndOverlay) {
        DestroyWindow(g_hwndOverlay);
        g_hwndOverlay = NULL;
    }
    if (g_hwndTaskbar) {
        RemoveWindowSubclass(g_hwndTaskbar, TaskbarSubclassProc, 0);
        g_hwndTaskbar = NULL;
    }
    g_hwndRebar = NULL;
    g_useRebarBand = FALSE;
    g_hwndVisualHost = NULL;
    g_hwndWorkerW = NULL;
    g_hwndDesktopDefView = NULL;
    g_rebarBandIndex = (UINT)-1;
    SetRectEmpty(&g_rebarBandRect);
    karaoke_dcomp_shutdown();
    dwrite_renderer_shutdown();
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        g_hInst = hinstDLL;
        DisableThreadLibraryCalls(hinstDLL);

        WCHAR szName[MAX_PATH];
        GetModuleFileNameW(NULL, szName, MAX_PATH);
        _wcslwr_s(szName, MAX_PATH);
        if (wcsstr(szName, L"explorer.exe"))
        {
            hook_log_set_version(ML_HOOK_VERSION);
            Log("MostLyricHook.dll loaded in explorer.exe (%s)\n", ML_HOOK_VERSION);
            CreateThread(NULL, 0, BandWindowThread, NULL, 0, NULL);
        }
    }
    else if (fdwReason == DLL_PROCESS_DETACH)
    {
        WCHAR szName[MAX_PATH];
        GetModuleFileNameW(NULL, szName, MAX_PATH);
        _wcslwr_s(szName, MAX_PATH);
        if (wcsstr(szName, L"explorer.exe"))
        {
            Cleanup();
            Log("MostLyricHook.dll unloaded from explorer.exe\n");
        }
    }
    return TRUE;
}
