#include <ux/app.hpp>

using namespace ion;

#include <stdio.h>
#include <string>
#include <oniguruma.h> // Include the Oniguruma header


extern "C" {
int onig_new(regex_t** reg, const UChar* pattern, const UChar* pattern_end,
         OnigOptionType option, OnigEncoding enc, OnigSyntaxType* syntax,
         OnigErrorInfo* einfo);
}

// Define the Rule structure to hold the Oniguruma regex and captures
struct Rule {
    OnigRegex match;  // Oniguruma regex
    var captures;     // Your captures data structure
    type_register(Rule);
};

array<Rule> read_grammar(path filePath) {
    array<Rule> rules;
    var js { filePath.read<var>() };

    auto read_patterns = [&](var& patterns) {
        for (var pattern: patterns.list()) {
            if (pattern.get("match")) {
                Rule          rule;
                var           match = pattern["match"];
                std::string   m = std::string(match);
                OnigRegex     regex;
                OnigErrorInfo einfo;
                ///
                int r = onig_new(&regex, (OnigUChar*)m.c_str(), (OnigUChar*)m.c_str() + m.length(),
                                 ONIG_OPTION_DEFAULT, ONIG_ENCODING_UTF8, ONIG_SYNTAX_DEFAULT, &einfo);
                if (r != ONIG_NORMAL) {
                    UChar s[ONIG_MAX_ERROR_MESSAGE_LEN];
                    onig_error_code_to_str(s, r, &einfo);
                    fprintf(stderr, "Regex compilation failed: %s\n", s);
                    // Handle the error here as needed
                }

                rule.match = regex;
                if (pattern.get("captures")) {
                    for (field<mx>& f : pattern["captures"].items()) {
                        var value = f.value;
                        rule.captures[f.key] = value["name"];
                    }
                }
                rules.push(rule);
            }
        }
    };

    read_patterns(js["patterns"]);
    if (js.get("repository")) {
        for (field<mx>& f : js["repository"].items()) {
            var item = f.value;
            if (item.get("patterns"))
                read_patterns(item["patterns"]);
        }
    }

    return rules;
}


array<str> apply_rules(str code, array<Rule>& rules) {
    array<str> categories { code.len(), code.len(), "unknown" };
    UChar* cp = (UChar*)code.cs();

    for (Rule& rule: rules) {
        OnigRegion* region  = onig_region_new();
        OnigUChar*  start   = cp;
        OnigUChar*  end     = cp + strlen((char*)cp);

        while (onig_search(rule.match, cp, end, start, end, region, ONIG_OPTION_NONE) >= 0) {

            for (field<mx> &capture : rule.captures.items()) {
                str skey = capture.key.grab();
                assert(skey.mem->type == typeof(char));
                int ikey = skey.integer_value();

                if (ikey < region->num_regs && region->beg[ikey] != ONIG_REGION_NOTPOS) {
                    // The capture key was matched
                    const OnigUChar* subMatchStart = cp + region->beg[ikey];
                    const OnigUChar* subMatchEnd = cp + region->end[ikey];

                    // Your logic here to determine the category for this capture
                    str category = capture.value.grab();

                    // Update the category for the matched portion
                    size_t s = subMatchStart - cp;
                    size_t count = subMatchEnd - subMatchStart;
                    for (int i = s, e = s + count; i < e; i++)
                        categories[i] = category;
                }
            }
            start = cp + region->end[0]; // Move to the end of the matched portion
        }

        onig_region_free(region, 1);
    }

    return categories;
}

struct View:Element {
    struct props {
        int         sample;
        int         sample2;
        int         test_set;
        callback    clicked;
        ///
        doubly<prop> meta() {
            return {
                prop { "sample",   sample   },
                prop { "sample2",  sample2  },
                prop { "test_set", test_set },
                prop { "clicked",  clicked  }
            };
        }
        type_register(props);
    };

    component(View, Element, props);

    node update() {
        return array<node> {
            Edit {
                { "content", "Multiline edit test" }
            }
        };
    }
};

int main(int argc, char *argv[]) {
    usleep(1000000);
    OnigRegex regex;
    OnigErrorInfo einfo;
    int r = onig_new(&regex, (OnigUChar*)"", (OnigUChar*)"",
                        ONIG_OPTION_DEFAULT, ONIG_ENCODING_UTF8, ONIG_SYNTAX_DEFAULT, &einfo);

    ion::path   grammarFile = "style/cpp.json";
    array<Rule> rules       = read_grammar(grammarFile);
    str         src         = "#include <stdio.h>\n\nint main() {\n\treturn 0;\n}\n";
    array<str>  categories  = apply_rules(src, rules);

    for (size_t i = 0; i < src.len(); ++i) {
        str cat = categories[i];
        std::cout << src[i] << ": " << cat << std::endl;
    }

    map<mx> defs { { "debug", uri { "ssh://ar-visions.com:1022" } } };
    map<mx> config { args::parse(argc, argv, defs) };
    if    (!config) return args::defaults(defs);
    ///
    return App(config, [](App &app) -> node {
        return View {
            { "id",      "main" },
            { "sample",  int(2) },
            { "sample2", "10"   },
            { "on-hover", callback([](event e) {
                console.log("hi");
            }) },
            { "clicked", callback([](event e) {
                printf("test!\n");
            }) }
        };
    });
}
