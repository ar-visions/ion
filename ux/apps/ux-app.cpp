#include <ux/app.hpp>

using namespace ion;

/// @brief
/// ux apps are both Canvas and 3D Scene.
/// the 3D can be reality or a game or a picture of both.
/// its a spatial app construct but thats also mostly 
/// compatible with any kind of app or game in the above
/// described contexts

struct AppTest:Element {
    struct props {
        callback    clicked;
        ///
        doubly<prop> meta() {
            return {
                prop { "clicked", clicked}
            };
        }
    };

    component(AppTest, Element, props);
};

int main(int argc, char *argv[]) {
    map<mx> defs { { "debug", uri { "ssh://ar-visions.com:1022" } } };
    map<mx> config { args::parse(argc, argv, defs) };
    if    (!config) return args::defaults(defs);
    return App(config, [](App &app) -> node {
        return AppTest {
            { "id", "main" }
        };
    });
}
