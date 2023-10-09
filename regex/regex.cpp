#define REGEX_IMPL
#include <regex/regex.hpp>

using namespace ion;

mx_implement(RegEx, mx)

struct iRegEx {
    regex_t *regex;
    num      last_index;
    num      bytes_left;
    memory*  last_mem;
    
    iRegEx() {
        static bool init;
        if (!init) {
            onig_init();
            init = true;
        }
    }
    ~iRegEx() {
        if (regex) {
            onig_free(regex);
        }
        drop(last_mem);
    }
    register(iRegEx);
};

RegEx::RegEx(symbol pattern) : RegEx(str(pattern)) { }

RegEx::RegEx(str pattern, RegEx::Behaviour b) : RegEx() {
    OnigErrorInfo err;
    int r = onig_new(
        &data->regex, (OnigUChar*)pattern.cs(),
        (OnigUChar*)(pattern.cs() + pattern.len()),
        ONIG_OPTION_DEFAULT, ONIG_ENCODING_UTF8, ONIG_SYNTAX_DEFAULT, &err);
    if (r != ONIG_NORMAL) {
        char estr[128];
        onig_error_code_to_str((UChar*)estr, r, &err);
        console.log("compilation failure for pattern: {0}\nerror:{1}", {pattern, str(estr)});
    }
}

/// escape regex characters with '\'
str RegEx::escape(str value) {
    return value.escape("\\{}*+?|^$.,[]()# ");
}

array<str> RegEx::exec(str input) {
    Behaviour b = data->b;
    array<str> result;

    if (input.mem != data->last_mem || b == Behaviour::none) {
        ::drop(data->last_mem);
        data->last_index = 0;
        data->bytes_left = 0;
        data->last_mem = input.grab();
    }
    if (data->regex && (!data->last_index || data->bytes_left)) {
        OnigRegion* region = onig_region_new();
        size_t      ilen   = input.len();
        cstr        origin = input.cs();
        cstr        i      = &origin[data->last_index];
        cstr        e      = &origin[ilen];
        ///
        for (size_t cursor = 0; ; cursor += region->end[0] + 1) {
            if (cursor == ilen)
                break;

            cstr s = &i[cursor];
            int  r = onig_search(
                data->regex,
                (OnigUChar*)s, (OnigUChar*)e,
                (OnigUChar*)s, (OnigUChar*)e,
                region, ONIG_OPTION_NONE);
            
            if (r >= 0) {
                cstr start   = s + region->beg[0];
                cstr end     = s + region->end[0];
                str  match   = str(start, std::distance(start, end));
                result      += match;
                data->last_index = std::distance(i, end + 1);
                data->bytes_left = std::distance(origin + data->last_index, e);
                continue;
            } else if (b == Behaviour::sticky || b == Behaviour::global_sticky) {
                data->last_index = 0;
                data->bytes_left = 0;
            }
            break;
        }
        onig_region_free(region, 1);
    }
    return result;
}
