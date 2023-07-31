#include <mx/mx.hpp>
#include <async/async.hpp>
#include <net/net.hpp>
#include <image/image.hpp>
#include <media/media.hpp>
#include <ux/ux.hpp>

using namespace ion;

/// ------------------------------------------------------------
/// audio annotation app first
/// ------------------------------------------------------------

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

    component(View, node, props);

    void mounting() {
        console.log("mounting");
    }

    /// button should have blue bg and white text in style css
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
    /// use css for setting most of these properties.
    return App([](App &ctx) -> Element {
        return View {
            { "id",      "main" }, /// sets id on the base node data; if there is no id then it should identify by its type
            { "sample",  int(2) },
            { "sample2", "10"   }, /// converts to int from char* or str
            { "clicked", callback([](event e) { /// dont access state in here! lol.
                printf("test!\n");
                int test = 0;
                test++;
            }) }
        };
    });
}
