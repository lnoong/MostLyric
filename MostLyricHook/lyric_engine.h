#ifndef LYRIC_ENGINE_H
#define LYRIC_ENGINE_H

#include <Windows.h>
#include "../config/config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _ML_LYRIC_RENDER_FRAME {
    BOOL active;
    wchar_t text[256];
    UINT32 highlight_chars;
    UINT32 highlight_end_chars;
    UINT32 highlight_progress;
} ML_LYRIC_RENDER_FRAME;

void lyric_engine_start(HWND hwndNotify);
void lyric_engine_stop(void);
void lyric_engine_on_config_changed(void);
BOOL lyric_engine_get_frame(ML_LYRIC_RENDER_FRAME* frame);

#ifdef __cplusplus
}
#endif

#endif
