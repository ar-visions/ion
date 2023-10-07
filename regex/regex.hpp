#pragma once
#include <mx/mx.hpp>

#ifdef REGEX_IMPL
#define ONIG_STATIC 1
#include <oniguruma.h>
typedef OnigRegexType  regex_t;
#endif
struct iRegEx;

namespace ion {

struct RegEx:mx {
    RegEx(str pattern);
    array<str> exec(str input);
    mx_declare(RegEx, mx, iRegEx)
};

}
