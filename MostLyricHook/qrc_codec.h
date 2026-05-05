#ifndef QRC_CODEC_H
#define QRC_CODEC_H

#include <string>
#include <vector>

bool qrc_decode_bytes(const std::vector<unsigned char>& bytes, std::string& plaintext);

#endif
