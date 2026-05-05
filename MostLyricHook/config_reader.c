#include "config_reader.h"
#include "hook_log.h"
#include <Windows.h>
#include <ShlObj.h>
#include <stdio.h>
#pragma comment(lib, "Shell32.lib")

static ML_CONFIG g_config;
static WCHAR g_iniPath[MAX_PATH] = {0};

static void EnsureIniPath(void)
{
    if (g_iniPath[0]) return;
    SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, g_iniPath);
    wcscat_s(g_iniPath, MAX_PATH, L"\\MostLyric");
    CreateDirectoryW(g_iniPath, NULL);
    wcscat_s(g_iniPath, MAX_PATH, L"\\config.ini");
}

void config_reader_init(void)
{
    ML_ConfigSetDefaults(&g_config);
    EnsureIniPath();

    g_config.width     = (int)GetPrivateProfileIntW(L"Window",  L"Width",  (INT)g_config.width,     g_iniPath);
    g_config.height    = (int)GetPrivateProfileIntW(L"Window",  L"Height", (INT)g_config.height,    g_iniPath);
    g_config.pos_x     = (int)GetPrivateProfileIntW(L"Window",  L"PosX",   (INT)g_config.pos_x,     g_iniPath);
    g_config.pos_y     = (int)GetPrivateProfileIntW(L"Window",  L"PosY",   (INT)g_config.pos_y,     g_iniPath);
    g_config.display_target = (int)GetPrivateProfileIntW(L"Window", L"DisplayTarget", (INT)g_config.display_target, g_iniPath);
    g_config.text_source = (int)GetPrivateProfileIntW(L"Lyrics", L"TextSource", (INT)g_config.text_source, g_iniPath);
    g_config.lyric_offset_ms = (int)GetPrivateProfileIntW(L"Lyrics", L"OffsetMs", (INT)g_config.lyric_offset_ms, g_iniPath);
    g_config.highlight_r = (int)GetPrivateProfileIntW(L"Lyrics", L"HighlightR", (INT)g_config.highlight_r, g_iniPath);
    g_config.highlight_g = (int)GetPrivateProfileIntW(L"Lyrics", L"HighlightG", (INT)g_config.highlight_g, g_iniPath);
    g_config.highlight_b = (int)GetPrivateProfileIntW(L"Lyrics", L"HighlightB", (INT)g_config.highlight_b, g_iniPath);
    g_config.font_size = (int)GetPrivateProfileIntW(L"Font", L"Size",       (INT)g_config.font_size, g_iniPath);
    g_config.font_r    = (int)GetPrivateProfileIntW(L"Font", L"ColorR",     (INT)g_config.font_r,    g_iniPath);
    g_config.font_g    = (int)GetPrivateProfileIntW(L"Font", L"ColorG",     (INT)g_config.font_g,    g_iniPath);
    g_config.font_b    = (int)GetPrivateProfileIntW(L"Font", L"ColorB",     (INT)g_config.font_b,    g_iniPath);

    GetPrivateProfileStringW(L"Font", L"Name", (LPCWSTR)g_config.font_name,
        g_config.font_name, 64, g_iniPath);
    GetPrivateProfileStringW(L"Text", L"Content", (LPCWSTR)g_config.text_content,
        g_config.text_content, 256, g_iniPath);
    GetPrivateProfileStringW(L"Lyrics", L"QQMusicLyricDir", (LPCWSTR)g_config.qqmusic_lyric_dir,
        g_config.qqmusic_lyric_dir, 260, g_iniPath);

    Log("Config loaded: %dx%d pos=(%d,%d) target=%d source=%d offset=%d font=%ls/%d color=(%d,%d,%d) highlight=(%d,%d,%d) text=%ls qrc_dir=%ls\n",
        g_config.width, g_config.height, g_config.pos_x, g_config.pos_y, g_config.display_target,
        g_config.text_source, g_config.lyric_offset_ms, g_config.font_name, g_config.font_size,
        g_config.font_r, g_config.font_g, g_config.font_b,
        g_config.highlight_r, g_config.highlight_g, g_config.highlight_b,
        g_config.text_content, g_config.qqmusic_lyric_dir);
}

const ML_CONFIG* config_reader_get(void)
{
    return &g_config;
}

void config_reader_update(const char* kv)
{
    if (!kv || !*kv) return;

    char key[64] = {0};
    char val[256] = {0};
    const char* eq = strchr(kv, '=');
    if (!eq) return;

    size_t klen = (size_t)(eq - kv);
    if (klen >= sizeof(key)) return;
    memcpy(key, kv, klen);
    strcpy_s(val, sizeof(val), eq + 1);

    if (strcmp(key, "Width") == 0)         g_config.width = atoi(val);
    else if (strcmp(key, "Height") == 0)   g_config.height = atoi(val);
    else if (strcmp(key, "PosX") == 0)     g_config.pos_x = atoi(val);
    else if (strcmp(key, "PosY") == 0)     g_config.pos_y = atoi(val);
    else if (strcmp(key, "DisplayTarget") == 0) g_config.display_target = atoi(val);
    else if (strcmp(key, "TextSource") == 0) g_config.text_source = atoi(val);
    else if (strcmp(key, "LyricOffsetMs") == 0) g_config.lyric_offset_ms = atoi(val);
    else if (strcmp(key, "HighlightR") == 0) g_config.highlight_r = atoi(val);
    else if (strcmp(key, "HighlightG") == 0) g_config.highlight_g = atoi(val);
    else if (strcmp(key, "HighlightB") == 0) g_config.highlight_b = atoi(val);
    else if (strcmp(key, "FontSize") == 0) g_config.font_size = atoi(val);
    else if (strcmp(key, "FontR") == 0)    g_config.font_r = atoi(val);
    else if (strcmp(key, "FontG") == 0)    g_config.font_g = atoi(val);
    else if (strcmp(key, "FontB") == 0)    g_config.font_b = atoi(val);
    else if (strcmp(key, "FontName") == 0)
    {
        WCHAR wval[64] = {0};
        MultiByteToWideChar(CP_UTF8, 0, val, -1, wval, 64);
        wcscpy_s(g_config.font_name, 64, wval);
    }
    else if (strcmp(key, "TextContent") == 0)
    {
        WCHAR wval[256] = {0};
        MultiByteToWideChar(CP_UTF8, 0, val, -1, wval, 256);
        wcscpy_s(g_config.text_content, 256, wval);
    }
    else if (strcmp(key, "QQMusicLyricDir") == 0)
    {
        WCHAR wval[260] = {0};
        MultiByteToWideChar(CP_UTF8, 0, val, -1, wval, 260);
        wcscpy_s(g_config.qqmusic_lyric_dir, 260, wval);
    }

    Log("Config updated: %s=%s\n", key, val);
}

void config_reader_commit(void)
{
    Log("Config committed, invalidating window\n");
}
