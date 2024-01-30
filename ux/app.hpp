#include <ux/ux.hpp>
#include <ux/canvas.hpp>
#include <ux/controls.hpp>
#include <media/streams.hpp>

namespace ion {

enums(AppType, undefined,
    undefined, VulkanOnly)

struct adata {
    composer::cmdata*   cmdata;
    MStream             media; // look!
    //GPU               win;
    Canvas             *canvas;
    vec2d               cursor;
    Element*            active;
    Element*            hover;
    VkEngine            e;
    lambda<node(struct App&)>  app_fn;
    lambda<bool(struct App&)>  loop_fn;
    map<mx>             args;
    Services            services;
    states<AppType>     app_type;
    ///
    type_register(adata);
};

struct App:composer {
    mx_object(App, composer, adata);

    App(map<mx> args, states<AppType> app_type, lambda<node(App&)> app_fn) : App() {
        data->args   = args;
        data->app_type = app_type;
        data->cmdata = composer::data; /// perhaps each data should be wrapped instance with an awareness of its peers
        data->app_fn = app_fn;
    }

    bool is_hovering(Element *e) {
        return data->hover == e;
    }

    bool is_active(Element *e) {
        return data->active == e;
    }

    void shell_server(uri url);
    int run();
    
    static void resize(vec2i &sz, App *app);

    operator int() { return run(); }

    operator bool() { return true; }

    mx operator[](symbol s) { return data->args[s]; }

    /// support 1 vec2, and 2 vec2's for a rectangular selection
    array<Element *> select_at(vec2d cur, bool active = true) {
        array<Element*> result = array<Element*>();
        lambda<void(Element*)> proc;
        proc = [&](Element *e) {
            array<Element*> inside = e->select([&](Element *ee) {
                node          &n = *(node*)ee;
                real           x = cur.x, y = cur.y;
                vec2d          o = ((Element*)n->parent)->offset(); // ee->offset(); // get parent offset
                vec2d        rel = cur - o;
                rectd    &bounds = ee->data->bounds;

                Element::props *eedata = ee->data;
                
                bool in = bounds.contains(rel);
                ee->data->cursor = rel; /// should set centrally once
                return (in && (!active || !eedata->active)) ? ee : null;
            });
            
            array<Element*> actives = e->select([&](Element *ee) -> Element* {
                return (active && ee->data->active) ? ee : null;
            });

            for (Element *i: inside)
                result += (Element*)i;
            
            for (Element *i: actives)
                result += (Element*)i;
        };
        /// work on single root vs array root
        if (composer::data->instances && composer::data->instances->data->children) {
            for (node *e: composer::data->instances->data->children)
                proc((Element*)e);
        } else if (composer::data->instances)
            proc((Element*)composer::data->instances);
        
        return result.reverse();
    }
    ///
};
}