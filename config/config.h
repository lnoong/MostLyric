#ifndef MOSTLYRIC_CONFIG_H
#define MOSTLYRIC_CONFIG_H

#include <wchar.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _ML_CONFIG {
    int width;
    int height;
    int pos_x;
    int pos_y;
    int display_target;
    int text_source;
    int lyric_offset_ms;
    int highlight_r, highlight_g, highlight_b;
    int font_size;
    wchar_t font_name[64];
    int font_r, font_g, font_b;
    wchar_t text_content[256];
    wchar_t qqmusic_lyric_dir[260];
} ML_CONFIG;

#define ML_DISPLAY_TARGET_TASKBAR 0
#define ML_DISPLAY_TARGET_DESKTOP 1

#define ML_TEXT_SOURCE_MANUAL 0
#define ML_TEXT_SOURCE_QQMUSIC_LOCAL 1

#define ML_DEFAULT_WIDTH        520
#define ML_DEFAULT_HEIGHT       48
#define ML_DEFAULT_POS_X        -1
#define ML_DEFAULT_POS_Y        -1
#define ML_DEFAULT_DISPLAY_TARGET ML_DISPLAY_TARGET_TASKBAR
#define ML_DEFAULT_TEXT_SOURCE ML_TEXT_SOURCE_MANUAL
#define ML_DEFAULT_LYRIC_OFFSET_MS 350
#define ML_DEFAULT_HIGHLIGHT_R 255
#define ML_DEFAULT_HIGHLIGHT_G 255
#define ML_DEFAULT_HIGHLIGHT_B 255
#define ML_DEFAULT_FONT_SIZE    38
#define ML_DEFAULT_FONT_NAME    L"Segoe UI"
#define ML_DEFAULT_FONT_R       0
#define ML_DEFAULT_FONT_G       0
#define ML_DEFAULT_FONT_B       0
#define ML_DEFAULT_TEXT         L"Hello World"
#define ML_DEFAULT_QQMUSIC_LYRIC_DIR L""

#define ML_PIPE_NAME            L"\\\\.\\pipe\\MostLyricConfig"

static void ML_ConfigSetDefaults(ML_CONFIG* cfg)
{
    cfg->width       = ML_DEFAULT_WIDTH;
    cfg->height      = ML_DEFAULT_HEIGHT;
    cfg->pos_x       = ML_DEFAULT_POS_X;
    cfg->pos_y       = ML_DEFAULT_POS_Y;
    cfg->display_target = ML_DEFAULT_DISPLAY_TARGET;
    cfg->text_source = ML_DEFAULT_TEXT_SOURCE;
    cfg->lyric_offset_ms = ML_DEFAULT_LYRIC_OFFSET_MS;
    cfg->highlight_r = ML_DEFAULT_HIGHLIGHT_R;
    cfg->highlight_g = ML_DEFAULT_HIGHLIGHT_G;
    cfg->highlight_b = ML_DEFAULT_HIGHLIGHT_B;
    cfg->font_size   = ML_DEFAULT_FONT_SIZE;
    wcscpy_s(cfg->font_name, 64, ML_DEFAULT_FONT_NAME);
    cfg->font_r      = ML_DEFAULT_FONT_R;
    cfg->font_g      = ML_DEFAULT_FONT_G;
    cfg->font_b      = ML_DEFAULT_FONT_B;
    wcscpy_s(cfg->text_content, 256, ML_DEFAULT_TEXT);
    wcscpy_s(cfg->qqmusic_lyric_dir, 260, ML_DEFAULT_QQMUSIC_LYRIC_DIR);
}

#ifdef __cplusplus
}
#endif

#endif
