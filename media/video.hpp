#include <mx/mx.hpp>
#include <media/streams.hpp>
struct iVideo;
namespace ion {
struct Video:mx {
    mx_declare(Video, mx, iVideo);
    int write_frame(Frame &f);
    void stop();
    Video(int width, int height, int hz, int audio_sample_rate, path output);
};
}