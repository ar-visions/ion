#include <ux/ux.hpp>
#include <ux/canvas.hpp>
#include <ux/controls.hpp>

namespace ion {
struct App:composer {
    struct adata {
        composer::cmdata*   cmdata;
        //array<Camera>     cameras;
        //GPU               win;
        Canvas             *canvas;
        vec2d               cursor;
        bool                buttons[16];
        array<Element*>     active;
        array<Element*>     hover;
        VkEngine            e;
        lambda<node(App&)>  app_fn;
        lambda<bool(App&)>  loop_fn;
        map<mx>             args;
        Services            services;
        ///
        type_register(adata);
    };

    mx_object(App, composer, adata);

    App(map<mx> args, lambda<node(App&)> app_fn) : App() {
        data->args   = args;
        data->cmdata = composer::data; /// perhaps each data should be wrapped instance with an awareness of its peers
        data->app_fn = app_fn;
    }

    bool is_hovering(Element *e) {
        return data->hover.index_of(e) >= 0;
    }

    bool is_active(Element *e) {
        return data->active.index_of(e) >= 0;
    }

    void shell_server(uri url);
    int run();
    
    static void resize(vec2i &sz, App *app);

    operator int() { return run(); }

    operator bool() { return true; }

    mx operator[](symbol s) { return data->args[s]; }

    ///
    array<Element *> select_at(vec2d cur, bool active = true) {
        array<Element*> result = array<Element*>();
        auto proc = [&](Element *e) {
            array<Element*> inside = e->select([&](Element *ee) {
                real           x = cur.x, y = cur.y;
                vec2d          o = ee->offset();
                vec2d        rel = cur + o;
                Element::props *edata = e->data;
                rectd &bounds = ee->data->fill_bounds ? ee->data->fill_bounds : ee->data->bounds;
                bool in = bounds.contains(rel);
                e->data->cursor = vec2d(x - o.x, y - o.y);
                return (in && (!active || !edata->active)) ? ee : null;
            });
            array<Element*> actives = e->select([&](Element *ee) -> Element* {
                return (active && ee->data->active) ? ee : null;
            });
            for (Element *i: inside)
                result += (Element*)i;
            for (Element *i: actives)
                result += (Element*)i;
        };

        if (composer::data->instances && composer::data->instances->data->children) {
            for (node *e: composer::data->instances->data->children)
                proc((Element*)e);
        } else if (composer::data->instances)
            proc((Element*)composer::data->instances);
        
        return result;
    }
    ///
};
}