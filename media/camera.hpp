#pragma once
#include <media/image.hpp>
#include <media/streams.hpp>
#include <async/async.hpp>

namespace ion {

MStream camera(
        Array<StreamType> stream_types, Array<Media> priority,
        str video_alias, str audio_alias, int width, int height);

}