#include "pipe_server.h"
#include "config_reader.h"
#include "lyric_engine.h"
#include "hook_log.h"
#include "../config/config.h"
#include <stdio.h>

extern HWND g_hwndBand;

static BOOL g_bPipeRunning = TRUE;

void pipe_on_commit(void)
{
    if (g_hwndBand)
    {
        // Reposition with new size
        extern HWND g_hwndTaskbar;
        extern void RepositionBand(HWND, HWND);
        lyric_engine_on_config_changed();
        RepositionBand(g_hwndBand, g_hwndTaskbar);

        InvalidateRect(g_hwndBand, NULL, TRUE);
    }
}

DWORD WINAPI PipeServerThread(LPVOID lpParam)
{
    Log("Pipe server thread started\n");

    while (g_bPipeRunning)
    {
        HANDLE hPipe = CreateNamedPipeW(
            ML_PIPE_NAME,
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            1, 4096, 4096, 0, NULL);

        if (hPipe == INVALID_HANDLE_VALUE)
        {
            Log("CreateNamedPipe failed: %d\n", GetLastError());
            Sleep(1000);
            continue;
        }

        Log("Pipe created, waiting for connection...\n");

        if (!ConnectNamedPipe(hPipe, NULL) && GetLastError() != ERROR_PIPE_CONNECTED)
        {
            CloseHandle(hPipe);
            if (!g_bPipeRunning) break;
            Sleep(100);
            continue;
        }

        Log("Pipe client connected\n");

        char buf[4096] = {0};
        DWORD totalRead = 0;

        while (g_bPipeRunning)
        {
            DWORD bytesRead = 0;
            BOOL ok = ReadFile(hPipe, buf + totalRead, (DWORD)(sizeof(buf) - totalRead - 1), &bytesRead, NULL);
            if (!ok || bytesRead == 0) break;

            totalRead += bytesRead;
            buf[totalRead] = '\0';

            // Process complete lines
            char* line = buf;
            while (g_bPipeRunning)
            {
                char* nl = strchr(line, '\n');
                if (!nl) break;

                *nl = '\0';
                // Trim \r
                if (nl > line && *(nl - 1) == '\r')
                    *(nl - 1) = '\0';

                if (strcmp(line, "COMMIT") == 0)
                {
                    pipe_on_commit();
                    DWORD written = 0;
                    const char* resp = "OK\n";
                    WriteFile(hPipe, resp, (DWORD)strlen(resp), &written, NULL);
                    FlushFileBuffers(hPipe);
                }
                else if (strcmp(line, "STOP") == 0)
                {
                    Log("Pipe STOP received\n");
                    pipe_on_stop();
                    DWORD written = 0;
                    const char* resp = "OK\n";
                    WriteFile(hPipe, resp, (DWORD)strlen(resp), &written, NULL);
                    FlushFileBuffers(hPipe);
                    g_bPipeRunning = FALSE;
                    break;
                }
                else
                {
                    config_reader_update(line);
                }

                line = nl + 1;
            }

            // Move leftover to front of buffer
            size_t remaining = buf + totalRead - line;
            if (remaining > 0 && line != buf)
                memmove(buf, line, remaining);
            totalRead = (DWORD)remaining;
            buf[totalRead] = '\0';
        }

        FlushFileBuffers(hPipe);
        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);

        Log("Pipe client disconnected\n");
    }

    Log("Pipe server thread exiting\n");
    return 0;
}

void pipe_server_stop(void)
{
    g_bPipeRunning = FALSE;

    // Connect to our own pipe to unblock ConnectNamedPipe
    HANDLE h = CreateFileW(ML_PIPE_NAME, GENERIC_READ | GENERIC_WRITE,
        0, NULL, OPEN_EXISTING, 0, NULL);
    if (h != INVALID_HANDLE_VALUE)
    {
        // Send a dummy commit so the server loop can exit cleanly
        const char* dummy = "COMMIT\n";
        DWORD written;
        WriteFile(h, dummy, (DWORD)strlen(dummy), &written, NULL);
        CloseHandle(h);
    }
}
