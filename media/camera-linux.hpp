#pragma once

#include <media/image.hpp>
#include <media/streams.hpp>
#include <async/async.hpp>

namespace ion {

Streams camera(array<StreamType> media,
               array<Media> priority,
               str video_dev, str audio_dev,
               int width, int height);

}
