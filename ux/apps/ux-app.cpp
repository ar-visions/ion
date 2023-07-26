#include <mx/mx.hpp>
#include <async/async.hpp>
#include <net/net.hpp>
#include <math/math.hpp>
#include <media/media.hpp>
#include <ux/ux.hpp>

using namespace ion;

/// ------------------------------------------------------------
/// audio annotation app first
/// ------------------------------------------------------------

struct ux_view:node {
    struct props {
        int sample;
        int sample2;
        callback handler;
        ///
        doubly<prop> meta() {
            return {
                prop { "sample",  sample },
                prop { "sample2", sample2 },
                prop { "handler", handler}
            };
        }
        type_register(props);
    };

    component(ux_view, node, props);

    void mounting() {
        console.log("mounting");
    }

    Element update() {
        int test = 0;
        test++;
        return button {
            { "content", fmt {"hello world: {0}", { state->sample }} },
            { "on-click",
                callback([&](event e) {
                    console.log("on-click...");
                    if (state->handler)
                        state->handler(e);
                })
            }
        };
    }
};

int main() {
    return app([](app &ctx) -> Element {
        return ux_view {
            arg { "id",      "main" },
            arg { "sample",  int(2) },
            arg { "sample2", "10"   }
        };
    });
}
