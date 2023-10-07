#pragma once
#include <mx/mx.hpp>

#ifdef REGEX_IMPL
#define ONIG_STATIC 1
#include <oniguruma.h>
#endif
struct iRegEx;

namespace ion {

struct RegEx:mx {
    enums(Behaviour, none, none, global, sticky, global_sticky);

    RegEx(str pattern);
    RegEx(symbol pattern);
    array<str> exec(str input, Behaviour b = Behaviour::sticky);
    mx_declare(RegEx, mx, iRegEx)
};

}
