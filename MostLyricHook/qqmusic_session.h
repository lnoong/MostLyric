#ifndef QQMUSIC_SESSION_H
#define QQMUSIC_SESSION_H

#include <string>

struct TrackSnapshot {
    std::wstring title;
    std::wstring artist;
    long long position_ms = 0;
    bool is_playing = false;
    bool valid = false;
};

bool qqmusic_poll_current_track(TrackSnapshot& out);

#endif
