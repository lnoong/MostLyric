#include "taskbar_host.h"
#include "hook_log.h"

#include <CommCtrl.h>

static BOOL CALLBACK DumpChildWindowProc(HWND hwnd, LPARAM lParam)
{
    ML_TASKBAR_HOSTS* hosts = (ML_TASKBAR_HOSTS*)lParam;
    WCHAR cls[128] = {0};
    WCHAR text[128] = {0};
    RECT rcWindow = {0};
    RECT rcClient = {0};

    GetClassNameW(hwnd, cls, ARRAYSIZE(cls));
    GetWindowTextW(hwnd, text, ARRAYSIZE(text));
    GetWindowRect(hwnd, &rcWindow);
    GetClientRect(hwnd, &rcClient);

    Log("  taskbar child hwnd=%p class=%ls visible=%d ex=0x%08lx style=0x%08lx win=(%ld,%ld,%ld,%ld) client=(%ld,%ld,%ld,%ld) text=%ls\n",
        hwnd, cls, IsWindowVisible(hwnd),
        GetWindowLongW(hwnd, GWL_EXSTYLE), GetWindowLongW(hwnd, GWL_STYLE),
        rcWindow.left, rcWindow.top, rcWindow.right, rcWindow.bottom,
        rcClient.left, rcClient.top, rcClient.right, rcClient.bottom, text);

    if (wcscmp(cls, L"MSTaskSwWClass") == 0)
        hosts->taskSwitch = hwnd;
    if (wcscmp(cls, L"Windows.UI.Composition.DesktopWindowContentBridge") == 0)
        hosts->visualHost = hwnd;

    return TRUE;
}

ML_TASKBAR_HOSTS taskbar_host_find(HWND hwndTaskbar)
{
    ML_TASKBAR_HOSTS hosts = {0};
    hosts.rebar = FindWindowExW(hwndTaskbar, NULL, L"ReBarWindow32", NULL);
    if (hosts.rebar)
        Log("Found ReBarWindow32: %p\n", hosts.rebar);
    else
        Log("ReBarWindow32 not found; falling back to Shell_TrayWnd child\n");

    Log("Taskbar child tree dump begin\n");
    EnumChildWindows(hwndTaskbar, DumpChildWindowProc, (LPARAM)&hosts);
    Log("Taskbar child tree dump end; MSTaskSwWClass=%p VisualHost=%p\n",
        hosts.taskSwitch, hosts.visualHost);
    return hosts;
}

void taskbar_host_dump_rebar_bands(HWND hwndRebar, const char* reason)
{
    if (!hwndRebar)
        return;

    int count = (int)SendMessageW(hwndRebar, RB_GETBANDCOUNT, 0, 0);
    Log("Rebar dump (%s): count=%d\n", reason, count);

    for (int i = 0; i < count; ++i)
    {
        WCHAR text[128] = {0};
        REBARBANDINFO rbi = {0};
        rbi.cbSize = sizeof(rbi);
        rbi.fMask = RBBIM_ID | RBBIM_STYLE | RBBIM_CHILD | RBBIM_SIZE |
            RBBIM_CHILDSIZE | RBBIM_TEXT;
        rbi.lpText = text;
        rbi.cch = ARRAYSIZE(text);

        RECT rc = {0};
        SendMessageW(hwndRebar, RB_GETBANDINFO, i, (LPARAM)&rbi);
        SendMessageW(hwndRebar, RB_GETRECT, i, (LPARAM)&rc);

        WCHAR cls[128] = {0};
        if (rbi.hwndChild)
            GetClassNameW(rbi.hwndChild, cls, ARRAYSIZE(cls));

        Log("  band[%d] id=%u style=0x%08x cx=%u child=%p class=%ls visible=%d rect=(%ld,%ld,%ld,%ld) text=%ls\n",
            i, rbi.wID, rbi.fStyle, rbi.cx, rbi.hwndChild, cls,
            rbi.hwndChild ? IsWindowVisible(rbi.hwndChild) : 0,
            rc.left, rc.top, rc.right, rc.bottom, text);
    }
}
