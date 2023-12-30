#include <mx/mx.hpp>
#include <media/streams.hpp>

namespace ion {

struct iVideo;
struct Video:mx {
    mx_declare(Video, mx, iVideo);
    
    int        write_frame(Frame &f);
    ion::image fetch_frame(int frame_id);
    void       stop();
    bool       is_recording();
    bool       is_playback();
    i64        duration();
    i64        timescale();
    i64        frame_count();
    i64        audio_timescale();
    image      audio_spectrum();

    Video(int width, int height, int hz, int audio_sample_rate, path output);
    Video(path file);
};
}