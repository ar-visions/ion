#include <mx/mx.hpp>
#include <media/streams.hpp>

namespace ion {

struct iVideo;
struct Video:mx {
    mx_declare(Video, mx, iVideo);
    
    int        write_frame(Frame &);
    bool       has_keyframe(mx nalus);
    ion::image seek_frame(i64);
    ion::image get_current_image();
    bool       get_play_state();
    i64        current_frame();
    void       stop();
    void       play_state(bool);
    bool       is_recording();
    bool       is_playback();
    i64        duration();
    i64        timescale();
    i64        frame_count();
    int        frame_rate();
    i64        audio_timescale();
    image      audio_spectrum();

    Video(int width, int height, int hz, int audio_sample_rate, path output);
    Video(path file);
};
}