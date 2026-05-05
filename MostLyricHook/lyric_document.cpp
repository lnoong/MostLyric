#include "lyric_document.h"

#include <algorithm>
#include <cwchar>
#include <cwctype>

static bool ParseNumber(const std::wstring& text, size_t& pos, long long& out)
{
    size_t begin = pos;
    while (pos < text.size() && iswdigit(text[pos]))
        ++pos;
    if (begin == pos)
        return false;
    out = _wtoi64(text.substr(begin, pos - begin).c_str());
    return true;
}

static bool ParseQrcLine(const std::wstring& line, LyricLine& out)
{
    if (line.empty() || line[0] != L'[')
        return false;
    size_t pos = 1;
    long long start = 0, duration = 0;
    if (!ParseNumber(line, pos, start) || pos >= line.size() || line[pos++] != L',' ||
        !ParseNumber(line, pos, duration) || pos >= line.size() || line[pos++] != L']')
        return false;

    out = {};
    out.start_ms = start;
    out.duration_ms = duration;

    while (pos < line.size())
    {
        std::wstring token;
        while (pos < line.size() && line[pos] != L'(')
            token.push_back(line[pos++]);
        if (pos >= line.size() || line[pos++] != L'(')
            break;

        long long spanStart = 0, spanDuration = 0;
        if (!ParseNumber(line, pos, spanStart) || pos >= line.size() || line[pos++] != L',' ||
            !ParseNumber(line, pos, spanDuration))
            break;
        while (pos < line.size() && line[pos] != L')')
            ++pos;
        if (pos < line.size())
            ++pos;

        UINT32 startChar = (UINT32)out.text.size();
        out.text += token;
        LyricSpan span;
        span.start_ms = spanStart >= out.start_ms ? spanStart - out.start_ms : spanStart;
        span.duration_ms = spanDuration;
        span.start_char = startChar;
        span.end_char = (UINT32)out.text.size();
        out.spans.push_back(span);
    }
    return !out.text.empty();
}

static bool ParseLrcTime(const std::wstring& line, long long& ms, size_t& endPos)
{
    int mm = 0, ss = 0, cs = 0;
    wchar_t dummy = 0;
    if (swscanf_s(line.c_str(), L"[%d:%d.%d%c", &mm, &ss, &cs, &dummy, 1) < 3)
        return false;
    size_t close = line.find(L']');
    if (close == std::wstring::npos)
        return false;
    ms = (long long)mm * 60000 + (long long)ss * 1000 + (long long)cs * 10;
    endPos = close + 1;
    return true;
}

LyricDocument lyric_parse_document(const std::wstring& text)
{
    LyricDocument doc;
    size_t pos = 0;
    while (pos < text.size())
    {
        size_t next = text.find_first_of(L"\r\n", pos);
        std::wstring line = text.substr(pos, next == std::wstring::npos ? std::wstring::npos : next - pos);
        pos = next == std::wstring::npos ? text.size() : next + 1;
        if (line.empty())
            continue;

        LyricLine qrcLine;
        if (ParseQrcLine(line, qrcLine))
        {
            doc.lines.push_back(qrcLine);
            continue;
        }

        long long ms = 0;
        size_t endPos = 0;
        if (ParseLrcTime(line, ms, endPos) && endPos < line.size())
        {
            LyricLine lrcLine;
            lrcLine.start_ms = ms;
            lrcLine.text = line.substr(endPos);
            doc.lines.push_back(lrcLine);
        }
    }

    std::sort(doc.lines.begin(), doc.lines.end(), [](const LyricLine& a, const LyricLine& b) {
        return a.start_ms < b.start_ms;
    });
    for (size_t i = 0; i < doc.lines.size(); ++i)
    {
        if (doc.lines[i].duration_ms <= 0 && i + 1 < doc.lines.size())
            doc.lines[i].duration_ms = doc.lines[i + 1].start_ms - doc.lines[i].start_ms;
    }
    return doc;
}

void lyric_highlight_at(const LyricLine& line, long long posMs, UINT32& chars, UINT32& endChars, UINT32& progress)
{
    long long elapsed = posMs - line.start_ms;
    chars = 0;
    endChars = 0;
    progress = 0;
    for (const LyricSpan& span : line.spans)
    {
        if (elapsed >= span.start_ms + span.duration_ms)
        {
            chars = span.end_char;
        }
        else if (elapsed >= span.start_ms)
        {
            chars = span.start_char;
            endChars = span.end_char;
            if (span.duration_ms > 0)
                progress = (UINT32)(((elapsed - span.start_ms) * 10000) / span.duration_ms);
            if (progress > 10000)
                progress = 10000;
            return;
        }
        else
        {
            break;
        }
    }
    endChars = chars;
    progress = 10000;
}
