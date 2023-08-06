#include <mx/mx.hpp>
#include <async/async.hpp>
#include <net/net.hpp>
#include <image/image.hpp>
#include <media/media.hpp>
#include <ux/ux.hpp>
#include <camera/camera.hpp>

using namespace ion;

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

    Element update() {
        return Button {
            { "content", fmt {"hello world: {0}", { state->sample }} },
            { "on-click",
                callback([&](event e) {
                    console.log("on-click...");
                    if (state->clicked)
                        state->clicked(e);
                })
            }
        };
    }
};

int main() {
    return App([](App &ctx) -> Element {
        return View {
            { "id",      "main" },
            { "sample",  int(2) },
            { "sample2", "10"   },
            { "clicked", callback([](event e) {
                printf("test!\n");
            }) }
        };
    });
}
