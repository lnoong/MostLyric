#include "hook_log.h"

#include <Windows.h>
#include <ShlObj.h>
#include <stdio.h>

#pragma comment(lib, "Shell32.lib")

static const char* g_version = "unknown";
static BOOL g_logReset = FALSE;

void hook_log_set_version(const char* version)
{
    g_version = version ? version : "unknown";
}

void Log(const char* format, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    PWSTR desktopPath = NULL;
    if (SHGetKnownFolderPath(&FOLDERID_Desktop, 0, NULL, &desktopPath) != S_OK)
        return;

    char logPath[MAX_PATH] = {0};
    WideCharToMultiByte(CP_ACP, 0, desktopPath, -1, logPath, MAX_PATH, NULL, NULL);
    strcat_s(logPath, MAX_PATH, "\\MostLyric.log");

    const char* mode = g_logReset ? "a" : "w";
    FILE* f = fopen(logPath, mode);
    if (f)
    {
        SYSTEMTIME st;
        GetLocalTime(&st);
        if (!g_logReset)
        {
            fprintf(f, "MostLyricHook version: %s\n", g_version);
            fprintf(f, "Process ID: %lu\n", GetCurrentProcessId());
            fprintf(f, "Log started: %02d/%02d/%04d %02d:%02d:%02d\n",
                st.wDay, st.wMonth, st.wYear,
                st.wHour, st.wMinute, st.wSecond);
            g_logReset = TRUE;
        }

        fprintf(f, "[%02d/%02d/%04d %02d:%02d:%02d] %s",
            st.wDay, st.wMonth, st.wYear,
            st.wHour, st.wMinute, st.wSecond,
            buffer);
        fclose(f);
    }

    CoTaskMemFree(desktopPath);
}
