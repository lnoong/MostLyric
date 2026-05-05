#include "qrc_codec.h"

#include "hook_log.h"
#include "qrc_des.h"

#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <cstring>

typedef int (__cdecl* ZlibUncompressFn)(unsigned char*, unsigned long*, const unsigned char*, unsigned long);

static bool InflateZlib(const std::vector<unsigned char>& input, std::string& output)
{
    static HMODULE zlib = LoadLibraryW(L"zlib1.dll");
    if (!zlib)
        zlib = LoadLibraryW(L"zlib.dll");
    if (!zlib)
    {
        WCHAR base[MAX_PATH] = {};
        WCHAR candidate[MAX_PATH] = {};
        if (GetEnvironmentVariableW(L"ProgramFiles(x86)", base, MAX_PATH))
        {
            swprintf_s(candidate, L"%ls\\Tencent\\QQMusic\\zlib1.dll", base);
            zlib = LoadLibraryW(candidate);
        }
        if (!zlib && GetEnvironmentVariableW(L"ProgramFiles", base, MAX_PATH))
        {
            swprintf_s(candidate, L"%ls\\Tencent\\QQMusic\\zlib1.dll", base);
            zlib = LoadLibraryW(candidate);
        }
    }
    if (!zlib)
    {
        Log("QRC decrypt: zlib1.dll/zlib.dll not found; cannot inflate encrypted qrc\n");
        return false;
    }

    auto uncompress = reinterpret_cast<ZlibUncompressFn>(GetProcAddress(zlib, "uncompress"));
    if (!uncompress)
        return false;

    unsigned long outLen = (unsigned long)std::max<size_t>(input.size() * 8, 64 * 1024);
    for (int i = 0; i < 6; ++i)
    {
        std::vector<unsigned char> out(outLen);
        int rc = uncompress(out.data(), &outLen, input.data(), (unsigned long)input.size());
        if (rc == 0)
        {
            output.assign((char*)out.data(), (size_t)outLen);
            return true;
        }
        outLen *= 2;
    }
    Log("QRC decrypt: zlib inflate failed\n");
    return false;
}

static bool HexToBytes(const std::vector<unsigned char>& input, std::vector<unsigned char>& output)
{
    auto value = [](unsigned char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };

    std::vector<unsigned char> clean;
    for (unsigned char c : input)
    {
        if (!isspace(c))
            clean.push_back(c);
    }
    if (clean.size() < 2 || clean.size() % 2 != 0)
        return false;

    output.resize(clean.size() / 2);
    for (size_t i = 0; i < clean.size(); i += 2)
    {
        int hi = value(clean[i]);
        int lo = value(clean[i + 1]);
        if (hi < 0 || lo < 0)
            return false;
        output[i / 2] = (unsigned char)((hi << 4) | lo);
    }
    return true;
}

static void DecodeQmcPayload(std::vector<unsigned char>& data)
{
    static const unsigned char magic[] = {0x98, 0x25, 0xB0, 0xAC, 0xE3, 0x02, 0x83, 0x68, 0xE8, 0xFC, 0x6C};
    if (data.size() < sizeof(magic) || memcmp(data.data(), magic, sizeof(magic)) != 0)
        return;

    static const unsigned char qmcKey[128] = {
        0xc3, 0x4a, 0xd6, 0xca, 0x90, 0x67, 0xf7, 0x52,
        0xd8, 0xa1, 0x66, 0x62, 0x9f, 0x5b, 0x09, 0x00,
        0xc3, 0x5e, 0x95, 0x23, 0x9f, 0x13, 0x11, 0x7e,
        0xd8, 0x92, 0x3f, 0xbc, 0x90, 0xbb, 0x74, 0x0e,
        0xc3, 0x47, 0x74, 0x3d, 0x90, 0xaa, 0x3f, 0x51,
        0xd8, 0xf4, 0x11, 0x84, 0x9f, 0xde, 0x95, 0x1d,
        0xc3, 0xc6, 0x09, 0xd5, 0x9f, 0xfa, 0x66, 0xf9,
        0xd8, 0xf0, 0xf7, 0xa0, 0x90, 0xa1, 0xd6, 0xf3,
        0xc3, 0xf3, 0xd6, 0xa1, 0x90, 0xa0, 0xf7, 0xf0,
        0xd8, 0xf9, 0x66, 0xfa, 0x9f, 0xd5, 0x09, 0xc6,
        0xc3, 0x1d, 0x95, 0xde, 0x9f, 0x84, 0x11, 0xf4,
        0xd8, 0x51, 0x3f, 0xaa, 0x90, 0x3d, 0x74, 0x47,
        0xc3, 0x0e, 0x74, 0xbb, 0x90, 0xbc, 0x3f, 0x92,
        0xd8, 0x7e, 0x11, 0x13, 0x9f, 0x23, 0x95, 0x5e,
        0xc3, 0x00, 0x09, 0x5b, 0x9f, 0x62, 0x66, 0xa1,
        0xd8, 0x52, 0xf7, 0x67, 0x90, 0xca, 0xd6, 0x4a
    };
    for (size_t i = 0; i < data.size(); ++i)
    {
        size_t keyOffset = i > 0x7FFF ? (i % 0x7FFF) & 0x7F : i & 0x7F;
        data[i] ^= qmcKey[keyOffset];
    }
    data.erase(data.begin(), data.begin() + sizeof(magic));
    Log("QRC decrypt: QMC pre-decode applied, payload=%zu bytes\n", data.size());
}

bool qrc_decode_bytes(const std::vector<unsigned char>& bytes, std::string& plaintext)
{
    if (bytes.empty())
        return false;

    if (bytes[0] == '[' || bytes[0] == '<')
    {
        plaintext.assign((const char*)bytes.data(), bytes.size());
        return true;
    }

    std::vector<unsigned char> data;
    if (!HexToBytes(bytes, data))
        data = bytes;

    DecodeQmcPayload(data);

    static const unsigned char key1[8] = {'!', '@', '#', ')', '(', 'N', 'H', 'L'};
    static const unsigned char key2[8] = {'1', '2', '3', 'Z', 'X', 'C', '!', '@'};
    static const unsigned char key3[8] = {'!', '@', '#', ')', '(', '*', '$', '%'};
    if (data.size() < 8 || data.size() % 8 != 0)
    {
        Log("QRC decrypt: encrypted size is not DES block aligned (%zu)\n", data.size());
        return false;
    }

    if (!qrc_des_transform(data.data(), data.size(), key1, false) ||
        !qrc_des_transform(data.data(), data.size(), key2, true) ||
        !qrc_des_transform(data.data(), data.size(), key3, false))
    {
        Log("QRC decrypt: DES transform failed\n");
        return false;
    }

    return InflateZlib(data, plaintext);
}
