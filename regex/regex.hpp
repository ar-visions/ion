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
    
    array<str>     exec(str input);
    array<utf16>   exec(utf16 input);

    void set_cursor(num from, num to = -1);
    
    void  load_patterns(utf16 *patterns, size_t len);
    void  load_patterns(str   *patterns, size_t len);

    template <typename T>
    bool test(T input) {
        return exec(input);
    }

    template <typename T>
    T replace(T input, mx mx_replacement) {
        array<T> matches = exec(input);
        T result = input;
        lambda<T(T, T)> fn = (mx_replacement.type()->traits & traits::lambda) ?
            mx_replacement.grab() : {};
        T s_replacement = !fn ? mx_replacement.grab() : {};
        ///
        for (T& match : matches) {
            int pos = result.index_of(match);
            if (pos >= 0) {
                T m0 = result.mid(0, pos);
                T m1 = result.mid(pos + match.len());
                if (fn) {
                    T cur = resut.mid(pos, match.len());
                    T replacement = fn(match, cur);
                    T res { m0.len() + s_replacement.len() + m1.len() }; /// reserve size to optimize concat
                    res += m0;
                    res += s_replacement;
                    res += m1;
                    result = res;
                } else {
                    T res { m0.len() + s_replacement.len() + m1.len() }; /// reserve size to optimize concat
                    res += m0;
                    res += s_replacement;
                    res += m1;
                    result = res;
                }
            }
        }

        return result;
    }

    mx_declare(RegEx, mx, iRegEx)
};

/// interface that resembles OnigScanner
struct Scanner:RegEx {
    Scanner(array<utf16> patterns, Behaviour b = Behaviour::none) : {
    }

    Scanner(symbol pattern, Behaviour b = Behaviour::none) : Scanner(utf16(pattern)) { }

};

}
