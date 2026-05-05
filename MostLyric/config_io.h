#pragma once
#include "../config/config.h"
#include <Windows.h>

void config_load(ML_CONFIG* cfg);
void config_save(const ML_CONFIG* cfg);
BOOL pipe_send_config(const ML_CONFIG* cfg);
BOOL pipe_send_stop(void);
void config_get_ini_path(WCHAR* path, SIZE_T cchPath);
