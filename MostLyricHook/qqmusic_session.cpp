#include "qqmusic_session.h"

#include <Windows.h>
#include <chrono>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>

#pragma comment(lib, "windowsapp.lib")

using namespace winrt;
using namespace Windows::Media::Control;

bool qqmusic_poll_current_track(TrackSnapshot& out)
{
    out = {};
    try
    {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        auto manager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
        if (!manager)
            return false;
        auto session = manager.GetCurrentSession();
        if (!session)
            return false;

        auto props = session.TryGetMediaPropertiesAsync().get();
        auto timeline = session.GetTimelineProperties();
        auto playbackInfo = session.GetPlaybackInfo();
        out.title = props.Title().c_str();
        out.artist = props.Artist().c_str();
        out.position_ms = std::chrono::duration_cast<std::chrono::milliseconds>(timeline.Position()).count();
        out.is_playing = playbackInfo.PlaybackStatus() == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;
        out.valid = !out.title.empty();
        return out.valid;
    }
    catch (...)
    {
        return false;
    }
}
