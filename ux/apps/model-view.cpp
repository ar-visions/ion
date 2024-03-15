#include <ux/app.hpp>

using namespace ion;

struct ModelView:Element {
    struct props {
        callback    clicked;
        ///
        doubly<prop> meta() {
            return {
                prop { "clicked", clicked}
            };
        }
        type_register(props);
    };

    component(ModelView, Element, props);
};

int main(int argc, char *argv[]) {
    map<mx> defs { { "debug", uri { "ssh://ar-visions.com:1022" } } };
    map<mx> config { args::parse(argc, argv, defs) };
    if    (!config) return args::defaults(defs);
    return App(config, [](App &app) -> node {
        return ModelView {
            { "id", "main" }
        };
    });
}
