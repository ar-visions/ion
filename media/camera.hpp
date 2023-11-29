#pragma once
#include <media/image.hpp>
#include <media/streams.hpp>
#include <async/async.hpp>

namespace ion {

MStream camera(
        array<StreamType> stream_types, array<Media> priority,
        str video_alias, str audio_alias, int width, int height);

}