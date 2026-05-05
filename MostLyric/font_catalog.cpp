#include "font_catalog.h"

#include <string>
#include <vector>

static std::vector<std::wstring> g_fontNamesWide;
static std::vector<std::string> g_fontNamesUtf8;
static std::string g_fontComboItems;
static int g_selectedFontIndex = 0;
static ImVector<ImWchar> g_cjkRanges;

static int CALLBACK EnumFontFamExProc(const LOGFONTW* lf, const TEXTMETRICW*, DWORD, LPARAM lParam)
{
    auto* fonts = (std::vector<std::wstring>*)lParam;
    if (lf->lfFaceName[0] == L'@' || lf->lfPitchAndFamily == 0)
        return 1;

    std::wstring name(lf->lfFaceName);
    for (const auto& existing : *fonts)
    {
        if (_wcsicmp(existing.c_str(), name.c_str()) == 0)
            return 1;
    }

    fonts->push_back(name);
    return 1;
}

static void EnumerateSystemFonts()
{
    g_fontNamesWide.clear();
    g_fontNamesUtf8.clear();

    HDC hdc = GetDC(NULL);
    LOGFONTW lf = {};
    lf.lfCharSet = DEFAULT_CHARSET;
    EnumFontFamiliesExW(hdc, &lf, EnumFontFamExProc, (LPARAM)&g_fontNamesWide, 0);
    ReleaseDC(NULL, hdc);

    for (const auto& name : g_fontNamesWide)
    {
        char buf[256] = {};
        WideCharToMultiByte(CP_UTF8, 0, name.c_str(), -1, buf, sizeof(buf), NULL, NULL);
        g_fontNamesUtf8.push_back(buf);
    }
}

static void BuildFontComboItems()
{
    g_fontComboItems.clear();
    for (const auto& name : g_fontNamesUtf8)
    {
        g_fontComboItems += name;
        g_fontComboItems += '\0';
    }
    g_fontComboItems += '\0';
}

static void SelectFontByName(const wchar_t* selectedFontName)
{
    g_selectedFontIndex = 0;
    if (!selectedFontName)
        return;

    for (int i = 0; i < (int)g_fontNamesWide.size(); ++i)
    {
        if (_wcsicmp(g_fontNamesWide[i].c_str(), selectedFontName) == 0)
        {
            g_selectedFontIndex = i;
            return;
        }
    }
}

static void BuildCjkRanges()
{
    g_cjkRanges.clear();
    g_cjkRanges.push_back(0x4E00); g_cjkRanges.push_back(0x9FFF);
    g_cjkRanges.push_back(0xF900); g_cjkRanges.push_back(0xFAFF);
    g_cjkRanges.push_back(0x3000); g_cjkRanges.push_back(0x303F);
    g_cjkRanges.push_back(0x3040); g_cjkRanges.push_back(0x309F);
    g_cjkRanges.push_back(0x30A0); g_cjkRanges.push_back(0x30FF);
    g_cjkRanges.push_back(0xFF00); g_cjkRanges.push_back(0xFFEF);
    g_cjkRanges.push_back(0x2E80); g_cjkRanges.push_back(0x2EFF);
    g_cjkRanges.push_back(0x2F00); g_cjkRanges.push_back(0x2FDF);
    g_cjkRanges.push_back(0x31C0); g_cjkRanges.push_back(0x31EF);
    g_cjkRanges.push_back(0x3100); g_cjkRanges.push_back(0x312F);
    g_cjkRanges.push_back(0x31A0); g_cjkRanges.push_back(0x31BF);
    g_cjkRanges.push_back(0);
}

void font_catalog_init(const wchar_t* selectedFontName)
{
    EnumerateSystemFonts();
    SelectFontByName(selectedFontName);
    BuildFontComboItems();
    BuildCjkRanges();
}

const char* font_catalog_combo_items()
{
    return g_fontComboItems.c_str();
}

int* font_catalog_selected_index()
{
    return &g_selectedFontIndex;
}

const wchar_t* font_catalog_selected_font_name()
{
    if (g_selectedFontIndex < 0 || g_selectedFontIndex >= (int)g_fontNamesWide.size())
        return NULL;
    return g_fontNamesWide[g_selectedFontIndex].c_str();
}

bool font_catalog_load_imgui_font(ImGuiIO& io, float fontSize)
{
    ImFontConfig fontCfg;
    fontCfg.OversampleH = 2;
    fontCfg.OversampleV = 1;

    const WCHAR* cjkFonts[] = {
        L"C:\\Windows\\Fonts\\msyh.ttc",
        L"C:\\Windows\\Fonts\\simhei.ttf",
        L"C:\\Windows\\Fonts\\simsun.ttc",
    };

    for (auto path : cjkFonts)
    {
        if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES)
            continue;

        char pathUtf8[MAX_PATH] = {};
        WideCharToMultiByte(CP_UTF8, 0, path, -1, pathUtf8, MAX_PATH, NULL, NULL);
        if (io.Fonts->AddFontFromFileTTF(pathUtf8, fontSize, &fontCfg, g_cjkRanges.Data))
            return true;
    }

    io.Fonts->AddFontDefault();
    for (auto path : cjkFonts)
    {
        if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES)
            continue;

        char pathUtf8[MAX_PATH] = {};
        WideCharToMultiByte(CP_UTF8, 0, path, -1, pathUtf8, MAX_PATH, NULL, NULL);
        ImFontConfig mergeCfg;
        mergeCfg.MergeMode = true;
        io.Fonts->AddFontFromFileTTF(pathUtf8, fontSize, &mergeCfg, g_cjkRanges.Data);
        break;
    }
    return false;
}
