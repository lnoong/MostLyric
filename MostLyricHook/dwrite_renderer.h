#ifndef DWRITE_RENDERER_H
#define DWRITE_RENDERER_H

#include <Windows.h>
#include "../config/config.h"
#include "lyric_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

BOOL dwrite_render_overlay(HWND hwndOverlay, int width, int height, const ML_CONFIG* cfg);
BOOL dwrite_render_overlay_frame(HWND hwndOverlay, int width, int height, const ML_CONFIG* cfg, const ML_LYRIC_RENDER_FRAME* frame);
void dwrite_renderer_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
