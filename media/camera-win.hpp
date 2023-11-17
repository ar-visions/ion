#pragma once
#include <media/image.hpp>
#include <media/streams.hpp>
#include <async/async.hpp>

namespace ion {

Streams camera(array<StreamType> media, array<Media> priority, str alias, int rwidth, int rheight);

}