#include "app_state.h"
#include "injector.h"
#include "config_io.h"
#include "preview.h"
#include "i18n.h"
#include "font_catalog.h"
#include "resource.h"
#include "../config/config.h"

#include <d3d11.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <tchar.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")

ML_CONFIG g_config;
bool g_injected = false;
int g_language = 0;
int g_lastAppliedTarget = ML_DEFAULT_DISPLAY_TARGET;

static ID3D11Device*            g_pd3dDevice = NULL;
static ID3D11DeviceContext*     g_pd3dDeviceContext = NULL;
static IDXGISwapChain*          g_pSwapChain = NULL;
static ID3D11RenderTargetView*  g_mainRenderTargetView = NULL;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags,
        featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (FAILED(hr))
        return false;

    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
    pBackBuffer->Release();
    return true;
}

static void CleanupDeviceD3D()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
}

static void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer = NULL;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
        {
            if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static char g_textContentBuf[1024] = {};
static char g_qqMusicLyricDirBuf[MAX_PATH * 3] = {};

struct ColorScheme {
    int nameStringId;
    int normalR, normalG, normalB;
    int highlightR, highlightG, highlightB;
};

static const ColorScheme g_colorSchemes[] = {
    { STR_SCHEME_FLOW_BLUE,     0x7F, 0x95, 0xB3, 0x2F, 0x80, 0xFF },
    { STR_SCHEME_DEEP_SEA,      0x8A, 0x97, 0xA8, 0x1F, 0x3A, 0x5F },
    { STR_SCHEME_AURORA_CYAN,   0x88, 0x99, 0xAA, 0x00, 0xA8, 0xE8 },
    { STR_SCHEME_NIGHT_PURPLE,  0x8B, 0x93, 0xA6, 0x7B, 0x61, 0xFF },
    { STR_SCHEME_RHYTHM_ORANGE, 0x8F, 0x98, 0xA8, 0xFF, 0x8C, 0x42 },
    { STR_SCHEME_FOREST_GREEN,  0x7C, 0x90, 0xA8, 0x2F, 0xBF, 0x71 },
};

static void ApplyColorScheme(const ColorScheme& scheme)
{
    g_config.font_r = scheme.normalR;
    g_config.font_g = scheme.normalG;
    g_config.font_b = scheme.normalB;
    g_config.highlight_r = scheme.highlightR;
    g_config.highlight_g = scheme.highlightG;
    g_config.highlight_b = scheme.highlightB;
}

static void SyncConfigToUtf8()
{
    WideCharToMultiByte(CP_UTF8, 0, g_config.text_content, -1, g_textContentBuf, sizeof(g_textContentBuf), NULL, NULL);
    WideCharToMultiByte(CP_UTF8, 0, g_config.qqmusic_lyric_dir, -1, g_qqMusicLyricDirBuf, sizeof(g_qqMusicLyricDirBuf), NULL, NULL);
}

static void SyncUtf8ToConfig()
{
    MultiByteToWideChar(CP_UTF8, 0, g_textContentBuf, -1, g_config.text_content, 256);
    MultiByteToWideChar(CP_UTF8, 0, g_qqMusicLyricDirBuf, -1, g_config.qqmusic_lyric_dir, 260);
}

static void ApplyWin11Style()
{
    ImGuiStyle& style = ImGui::GetStyle();

    // Windows 11 light settings-style palette.
    ImVec4 bg = ImVec4(0.953f, 0.957f, 0.973f, 1.0f);          // #F3F4F8
    ImVec4 surface = ImVec4(1.000f, 1.000f, 1.000f, 1.0f);     // #FFFFFF
    ImVec4 surfaceAlt = ImVec4(0.980f, 0.984f, 0.992f, 1.0f);  // #FAFBFD
    ImVec4 hover = ImVec4(0.925f, 0.941f, 0.965f, 1.0f);       // #ECF0F6
    ImVec4 active = ImVec4(0.886f, 0.918f, 0.957f, 1.0f);      // #E2EAF4
    ImVec4 accent = ImVec4(0.000f, 0.471f, 0.843f, 1.0f);      // #0078D4
    ImVec4 accentHover = ImVec4(0.063f, 0.416f, 0.722f, 1.0f); // #106AB8
    ImVec4 text = ImVec4(0.125f, 0.129f, 0.145f, 1.0f);        // #202125
    ImVec4 textDim = ImVec4(0.376f, 0.392f, 0.431f, 1.0f);     // #60646E
    ImVec4 border = ImVec4(0.800f, 0.820f, 0.855f, 0.9f);      // #CCD1DA

    style.Colors[ImGuiCol_WindowBg] = bg;
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0,0,0,0);
    style.Colors[ImGuiCol_Border] = border;
    style.Colors[ImGuiCol_FrameBg] = surface;
    style.Colors[ImGuiCol_FrameBgHovered] = hover;
    style.Colors[ImGuiCol_FrameBgActive] = active;
    style.Colors[ImGuiCol_TitleBg] = bg;
    style.Colors[ImGuiCol_TitleBgActive] = bg;
    style.Colors[ImGuiCol_Button] = surface;
    style.Colors[ImGuiCol_ButtonHovered] = hover;
    style.Colors[ImGuiCol_ButtonActive] = active;
    style.Colors[ImGuiCol_Header] = surfaceAlt;
    style.Colors[ImGuiCol_HeaderHovered] = hover;
    style.Colors[ImGuiCol_HeaderActive] = active;
    style.Colors[ImGuiCol_Separator] = border;
    style.Colors[ImGuiCol_SliderGrab] = accent;
    style.Colors[ImGuiCol_SliderGrabActive] = accentHover;
    style.Colors[ImGuiCol_CheckMark] = accent;
    style.Colors[ImGuiCol_Text] = text;
    style.Colors[ImGuiCol_TextDisabled] = textDim;
    style.Colors[ImGuiCol_PopupBg] = surface;
    style.Colors[ImGuiCol_MenuBarBg] = surface;
    style.Colors[ImGuiCol_ScrollbarBg] = bg;
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.745f, 0.765f, 0.804f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.650f, 0.675f, 0.720f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.560f, 0.590f, 0.635f, 1.0f);
    style.Colors[ImGuiCol_NavHighlight] = accent;
    style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0,0,0,0.25f);

    style.WindowRounding = 10.0f;
    style.FrameRounding = 6.0f;
    style.GrabRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.PopupRounding = 6.0f;
    style.ChildRounding = 6.0f;

    style.WindowPadding = ImVec2(16, 12);
    style.FramePadding = ImVec2(9, 6);
    style.ItemSpacing = ImVec2(9, 7);
    style.ItemInnerSpacing = ImVec2(6, 4);
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
}

static void BuildUI()
{
    // Desktop rendering is kept in the Hook, but hidden from the current settings UI.
    if (g_config.display_target != ML_DISPLAY_TARGET_TASKBAR)
        g_config.display_target = ML_DISPLAY_TARGET_TASKBAR;

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
    ImGui::Begin(i18n_get_utf8(STR_APP_TITLE), NULL,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);

    if (ImGui::CollapsingHeader(i18n_get_utf8(STR_DISPLAY_AREA), ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::SliderInt(i18n_get_utf8(STR_WINDOW_WIDTH), &g_config.width, 50, 1920);
        ImGui::SliderInt(i18n_get_utf8(STR_WINDOW_HEIGHT), &g_config.height, 10, 200);

        bool autoPos = (g_config.pos_x < 0 || g_config.pos_y < 0);
        if (ImGui::Checkbox(i18n_get_utf8(STR_POS_AUTO), &autoPos))
        {
            if (autoPos)
            {
                g_config.pos_x = -1;
                g_config.pos_y = -1;
            }
            else
            {
                HWND hwndTaskbar = FindWindowW(L"Shell_TrayWnd", NULL);
                RECT rcTaskbar = {};
                if (hwndTaskbar) GetWindowRect(hwndTaskbar, &rcTaskbar);
                g_config.pos_x = rcTaskbar.right - g_config.width - 320;
                g_config.pos_y = rcTaskbar.top + (rcTaskbar.bottom - rcTaskbar.top - g_config.height) / 2;
            }
        }
        if (!autoPos)
        {
            ImGui::SliderInt(i18n_get_utf8(STR_POS_X), &g_config.pos_x, 0, 3840);
            ImGui::SliderInt(i18n_get_utf8(STR_POS_Y), &g_config.pos_y, 0, 2160);
        }
    }

    if (ImGui::CollapsingHeader(i18n_get_utf8(STR_LYRIC_SOURCE), ImGuiTreeNodeFlags_DefaultOpen))
    {
        const char* sources[] = {
            i18n_get_utf8(STR_SOURCE_MANUAL),
            i18n_get_utf8(STR_SOURCE_QQ_MUSIC)
        };
        ImGui::Combo(i18n_get_utf8(STR_TEXT_SOURCE),
            &g_config.text_source, sources, IM_ARRAYSIZE(sources));
        if (g_config.text_source == ML_TEXT_SOURCE_QQMUSIC_LOCAL)
        {
            ImGui::SliderInt(i18n_get_utf8(STR_LYRIC_OFFSET),
                &g_config.lyric_offset_ms, -3000, 3000);
            ImGui::InputText(i18n_get_utf8(STR_QQMUSIC_LYRIC_DIR), g_qqMusicLyricDirBuf, sizeof(g_qqMusicLyricDirBuf));
        }
        else
        {
            if (ImGui::InputTextMultiline("##text", g_textContentBuf, sizeof(g_textContentBuf), ImVec2(-1, 60)))
                SyncUtf8ToConfig();
        }
    }

    if (ImGui::CollapsingHeader(i18n_get_utf8(STR_APPEARANCE_COLORS), ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::SliderInt(i18n_get_utf8(STR_FONT_SIZE), &g_config.font_size, 6, 72);

        if (font_catalog_combo_items()[0] != '\0')
        {
            if (ImGui::Combo(i18n_get_utf8(STR_FONT_NAME),
                font_catalog_selected_index(), font_catalog_combo_items()))
            {
                const wchar_t* selectedFont = font_catalog_selected_font_name();
                if (selectedFont)
                    wcscpy_s(g_config.font_name, 64, selectedFont);
            }
        }

        static int selectedScheme = 0;
        const char* schemeNames[IM_ARRAYSIZE(g_colorSchemes)] = {};
        for (int i = 0; i < IM_ARRAYSIZE(g_colorSchemes); ++i)
            schemeNames[i] = i18n_get_utf8(g_colorSchemes[i].nameStringId);
        if (ImGui::Combo(i18n_get_utf8(STR_COLOR_SCHEME), &selectedScheme, schemeNames, IM_ARRAYSIZE(schemeNames)))
            ApplyColorScheme(g_colorSchemes[selectedScheme]);

        float fc[3] = { g_config.font_r / 255.f, g_config.font_g / 255.f, g_config.font_b / 255.f };
        if (ImGui::ColorEdit3(i18n_get_utf8(STR_UNSUNG_COLOR), fc))
        {
            g_config.font_r = (int)(fc[0] * 255);
            g_config.font_g = (int)(fc[1] * 255);
            g_config.font_b = (int)(fc[2] * 255);
        }
        float hc[3] = { g_config.highlight_r / 255.f, g_config.highlight_g / 255.f, g_config.highlight_b / 255.f };
        if (ImGui::ColorEdit3(i18n_get_utf8(STR_SUNG_COLOR), hc))
        {
            g_config.highlight_r = (int)(hc[0] * 255);
            g_config.highlight_g = (int)(hc[1] * 255);
            g_config.highlight_b = (int)(hc[2] * 255);
        }
    }

    if (ImGui::CollapsingHeader(i18n_get_utf8(STR_SYSTEM), ImGuiTreeNodeFlags_DefaultOpen))
    {
        const char* langs[] = { "English", "Simplified Chinese" };
        if (ImGui::Combo(i18n_get_utf8(STR_LANGUAGE), &g_language, langs, IM_ARRAYSIZE(langs)))
        {
            i18n_init((ML_Lang)g_language);
            WCHAR iniPath[MAX_PATH];
            config_get_ini_path(iniPath, MAX_PATH);
            WritePrivateProfileStringW(L"General", L"Language", g_language == 1 ? L"zh-cn" : L"en", iniPath);
        }

        ImGui::Text("%s: %s", i18n_get_utf8(STR_STATUS),
            g_injected ? i18n_get_utf8(STR_INJECTED) : i18n_get_utf8(STR_NOT_INJECTED));
    }

    ImGui::Separator();

    // Action buttons
    if (ImGui::Button(i18n_get_utf8(STR_PREVIEW)))
        ShowPreview(&g_config, GetModuleHandle(NULL));
    ImGui::SameLine();
    if (ImGui::Button(i18n_get_utf8(STR_APPLY)))
    {
        SyncUtf8ToConfig();
        config_save(&g_config);
        if (g_injected)
        {
            if (g_config.display_target != g_lastAppliedTarget)
            {
                EjectDLL();
                if (InjectDLL())
                {
                    g_injected = true;
                    g_lastAppliedTarget = g_config.display_target;
                }
                else
                {
                    g_injected = false;
                    ImGui::OpenPopup("InjectFailed");
                }
            }
            else
            {
                pipe_send_config(&g_config);
            }
        }
        else
        {
            if (InjectDLL())
            {
                g_injected = true;
                g_lastAppliedTarget = g_config.display_target;
            }
            else
                ImGui::OpenPopup("InjectFailed");
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(i18n_get_utf8(STR_STOP)))
    {
        ClosePreview();
        if (EjectDLL())
            g_injected = false;
        else
            ImGui::OpenPopup("EjectFailed");
    }

    if (ImGui::BeginPopup("InjectFailed"))
    {
        ImGui::Text("%s", i18n_get_utf8(STR_INJECT_FAILED));
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("EjectFailed"))
    {
        ImGui::Text("%s", i18n_get_utf8(STR_EJECT_FAILED));
        ImGui::EndPopup();
    }

    ImGui::End();
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int)
{
    // Load config
    config_load(&g_config);
    g_lastAppliedTarget = g_config.display_target;
    SyncConfigToUtf8();

    // Load language
    WCHAR iniPath[MAX_PATH];
    config_get_ini_path(iniPath, MAX_PATH);
    WCHAR langBuf[16] = {0};
    GetPrivateProfileStringW(L"General", L"Language", L"en", langBuf, 16, iniPath);
    if (wcscmp(langBuf, L"zh-cn") == 0)
    {
        g_language = 1;
        i18n_init(ML_LANG_ZH_CN);
    }
    else
    {
        g_language = 0;
        i18n_init(ML_LANG_EN);
    }

    font_catalog_init(g_config.font_name);

    // Check if already injected
    g_injected = IsDLLInjected();

    // Create application window
    HICON hIcon = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON,
        GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
    HICON hIconSmall = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW), CS_CLASSDC, WndProc, 0L, 0L, hInstance, hIcon, NULL, NULL, NULL, L"MostLyricClass", hIconSmall };
    RegisterClassExW(&wc);
    HWND hwnd = CreateWindowExW(0, L"MostLyricClass", L"MostLyric", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        100, 100, 480, 620, NULL, NULL, hInstance, NULL);
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    font_catalog_load_imgui_font(io, 18.0f);

    ImGui::StyleColorsLight();
    ApplyWin11Style();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Main loop
    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        BuildUI();

        ImGui::Render();
        const float clear_color[] = { 0.953f, 0.957f, 0.973f, 1.0f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0);
    }

    // Cleanup
    ClosePreview();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}
