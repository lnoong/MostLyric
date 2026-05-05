#ifndef CONFIG_READER_H
#define CONFIG_READER_H

#include "../config/config.h"

#ifdef __cplusplus
extern "C" {
#endif

void config_reader_init(void);
const ML_CONFIG* config_reader_get(void);
void config_reader_update(const char* kv);
void config_reader_commit(void);

#ifdef __cplusplus
}
#endif

#endif
