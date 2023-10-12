#define REGEX_IMPL
#include <regex/regex.hpp>

using namespace ion;

mx_implement(RegEx, mx)

/// currently 'behaviour' broadcast across all states
struct oniguruma {
    RegEx::Behaviour b;
    regex_t         *regex;
    mx               pattern; // construction pattern
    num              last_index;
    num              bytes_left;
};

struct iRegEx {
    oniguruma*       states;
    size_t           regex_count;
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
    load_patterns(&pattern, 1);
}

void RegEx::load_patterns(str *patterns, size_t len) {
    data->regex = (regex_t**)calloc(len, sizeof(regex_t*));
    for (size_t i = 0; i < len; i++) {
        str &pattern = patterns[i];
        int r = onig_new(
            &data->regex[i], (OnigUChar*)pattern.cs(),
            (OnigUChar*)(pattern.cs() + pattern.len()),
            ONIG_OPTION_DEFAULT, ONIG_ENCODING_UTF8, ONIG_SYNTAX_DEFAULT, &err);
        if (r != ONIG_NORMAL) {
            char estr[128];
            onig_error_code_to_str((UChar*)estr, r, &err);
            console.log("compilation failure for pattern: {0}\nerror:{1}", {pattern, str(estr)});
        }
    }
}

void RegEx::load_patterns(utf16 *patterns, size_t len) {
    OnigErrorInfo err;
    data->b = b;
    data->states = (oniguruma*)calloc(len, sizeof(oniguruma));
    for (size_t i = 0; i < len; i++) {
        utf16 &pattern = patterns[i];
        data->states[i].pattern = pattern.grab();
        int r = onig_new(
            &data->states[i].regex, (OnigUChar*)pattern.data,
            (OnigUChar*)(pattern.data + pattern.len()), /// i think its sized by 2?
            ONIG_OPTION_DEFAULT, ONIG_ENCODING_UTF16_LE, ONIG_SYNTAX_DEFAULT, &err);
        if (r != ONIG_NORMAL) {
            char estr[128];
            onig_error_code_to_str((UChar*)estr, r, &err);
            console.log("compilation failure for pattern: {0}\nerror:{1}", {str(pattern), str(estr)});
        }
    }
}

RegEx::RegEx(utf16 pattern, Behaviour b) : RegEx() {
    data->b = b;
    load_patterns(&pattern, 1);
}

RegEx::RegEx(array<utf16> patterns, Behaviour b) : RegEx() {
    data->b = b;
    load_patterns(patterns.data, patterns.len());
}

/// replace with above when tested
utf16 RegEx::replace(utf16 input, mx mx_replacement) {
    utf16 result = input;
    return result;
}

/// if there are multiple patterns this is used to query which specific one matched
/// we must alter
size_t RegEx::pattern_match() {
    return data->pattern_index;
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

    if (input.mem != data->last_mem || b == Behaviour::none) {
        ::drop(data->last_mem);
        for (size_t i = 0; i < regex_count; i++) {
            oniguruma *state = data->states[i];
            state->last_index = 0;
            state->bytes_left = 0;
        }
        data->last_mem = input.grab();
    }

    for (size_t i = 0; i < regex_count; i++) {
        array<utf16> result;
        oniguruma *state = data->states[i];
  
        if (state->regex && (!state->last_index || state->bytes_left)) {
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
                    state->last_index = std::distance(i, (cstr)end + sizeof(wchar_t));
                    state->bytes_left = std::distance((cstr)origin + data->last_index, (cstr)e);
                    continue;
                } else if (b == Behaviour::sticky || b == Behaviour::global_sticky) {
                    state->last_index = 0;
                    state->bytes_left = 0;
                }
                break;
            }
            onig_region_free(region, 1);
        }
        if (result)
            return result;
    }
    return {};
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
