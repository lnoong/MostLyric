#ifndef DESKTOP_HOST_H
#define DESKTOP_HOST_H

#include <Windows.h>

typedef struct _ML_DESKTOP_HOST {
    HWND host;
    HWND workerW;
    HWND defView;
    HWND progman;
} ML_DESKTOP_HOST;

ML_DESKTOP_HOST desktop_host_find(void);

#endif
