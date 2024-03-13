#pragma once
#include <math/math.hpp>

namespace ion {
struct ngon {
    size_t size;
    u32   *indices;
};

using mesh  = array<ngon>;
using face  = ngon;
using vpair = std::pair<int, int>;

struct vpair_hash {
    std::size_t operator()(const vpair& p) const {
        return std::hash<int>()(p.first) ^ std::hash<int>()(p.second);
    }
};
}
