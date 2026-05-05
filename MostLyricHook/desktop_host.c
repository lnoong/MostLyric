#include "desktop_host.h"
#include "hook_log.h"

typedef struct _ML_DESKTOP_HOSTS {
    HWND defViewParent;
    HWND defView;
    HWND workerAfterDefViewParent;
} ML_DESKTOP_HOSTS;

static BOOL CALLBACK FindWorkerWProc(HWND hwnd, LPARAM lParam)
{
    HWND shellView = FindWindowExW(hwnd, NULL, L"SHELLDLL_DefView", NULL);
    if (shellView)
    {
        ML_DESKTOP_HOSTS* hosts = (ML_DESKTOP_HOSTS*)lParam;
        hosts->defViewParent = hwnd;
        hosts->defView = shellView;
        hosts->workerAfterDefViewParent = FindWindowExW(NULL, hwnd, L"WorkerW", NULL);
        return FALSE;
    }
    return TRUE;
}

static void DumpDesktopHost(HWND hwnd, const char* name)
{
    if (!hwnd)
    {
        Log("Desktop host %s: NULL\n", name);
        return;
    }

    WCHAR cls[128] = {0};
    RECT rcWindow = {0};
    RECT rcClient = {0};
    GetClassNameW(hwnd, cls, ARRAYSIZE(cls));
    GetWindowRect(hwnd, &rcWindow);
    GetClientRect(hwnd, &rcClient);
    Log("Desktop host %s: hwnd=%p class=%ls visible=%d win=(%ld,%ld,%ld,%ld) client=(%ld,%ld,%ld,%ld)\n",
        name, hwnd, cls, IsWindowVisible(hwnd),
        rcWindow.left, rcWindow.top, rcWindow.right, rcWindow.bottom,
        rcClient.left, rcClient.top, rcClient.right, rcClient.bottom);
}

ML_DESKTOP_HOST desktop_host_find(void)
{
    ML_DESKTOP_HOST result = {0};
    result.progman = FindWindowW(L"Progman", NULL);

    if (result.progman)
    {
        DWORD_PTR unused = 0;
        SendMessageTimeoutW(result.progman, 0x052C, 0xD, 0,
            SMTO_NORMAL, 1000, &unused);
        SendMessageTimeoutW(result.progman, 0x052C, 0xD, 1,
            SMTO_NORMAL, 1000, &unused);
        SendMessageTimeoutW(result.progman, 0x052C, 0, 0,
            SMTO_NORMAL, 1000, &unused);
        Sleep(200);
    }

    ML_DESKTOP_HOSTS hosts = {0};
    EnumWindows(FindWorkerWProc, (LPARAM)&hosts);

    DumpDesktopHost(result.progman, "Progman");
    DumpDesktopHost(hosts.defViewParent, "DefViewParent");
    DumpDesktopHost(hosts.defView, "SHELLDLL_DefView");
    DumpDesktopHost(hosts.workerAfterDefViewParent, "WorkerWAfterDefViewParent");

    result.workerW = hosts.workerAfterDefViewParent;
    result.defView = hosts.defView;
    result.host = result.workerW ? result.workerW : result.defView;
    if (!result.host)
        result.host = result.progman;

    if (result.workerW)
        ShowWindow(result.workerW, SW_SHOWNOACTIVATE);

    Log("Selected desktop host: %p worker=%p defView=%p progman=%p\n",
        result.host, result.workerW, result.defView, result.progman);
    return result;
}
