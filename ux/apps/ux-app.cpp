struct abc1 { int abc; };

static abc1 *abc = new abc1 { };

#include <mx/mx.hpp>
#include <async/async.hpp>
#include <net/net.hpp>
#include <image/image.hpp>
#include <media/media.hpp>
#include <ux/ux.hpp>
#include <camera/camera.hpp>

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

    int context_var;

    doubly<prop> meta() {
        return {
            prop { "context_var", context_var }
        };
    }

    component(View, Element, props);

    void mounting() {
        console.log("mounting");
    }
 
    node update() {
        return array<node> {
            Edit {
                { "content", "Multiline edit test\nAnother line" }
            }
        };
    }
};

struct test11 {
    int a = 1;
    test11() { }
    int method(int v) {
        printf("a = %d\n", a);
        return 1;
    }
};

int main(int argc, char *argv[]) {

    test11 t1;
    t1.a = 5;
    lambda<int(int)> test = lambda<int(int)>(&t1, &test11::method);

    test(1); // should print a = 6

    /*
    using ltype = lambda<int(int, short, protocol)>;

    ltype test = [](int a, short b, protocol c) -> int {
        printf("a = %d\n", (int)a);
        printf("b = %d\n", (int)b);
        printf("c = %s\n", str(c).cs());
        return 4;
    };

    array<str> args = {"1", "2", "ssh"};
    mx result       = call(test, args);

    int ires = int(result);
    */


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
