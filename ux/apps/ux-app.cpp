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
        callback handler;
        ///
        doubly<prop> meta() {
            return {
                prop { "sample",  sample },
                prop { "handler", handler}
            };
        }
    };

    component(ux_view, node, props);

    void mounting() {
        console.log("mounting");
    }

    Element render() {
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
            { "id",     "main"  },
            { "sample",  int(2) }
        };
    });
}
