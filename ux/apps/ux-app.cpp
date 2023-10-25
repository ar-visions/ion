#include <ux/app.hpp>

using namespace ion;

/// @brief ux apps are both gfx and 3D context.  the 3D background can be reality or a game or a picture of both.

/// all of the scenes are staged differently
/// issue is accessing the members of their View, perhaps not a deal at all?
/// 
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

    component(View, Element, props);
};

int main(int argc, char *argv[]) {
    map<mx> defs { { "debug", uri { "ssh://ar-visions.com:1022" } } };
    map<mx> config { args::parse(argc, argv, defs) };
    if    (!config) return args::defaults(defs);
    return App(config, [](App &app) -> node {
        return View {
            { "id", "main" }
        };
    });
}
