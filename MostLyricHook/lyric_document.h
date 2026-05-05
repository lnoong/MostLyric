#ifndef LYRIC_DOCUMENT_H
#define LYRIC_DOCUMENT_H

#include <Windows.h>
#include <string>
#include <vector>

struct LyricSpan {
    long long start_ms = 0;
    long long duration_ms = 0;
    UINT32 start_char = 0;
    UINT32 end_char = 0;
};

struct LyricLine {
    long long start_ms = 0;
    long long duration_ms = 0;
    std::wstring text;
    std::vector<LyricSpan> spans;
};

struct LyricDocument {
    std::vector<LyricLine> lines;
};

LyricDocument lyric_parse_document(const std::wstring& text);
void lyric_highlight_at(const LyricLine& line, long long posMs, UINT32& chars, UINT32& endChars, UINT32& progress);

#endif
