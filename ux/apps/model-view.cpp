#include <ux/app.hpp>

using namespace ion;

struct ModelView:Element {
    struct props {
        callback    clicked;
        ///
        properties meta() {
            return {
                prop { "clicked", clicked}
            };
        }
    };

    component(ModelView, Element, props);
};

int main(int argc, char *argv[]) {
    map defs { { "debug", uri { "ssh://ar-visions.com:1022" } } };
    map config { args::parse(argc, argv, defs) };
    if    (!config) return args::defaults(defs);
    return App(config, [](App &app) -> node {
        return ModelView {
            { "id", "main" }
        };
    });
}
