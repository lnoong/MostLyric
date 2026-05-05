#ifndef TASKBAR_HOST_H
#define TASKBAR_HOST_H

#include <Windows.h>

typedef struct _ML_TASKBAR_HOSTS {
    HWND rebar;
    HWND taskSwitch;
    HWND visualHost;
} ML_TASKBAR_HOSTS;

ML_TASKBAR_HOSTS taskbar_host_find(HWND hwndTaskbar);
void taskbar_host_dump_rebar_bands(HWND hwndRebar, const char* reason);

#endif
