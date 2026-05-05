#ifndef PIPE_SERVER_H
#define PIPE_SERVER_H

#include <Windows.h>

DWORD WINAPI PipeServerThread(LPVOID lpParam);
void pipe_server_stop(void);

// Called by pipe server when COMMIT is received
extern void pipe_on_commit(void);

// Called by pipe server when STOP is received
extern void pipe_on_stop(void);

#endif
