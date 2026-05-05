#ifndef HOOK_LOG_H
#define HOOK_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

void hook_log_set_version(const char* version);
void Log(const char* format, ...);

#ifdef __cplusplus
}
#endif

#endif
