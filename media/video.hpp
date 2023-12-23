#include <mx/mx.hpp>
#include <media/streams.hpp>

namespace ion {

struct iVideo;
struct Video:mx {
    mx_declare(Video, mx, iVideo);
    
    int        write_frame(Frame &f);
    ion::image fetch_frame(int frame_id);
    void       stop();

    Video(int width, int height, int hz, int audio_sample_rate, path output);
    Video(path file);
};
}