#define REGEX_IMPL
#include <regex/regex.hpp>

using namespace ion;

mx_implement(RegEx, mx)

struct iRegEx {
    regex_t         *regex;
    RegEx::Behaviour b;
    num              last_index;
    num              bytes_left;
    memory*          last_mem;
    
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

RegEx::RegEx(symbol pattern, Behaviour b) : RegEx(str(pattern), b) { }

RegEx::RegEx(str pattern, Behaviour b) : RegEx() {
    OnigErrorInfo err;
    data->b = b;
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

RegEx::RegEx(utf16 pattern, Behaviour b) : RegEx() {
    OnigErrorInfo err;
    data->b = b;
    int r = onig_new(
        &data->regex, (OnigUChar*)pattern.data,
        (OnigUChar*)(pattern.data + pattern.len()), /// i think its sized by 2?
        ONIG_OPTION_DEFAULT, ONIG_ENCODING_UTF16_LE, ONIG_SYNTAX_DEFAULT, &err);
    if (r != ONIG_NORMAL) {
        char estr[128];
        onig_error_code_to_str((UChar*)estr, r, &err);
        console.log("compilation failure for pattern: {0}\nerror:{1}", {str(pattern), str(estr)});
    }
}

/// replace with above when tested
utf16 RegEx::replace(utf16 input, mx mx_replacement) {
    utf16 result = input;
    return result;
}

/// escape regex characters with '\'
str RegEx::escape(str value) {
    return value.escape("\\{}*+?|^$.,[]()# ");
}

utf16 RegEx::escape(utf16 value) {
    return value.escape("\\{}*+?|^$.,[]()# ");
}

void RegEx::set_cursor(num from, num to) {
    if (data->last_mem) {
        memory *m = data->last_mem;
        if (m->type == typeof(str)) {
            str input = m->grab();
            data->last_index = from;
            data->bytes_left = to + (num)input.len() - from;
            assert(data->bytes_left >= 0);
        } else if (m->type == typeof(utf16)) {
            utf16 input = m->grab();
            data->last_index = from;
            data->bytes_left = to + input.len() - from;
            assert(data->bytes_left >= 0);
        }
    } else {
        assert(false);
    }
}

/// 
array<utf16> RegEx::exec(utf16 input) {
    Behaviour b = data->b;
    array<utf16> result;

    if (data->last_index == 0) /// the user can set this as they can in js
        data->bytes_left = 0;
    
    if (input.mem != data->last_mem || b == Behaviour::none) {
        ::drop(data->last_mem);
        data->last_index = 0;
        data->bytes_left = 0;
        data->last_mem = input.grab();
    }
    if (data->regex && (!data->last_index || data->bytes_left)) {
        OnigRegion* region = onig_region_new();
        size_t      ilen   = input.len();
        wstr        origin = input.data;
        cstr        i      = (cstr)&origin[data->last_index];
        cstr        e      = (cstr)&origin[ilen];

        /// iterate by cstr units of 1, as oni api uses these units
        for (size_t cursor = 0; ; cursor += region->end[0] + sizeof(wchar_t)) {
            if (cursor == ilen * sizeof(wchar_t))
                break;

            cstr s = &i[cursor]; /// we still use cstr in here because Oni uses +2 per char here
            int  r = onig_search(
                data->regex,
                (OnigUChar*)s, (OnigUChar*)e,
                (OnigUChar*)s, (OnigUChar*)e,
                region, ONIG_OPTION_NONE);
            
            if (r >= 0) {
                wstr start   = (utf16::char_t*)(s + region->beg[0]);
                wstr end     = (utf16::char_t*)(s + region->end[0]);
                utf16 match  = utf16(start, std::distance(start, end));
                result      += match;
                data->last_index = std::distance(i, (cstr)end + sizeof(wchar_t));
                data->bytes_left = std::distance((cstr)origin + data->last_index, (cstr)e);
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


array<str> RegEx::exec(str input) {
    Behaviour b = data->b;
    array<str> result;

    if (data->last_index == 0)
        data->bytes_left = 0;
    
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
                data->bytes_left = std::distance(origin + size_t(data->last_index), e);
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
