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

/// Get RTC Stream going (compile all into singular module app in webrtc/apps)

/// that will stream views such as these that are registered components for web
/// better to declare them in code as such with the type registration
/// lookup parents and add to its children classes

struct View:node {
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

    component(View, node, props);

    void mounting() {
        console.log("mounting");
    }
 
    node update() {
        return array<node> {
            Edit {
                { "content", "VAVAVAVAVAVAVAVAVAVAVAVAVAVA" }
            }
        };
    }
};

int main() {
    return App([](App &ctx) -> node {
        return View {
            { "id",      "main" },
            { "sample",  int(2) },
            { "sample2", "10"   },
            { "on-hover", callback([](event e) {
                console.log("hi");
            }},
            { "clicked", callback([](event e) {
                printf("test!\n");
            }) }
        };
    });
}
