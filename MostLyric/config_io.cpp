#include "config_io.h"
#include <ShlObj.h>
#include <cstdio>
#pragma comment(lib, "Shell32.lib")

void config_get_ini_path(WCHAR* path, SIZE_T cchPath)
{
    SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, path);
    wcscat_s(path, cchPath, L"\\MostLyric");
    CreateDirectoryW(path, NULL);
    wcscat_s(path, cchPath, L"\\config.ini");
}

void config_load(ML_CONFIG* cfg)
{
    ML_ConfigSetDefaults(cfg);

    WCHAR iniPath[MAX_PATH];
    config_get_ini_path(iniPath, MAX_PATH);

    cfg->width     = (int)GetPrivateProfileIntW(L"Window", L"Width",  (INT)cfg->width,  iniPath);
    cfg->height    = (int)GetPrivateProfileIntW(L"Window", L"Height", (INT)cfg->height, iniPath);
    cfg->pos_x     = (int)GetPrivateProfileIntW(L"Window", L"PosX",  (INT)cfg->pos_x,  iniPath);
    cfg->pos_y     = (int)GetPrivateProfileIntW(L"Window", L"PosY",  (INT)cfg->pos_y,  iniPath);
    cfg->display_target = (int)GetPrivateProfileIntW(L"Window", L"DisplayTarget", (INT)cfg->display_target, iniPath);
    cfg->text_source = (int)GetPrivateProfileIntW(L"Lyrics", L"TextSource", (INT)cfg->text_source, iniPath);
    cfg->lyric_offset_ms = (int)GetPrivateProfileIntW(L"Lyrics", L"OffsetMs", (INT)cfg->lyric_offset_ms, iniPath);
    cfg->highlight_r = (int)GetPrivateProfileIntW(L"Lyrics", L"HighlightR", (INT)cfg->highlight_r, iniPath);
    cfg->highlight_g = (int)GetPrivateProfileIntW(L"Lyrics", L"HighlightG", (INT)cfg->highlight_g, iniPath);
    cfg->highlight_b = (int)GetPrivateProfileIntW(L"Lyrics", L"HighlightB", (INT)cfg->highlight_b, iniPath);
    cfg->font_size = (int)GetPrivateProfileIntW(L"Font", L"Size",     (INT)cfg->font_size, iniPath);
    cfg->font_r    = (int)GetPrivateProfileIntW(L"Font", L"ColorR",   (INT)cfg->font_r,   iniPath);
    cfg->font_g    = (int)GetPrivateProfileIntW(L"Font", L"ColorG",   (INT)cfg->font_g,   iniPath);
    cfg->font_b    = (int)GetPrivateProfileIntW(L"Font", L"ColorB",   (INT)cfg->font_b,   iniPath);

    GetPrivateProfileStringW(L"Font", L"Name", (LPCWSTR)cfg->font_name,
        cfg->font_name, 64, iniPath);
    GetPrivateProfileStringW(L"Text", L"Content", (LPCWSTR)cfg->text_content,
        cfg->text_content, 256, iniPath);
    GetPrivateProfileStringW(L"Lyrics", L"QQMusicLyricDir", (LPCWSTR)cfg->qqmusic_lyric_dir,
        cfg->qqmusic_lyric_dir, 260, iniPath);
}

static void WriteInt(LPCWSTR section, LPCWSTR key, int val, const WCHAR* iniPath)
{
    WCHAR buf[16];
    _itow_s(val, buf, 10);
    WritePrivateProfileStringW(section, key, buf, iniPath);
}

void config_save(const ML_CONFIG* cfg)
{
    WCHAR iniPath[MAX_PATH];
    config_get_ini_path(iniPath, MAX_PATH);

    WriteInt(L"Window", L"Width",  cfg->width,  iniPath);
    WriteInt(L"Window", L"Height", cfg->height, iniPath);
    WriteInt(L"Window", L"PosX",   cfg->pos_x,  iniPath);
    WriteInt(L"Window", L"PosY",   cfg->pos_y,  iniPath);
    WriteInt(L"Window", L"DisplayTarget", cfg->display_target, iniPath);
    WriteInt(L"Lyrics", L"TextSource", cfg->text_source, iniPath);
    WriteInt(L"Lyrics", L"OffsetMs", cfg->lyric_offset_ms, iniPath);
    WriteInt(L"Lyrics", L"HighlightR", cfg->highlight_r, iniPath);
    WriteInt(L"Lyrics", L"HighlightG", cfg->highlight_g, iniPath);
    WriteInt(L"Lyrics", L"HighlightB", cfg->highlight_b, iniPath);
    WritePrivateProfileStringW(L"Lyrics", L"QQMusicLyricDir", cfg->qqmusic_lyric_dir, iniPath);
    WriteInt(L"Font", L"Size",     cfg->font_size, iniPath);
    WritePrivateProfileStringW(L"Font", L"Name", cfg->font_name, iniPath);
    WriteInt(L"Font", L"ColorR",   cfg->font_r, iniPath);
    WriteInt(L"Font", L"ColorG",   cfg->font_g, iniPath);
    WriteInt(L"Font", L"ColorB",   cfg->font_b, iniPath);
    WritePrivateProfileStringW(L"Text", L"Content", cfg->text_content, iniPath);
}

BOOL pipe_send_config(const ML_CONFIG* cfg)
{
    HANDLE hPipe = CreateFileW(
        ML_PIPE_NAME,
        GENERIC_READ | GENERIC_WRITE,
        0, NULL, OPEN_EXISTING, 0, NULL);

    if (hPipe == INVALID_HANDLE_VALUE)
        return FALSE;

    char msg[4096] = {0};
    int offset = 0;

    char fontNameUtf8[256] = {};
    WideCharToMultiByte(CP_UTF8, 0, cfg->font_name, -1, fontNameUtf8, sizeof(fontNameUtf8), NULL, NULL);

    char textUtf8[1024] = {};
    WideCharToMultiByte(CP_UTF8, 0, cfg->text_content, -1, textUtf8, sizeof(textUtf8), NULL, NULL);

    char lyricDirUtf8[1024] = {};
    WideCharToMultiByte(CP_UTF8, 0, cfg->qqmusic_lyric_dir, -1, lyricDirUtf8, sizeof(lyricDirUtf8), NULL, NULL);

    offset += snprintf(msg + offset, sizeof(msg) - offset,
        "Width=%d\nHeight=%d\nPosX=%d\nPosY=%d\nDisplayTarget=%d\nTextSource=%d\nLyricOffsetMs=%d\nHighlightR=%d\nHighlightG=%d\nHighlightB=%d\nFontSize=%d\nFontName=%s\nFontR=%d\nFontG=%d\nFontB=%d\nTextContent=%s\nQQMusicLyricDir=%s\nCOMMIT\n",
        cfg->width, cfg->height, cfg->pos_x, cfg->pos_y, cfg->display_target,
        cfg->text_source, cfg->lyric_offset_ms, cfg->highlight_r, cfg->highlight_g, cfg->highlight_b,
        cfg->font_size, fontNameUtf8, cfg->font_r, cfg->font_g, cfg->font_b, textUtf8, lyricDirUtf8);

    DWORD written = 0;
    WriteFile(hPipe, msg, (DWORD)offset, &written, NULL);

    char resp[16] = {0};
    DWORD read = 0;
    ReadFile(hPipe, resp, sizeof(resp) - 1, &read, NULL);

    CloseHandle(hPipe);
    return read > 0;
}

BOOL pipe_send_stop(void)
{
    HANDLE hPipe = CreateFileW(
        ML_PIPE_NAME,
        GENERIC_READ | GENERIC_WRITE,
        0, NULL, OPEN_EXISTING, 0, NULL);

    if (hPipe == INVALID_HANDLE_VALUE)
        return FALSE;

    const char* msg = "STOP\n";
    DWORD written = 0;
    if (!WriteFile(hPipe, msg, (DWORD)strlen(msg), &written, NULL))
    {
        CloseHandle(hPipe);
        return FALSE;
    }

    char resp[16] = {0};
    DWORD read = 0;
    ReadFile(hPipe, resp, sizeof(resp) - 1, &read, NULL);

    CloseHandle(hPipe);
    return read > 0;
}
