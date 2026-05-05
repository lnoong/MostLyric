#include "i18n.h"
#include <Windows.h>
#include <string.h>

struct StringEntry {
    const wchar_t* en;
    const wchar_t* zh_cn;
};

static StringEntry g_strings[STR_COUNT] = {
    { L"MostLyric Settings",              L"MostLyric \x8BBE\x7F6E" },
    { L"Window Size",                     L"\x7A97\x53E3\x5927\x5C0F" },
    { L"Width",                           L"\x5BBD\x5EA6" },
    { L"Height",                          L"\x9AD8\x5EA6" },
    { L"Position",                        L"\x4F4D\x7F6E" },
    { L"X",                               L"X" },
    { L"Y",                               L"Y" },
    { L"Auto",                            L"\x81EA\x52A8" },
    { L"Display Target",                  L"\x663E\x793A\x76EE\x6807" },
    { L"Taskbar",                         L"\x4EFB\x52A1\x680F" },
    { L"Desktop (WorkerW)",               L"\x684C\x9762 (WorkerW)" },
    { L"Font Settings",                   L"\x5B57\x4F53\x8BBE\x7F6E" },
    { L"Font Size",                       L"\x5B57\x4F53\x5927\x5C0F" },
    { L"Font Name",                       L"\x5B57\x4F53\x540D\x79F0" },
    { L"Font Color",                      L"\x5B57\x4F53\x989C\x8272" },
    { L"Text Content",                    L"\x6587\x672C\x5185\x5BB9" },
    { L"Text Source",                     L"\x6587\x672C\x6765\x6E90" },
    { L"Manual Text",                     L"\x624B\x52A8\x6587\x672C" },
    { L"QQ Music Lyrics",                 L"QQ\x97F3\x4E50\x6B4C\x8BCD" },
    { L"Lyric Offset (ms)",               L"\x6B4C\x8BCD\x504F\x79FB (\x6BEB\x79D2)" },
    { L"Preview",                         L"\x9884\x89C8" },
    { L"Apply",                           L"\x5E94\x7528" },
    { L"Stop",                            L"\x5378\x8f7d" },
    { L"Language",                        L"\x8BED\x8A00" },
    { L"Status",                          L"\x72B6\x6001" },
    { L"Not injected",                    L"\x672A\x6CE8\x5165" },
    { L"Injected",                        L"\x5DF2\x6CE8\x5165" },
    { L"Injection failed (run as admin?)",L"\x6CE8\x5165\x5931\x8D25 (\x8BF7\x4EE5\x7BA1\x7406\x5458\x8FD0\x884C?)" },
    { L"Ejection failed",                L"\x5378\x8F7D\x5931\x8D25" },
    { L"Display Area",                    L"\x663E\x793A\x533A\x57DF" },
    { L"Lyric Source",                    L"\x6B4C\x8BCD\x6765\x6E90" },
    { L"Appearance & Colors",             L"\x5916\x89C2\x4E0E\x914D\x8272" },
    { L"Color Scheme",                    L"\x9ED8\x8BA4\x914D\x8272" },
    { L"Unsung Color",                    L"\x672A\x5531\x989C\x8272" },
    { L"Sung Color",                      L"\x5DF2\x5531\x989C\x8272" },
    { L"System",                          L"\x7CFB\x7EDF" },
    { L"QQ Music Lyric Folder",           L"QQ\x97F3\x4E50\x6B4C\x8BCD\x76EE\x5F55" },
    { L"Flow Blue",                       L"\x6D41\x5149\x84DD" },
    { L"Deep Sea",                        L"\x6DF1\x6D77\x9759\x8C27" },
    { L"Aurora Cyan",                     L"\x6781\x5149\x9752" },
    { L"Night Purple",                    L"\x591C\x5E55\x7D2B" },
    { L"Rhythm Orange",                   L"\x8282\x594F\x6A59" },
    { L"Forest Green",                    L"\x68EE\x6797\x7EFF" },
};

static ML_Lang g_lang = ML_LANG_EN;
static char g_utf8Buf[STR_COUNT][256] = {};
static bool g_utf8Cached[STR_COUNT] = {};

void i18n_init(ML_Lang lang)
{
    g_lang = lang;
    memset(g_utf8Cached, 0, sizeof(g_utf8Cached));
}

const wchar_t* i18n_get(int string_id)
{
    if (string_id < 0 || string_id >= STR_COUNT) return L"???";
    switch (g_lang) {
        case ML_LANG_ZH_CN: return g_strings[string_id].zh_cn;
        default:            return g_strings[string_id].en;
    }
}

const char* i18n_get_utf8(int string_id)
{
    if (string_id < 0 || string_id >= STR_COUNT) return "???";
    if (!g_utf8Cached[string_id]) {
        WideCharToMultiByte(CP_UTF8, 0, i18n_get(string_id), -1,
            g_utf8Buf[string_id], 256, NULL, NULL);
        g_utf8Cached[string_id] = true;
    }
    return g_utf8Buf[string_id];
}
