#include "lyric_engine.h"

#include "config_reader.h"
#include "hook_log.h"
#include "lyric_document.h"
#include "qrc_codec.h"
#include "qqmusic_session.h"

#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <exception>
#include <filesystem>
#include <cwctype>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

static const UINT ML_WM_RENDER_LYRIC = WM_APP + 0x4D4C;
static const DWORD ML_LYRIC_FRAME_INTERVAL_MS = 16;
static const DWORD ML_IDLE_POLL_INTERVAL_MS = 300;
static const DWORD ML_PAUSED_POLL_INTERVAL_MS = 1000;
static const DWORD ML_PLAYING_TRACK_POLL_INTERVAL_MS = 3000;

static std::thread g_worker;
static std::atomic<bool> g_running{false};
static std::mutex g_frameMutex;
static ML_LYRIC_RENDER_FRAME g_frame = {};
static HWND g_hwndNotify = nullptr;

static std::wstring ToLower(std::wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return (wchar_t)towlower(ch);
    });
    return value;
}

static std::wstring NormalizeForMatch(std::wstring value)
{
    value = ToLower(value);
    std::wstring out;
    for (wchar_t ch : value)
    {
        if (iswalnum(ch) || ch > 127)
            out.push_back(ch);
    }
    return out;
}

static std::wstring Utf8ToWide(const std::string& text)
{
    if (text.empty())
        return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, text.data(), (int)text.size(), nullptr, 0);
    if (len <= 0)
        return {};
    std::wstring out((size_t)len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), (int)text.size(), out.data(), len);
    return out;
}

static bool ReadFileBytes(const fs::path& path, std::vector<unsigned char>& bytes)
{
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;

    LARGE_INTEGER size = {};
    bool ok = GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart < 16 * 1024 * 1024;
    if (ok)
    {
        bytes.resize((size_t)size.QuadPart);
        DWORD read = 0;
        ok = ReadFile(file, bytes.data(), (DWORD)bytes.size(), &read, nullptr) && read == bytes.size();
    }
    CloseHandle(file);
    return ok;
}

static void AddCandidateDir(std::vector<fs::path>& dirs, const wchar_t* value)
{
    if (value && value[0])
        dirs.emplace_back(value);
}

static fs::path FindLyricFile(const ML_CONFIG& cfg, const TrackSnapshot& track)
{
    std::vector<fs::path> dirs;
    AddCandidateDir(dirs, cfg.qqmusic_lyric_dir);

    WCHAR buf[MAX_PATH] = {};
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", buf, MAX_PATH))
    {
        dirs.emplace_back(std::wstring(buf) + L"\\Tencent\\QQMusic");
        dirs.emplace_back(std::wstring(buf) + L"\\QQMusic");
    }
    if (GetEnvironmentVariableW(L"APPDATA", buf, MAX_PATH))
        dirs.emplace_back(std::wstring(buf) + L"\\Tencent\\QQMusic");

    std::wstring title = NormalizeForMatch(track.title);
    std::wstring artist = NormalizeForMatch(track.artist);
    fs::path best;
    int bestScore = -1;

    for (const auto& dir : dirs)
    {
        std::error_code ec;
        if (!fs::exists(dir, ec))
            continue;
        int scanned = 0;
        for (fs::recursive_directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end;
             it != end && !ec && scanned < 3000; it.increment(ec), ++scanned)
        {
            if (!it->is_regular_file(ec))
                continue;
            std::wstring ext = ToLower(it->path().extension().wstring());
            if (ext != L".qrc" && ext != L".lrc")
                continue;
            std::wstring name = NormalizeForMatch(it->path().stem().wstring());
            int score = 0;
            if (!title.empty() && name.find(title) != std::wstring::npos) score += 100;
            if (!artist.empty() && name.find(artist) != std::wstring::npos) score += 50;
            if (ext == L".qrc") score += 5;
            if (score > bestScore)
            {
                bestScore = score;
                best = it->path();
            }
        }
    }

    if (bestScore <= 0)
        return {};
    return best;
}

static bool LoadLyricsForTrack(const ML_CONFIG& cfg, const TrackSnapshot& track, LyricDocument& doc)
{
    fs::path path = FindLyricFile(cfg, track);
    if (path.empty())
    {
        Log("Lyric engine: no local lyric matched title=%ls artist=%ls\n", track.title.c_str(), track.artist.c_str());
        return false;
    }

    std::vector<unsigned char> bytes;
    if (!ReadFileBytes(path, bytes))
        return false;

    std::string plain;
    if (!qrc_decode_bytes(bytes, plain))
    {
        Log("Lyric engine: failed to decode lyric file %ls\n", path.c_str());
        return false;
    }

    doc = lyric_parse_document(Utf8ToWide(plain));
    Log("Lyric engine: loaded %zu lines from %ls\n", doc.lines.size(), path.c_str());
    return !doc.lines.empty();
}

static void PublishFrame(const std::wstring& text, UINT32 highlightChars, UINT32 highlightEndChars, UINT32 highlightProgress, bool active)
{
    {
        std::lock_guard<std::mutex> lock(g_frameMutex);
        g_frame.active = active ? TRUE : FALSE;
        g_frame.highlight_chars = highlightChars;
        g_frame.highlight_end_chars = highlightEndChars;
        g_frame.highlight_progress = highlightProgress;
        wcsncpy_s(g_frame.text, text.c_str(), _TRUNCATE);
    }
    if (g_hwndNotify)
        PostMessageW(g_hwndNotify, ML_WM_RENDER_LYRIC, 0, 0);
}

static void ResetTrackState(TrackSnapshot& cachedTrack, LyricDocument& doc, std::wstring& trackKey,
    size_t& activeLineIndex, long long& activeLineEndMs, bool& pendingGapSync)
{
    PublishFrame(L"", 0, 0, 0, false);
    cachedTrack = {};
    trackKey.clear();
    doc = {};
    activeLineIndex = (size_t)-1;
    activeLineEndMs = 0;
    pendingGapSync = false;
}

static bool ShouldPollMedia(const TrackSnapshot& cachedTrack, bool pendingGapSync, ULONGLONG nowTick, ULONGLONG lastMediaPollTick)
{
    return !cachedTrack.valid ||
        pendingGapSync ||
        (cachedTrack.is_playing && nowTick - lastMediaPollTick >= ML_PLAYING_TRACK_POLL_INTERVAL_MS) ||
        (!cachedTrack.is_playing && nowTick - lastMediaPollTick >= ML_PAUSED_POLL_INTERVAL_MS);
}

static bool TryPublishActiveLine(const LyricDocument& doc, long long positionMs,
    size_t& activeLineIndex, long long& activeLineEndMs)
{
    for (size_t i = 0; i < doc.lines.size(); ++i)
    {
        long long end = doc.lines[i].duration_ms > 0 ? doc.lines[i].start_ms + doc.lines[i].duration_ms :
            (i + 1 < doc.lines.size() ? doc.lines[i + 1].start_ms : doc.lines[i].start_ms + 5000);
        if (positionMs >= doc.lines[i].start_ms && positionMs < end)
        {
            if (activeLineIndex != i)
            {
                activeLineIndex = i;
                activeLineEndMs = end;
            }
            UINT32 highlightChars = 0;
            UINT32 highlightEndChars = 0;
            UINT32 highlightProgress = 0;
            lyric_highlight_at(doc.lines[i], positionMs, highlightChars, highlightEndChars, highlightProgress);
            PublishFrame(doc.lines[i].text, highlightChars, highlightEndChars, highlightProgress, true);
            return true;
        }
    }
    return false;
}

static bool RefreshTrackIfNeeded(const ML_CONFIG& cfg, TrackSnapshot& cachedTrack, LyricDocument& doc,
    std::wstring& trackKey, ULONGLONG nowTick, ULONGLONG& lastMediaPollTick, ULONGLONG& lastTrackPositionTick,
    long long& extrapolatedPosition, size_t& activeLineIndex, long long& activeLineEndMs, bool forcePositionSync)
{
    TrackSnapshot polledTrack;
    lastMediaPollTick = nowTick;
    if (!qqmusic_poll_current_track(polledTrack))
        return cachedTrack.valid;

    std::wstring nextKey = ToLower(polledTrack.title + L"|" + polledTrack.artist);
    bool isNewTrack = !cachedTrack.valid || nextKey != trackKey;
    bool playbackStateChanged = cachedTrack.valid && cachedTrack.is_playing != polledTrack.is_playing;

    if (isNewTrack || playbackStateChanged || !polledTrack.is_playing || forcePositionSync)
    {
        cachedTrack = polledTrack;
        lastTrackPositionTick = nowTick;
        extrapolatedPosition = cachedTrack.position_ms;
    }
    else
    {
        cachedTrack.title = polledTrack.title;
        cachedTrack.artist = polledTrack.artist;
        cachedTrack.is_playing = polledTrack.is_playing;
        cachedTrack.valid = polledTrack.valid;
    }

    if (isNewTrack)
    {
        trackKey = nextKey;
        doc = {};
        activeLineIndex = (size_t)-1;
        activeLineEndMs = 0;
        if (!LoadLyricsForTrack(cfg, cachedTrack, doc))
        {
            std::wstring fallback = cachedTrack.artist.empty() ?
                cachedTrack.title :
                cachedTrack.title + L" - " + cachedTrack.artist;
            PublishFrame(fallback, 0, 0, 0, true);
        }
    }
    return true;
}

static void WorkerMain()
{
    Log("Lyric engine thread started\n");
    LyricDocument doc;
    std::wstring trackKey;
    TrackSnapshot cachedTrack;
    ULONGLONG lastMediaPollTick = 0;
    ULONGLONG lastTrackPositionTick = 0;
    size_t activeLineIndex = (size_t)-1;
    long long activeLineEndMs = 0;
    bool pendingGapSync = false;

    while (g_running.load())
    {
        DWORD sleepMs = ML_LYRIC_FRAME_INTERVAL_MS;
        try
        {
            ML_CONFIG cfg = *config_reader_get();
            if (cfg.text_source != ML_TEXT_SOURCE_QQMUSIC_LOCAL)
            {
                ResetTrackState(cachedTrack, doc, trackKey, activeLineIndex, activeLineEndMs, pendingGapSync);
                Sleep(500);
                continue;
            }

            ULONGLONG nowTick = GetTickCount64();
            long long extrapolatedPosition = cachedTrack.position_ms;
            if (cachedTrack.valid && cachedTrack.is_playing && lastTrackPositionTick != 0)
                extrapolatedPosition += (long long)(nowTick - lastTrackPositionTick);

            bool forcePositionSync = pendingGapSync;
            if (ShouldPollMedia(cachedTrack, pendingGapSync, nowTick, lastMediaPollTick) &&
                !RefreshTrackIfNeeded(cfg, cachedTrack, doc, trackKey, nowTick, lastMediaPollTick, lastTrackPositionTick,
                    extrapolatedPosition, activeLineIndex, activeLineEndMs, forcePositionSync))
            {
                Sleep(ML_IDLE_POLL_INTERVAL_MS);
                continue;
            }
            if (forcePositionSync)
                pendingGapSync = false;

            if (!cachedTrack.valid)
            {
                Sleep(ML_IDLE_POLL_INTERVAL_MS);
                continue;
            }

            extrapolatedPosition = cachedTrack.position_ms;
            if (cachedTrack.is_playing && lastTrackPositionTick != 0)
                extrapolatedPosition += (long long)(nowTick - lastTrackPositionTick);
            long long lyricPosition = extrapolatedPosition + cfg.lyric_offset_ms;

            bool publishedLine = TryPublishActiveLine(doc, lyricPosition, activeLineIndex, activeLineEndMs);
            if (!publishedLine && doc.lines.empty())
                sleepMs = 250;
            else if (!publishedLine)
                pendingGapSync = true;
            else if (!cachedTrack.is_playing)
                sleepMs = 100;
        }
        catch (const std::exception& ex)
        {
            Log("Lyric engine exception: %S\n", ex.what());
            Sleep(1000);
        }
        catch (...)
        {
            Log("Lyric engine unknown exception\n");
            Sleep(1000);
        }

        Sleep(sleepMs);
    }
    PublishFrame(L"", 0, 0, 0, false);
    Log("Lyric engine thread exiting\n");
}

void lyric_engine_start(HWND hwndNotify)
{
    g_hwndNotify = hwndNotify;
    if (g_running.exchange(true))
        return;
    g_worker = std::thread(WorkerMain);
}

void lyric_engine_stop(void)
{
    if (!g_running.exchange(false))
        return;
    if (g_worker.joinable())
        g_worker.join();
}

void lyric_engine_on_config_changed(void)
{
    PublishFrame(L"", 0, 0, 0, false);
}

BOOL lyric_engine_get_frame(ML_LYRIC_RENDER_FRAME* frame)
{
    if (!frame)
        return FALSE;
    std::lock_guard<std::mutex> lock(g_frameMutex);
    *frame = g_frame;
    return g_frame.active;
}
