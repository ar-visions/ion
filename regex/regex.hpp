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

    RegEx(str pattern, Behaviour b = Behaviour::none);
    RegEx(utf16 pattern, Behaviour b = Behaviour::none);
    RegEx(symbol pattern, Behaviour b = Behaviour::none);
    static str   escape(str input);
    static utf16 escape(utf16 input);
    array<str>   exec(str input);
    array<utf16> exec(utf16 input);
    mx_declare(RegEx, mx, iRegEx)
};

}
