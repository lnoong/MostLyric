#pragma once

#include <Windows.h>
#include <imgui.h>

void font_catalog_init(const wchar_t* selectedFontName);
const char* font_catalog_combo_items();
int* font_catalog_selected_index();
const wchar_t* font_catalog_selected_font_name();
bool font_catalog_load_imgui_font(ImGuiIO& io, float fontSize);
