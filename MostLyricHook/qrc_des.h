#ifndef QRC_DES_H
#define QRC_DES_H

#include <stddef.h>
#include <stdint.h>

bool qrc_des_transform(uint8_t* data, size_t size, const uint8_t key[8], bool encrypt);

#endif
