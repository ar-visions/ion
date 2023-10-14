#pragma once
#include <mx/mx.hpp>

#ifdef REGEX_IMPL
#define ONIG_STATIC 1
#include <oniguruma.h>
#endif
struct iRegEx;

namespace ion {

template <typename T>
struct indexed {
    T   value;
    i64 index;
    i64 length;
    operator T&() {
        return value;
    }
};

/// support the multi-pattern pattern with different constructor inputs
/// essentially the interface of vscode's oniguruma scanner, but looks more like JS
struct RegEx:mx {
    enums(Behaviour, none, none, global, sticky, global_sticky);

    RegEx(str           pattern,  Behaviour b = Behaviour::none);
    RegEx(utf16         pattern,  Behaviour b = Behaviour::none);
    RegEx(symbol        pattern,  Behaviour b = Behaviour::none);
    RegEx(array<utf16>  patterns, Behaviour b = Behaviour::none);

    static str    escape(str input);
    static utf16  escape(utf16 input);

    array<indexed<str>>     exec(str input);
    array<indexed<utf16>>   exec(utf16 input);

    void      set_cursor(num from, num to = -1);
    void   load_patterns(utf16 *patterns, size_t len);
    void   load_patterns(str   *patterns, size_t len);
    void           reset();
    size_t pattern_index();

    template <typename T>
    bool test(T input) {
        return exec(input);
    }

    template <typename T>
    T replace(T input, mx mx_replacement) {
        array<T> matches = exec(input);
        T result = input;
        lambda<T(T, T)> fn = (mx_replacement.type()->traits & traits::lambda) ?
            mx_replacement.grab() : lambda<T(T, T)> {};
        T s_replacement = !fn ? mx_replacement.grab() : lambda<T(T, T)> {};
        ///
        for (T& match : matches) {
            int pos = result.index_of(match);
            if (pos >= 0) {
                T m0 = result.mid(0, pos);
                T m1 = result.mid(pos + match.len());
                if (fn) {
                    T cur = result.mid(pos, match.len());
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

}
