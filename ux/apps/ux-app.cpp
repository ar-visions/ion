#include <ux/app.hpp>

using namespace ion;



struct View:Element {
    struct props {
        int         sample;
        int         sample2;
        callback    clicked;
        ///
        doubly<prop> meta() {
            return {
                prop { "sample",  sample },
                prop { "sample2", sample2 },
                prop { "clicked", clicked}
            };
        }
        type_register(props);
    };

    int context_var;

    doubly<prop> meta() {
        return {
            prop { "context_var", context_var }
        };
    }

    component(View, Element, props);

    void mounted() {
        console.log("mounted");
    }
 
    node update() {
        return ion::array<node> {
            Edit {
                { "content", "Multiline edit test" }
            }
        };
    }
};

struct Test3:mx {
    struct members {
        int t3_int;
        register(members)
        doubly<prop> meta() {
            return {
                prop { "t3_int", t3_int }
            };
        }
    };
    mx_basic(Test3);
};

struct Test2:mx {
    struct members {
        int test2_int;
        array<Test3> test3_values;
        register(members)
        doubly<prop> meta() {
            return {
                prop { "test2_int",    test2_int    },
                prop { "test3_values", test3_values }
            };
        }
    };
    mx_basic(Test2);
};

struct Test1:mx {
    struct members {
        str   str_value;
        int   int_value;
        short short_value;
        bool  bool_value;
        Test2 test2_value;
        register(members)
        doubly<prop> meta() {
            return {
                prop { "str_value",   str_value   },
                prop { "int_value",   int_value   },
                prop { "short_value", short_value },
                prop { "bool_value",  bool_value  },
                prop { "test2_value", test2_value }
            };
        }
    };
    mx_basic(Test1);
};

int main(int argc, char *argv[]) {
    array<int> i_array;
    Test1 t1;

    t1.set_meta("str_value", mx(str("test")));

    str s0 = t1->str_value;

    path  p  = "test1.json";
    Test1 t11 = p.read<Test1>(); /// test2

    Test2 t2a;
    Test3 &t3v = t11->test2_value->test3_values[0];

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
