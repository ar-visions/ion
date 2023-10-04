#include <ux/app.hpp>

using namespace ion;

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
