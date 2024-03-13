#include <ux/app.hpp>
#include <ssh/ssh.hpp>
#include <ux/canvas.hpp>
#include <ux/webgpu.hpp>

#ifdef __APPLE__
extern "C" void AllowKeyRepeats(void);
#endif

using namespace ion;

namespace ion {

static cstr cstr_copy(cstr s) {
    size_t len = strlen(s);
    cstr     r = (cstr)malloc(len + 1);
    memcpy((void *)r, (void *)s, len + 1);
    return cstr(r);
}


static void glfw_error(int code, symbol cstr) {
    console.log("glfw error: {0}", { str(cstr) });
}

void App::resize(vec2i &sz, App *app) {
    printf("resized: %d, %d\n", sz.x, sz.y);
}

void App::shell_server(uri bind) {
    ///
    async([bind, data=this->data](runtime *rt, int i) -> mx {
        SSHService ssh;
        logger::service = [p=&ssh](mx msg) -> void {
            p->send_message(null, msg.hold());
        };
        data->services = Services({}, [&](Services &app) {
            return array<node> {
                SSHService {
                    { "id",   "ssh" },
                    { "bind",  bind },
                    { "ref",     lambda<void(mx)>           ([&](mx obj)                     { ssh = obj.hold(); })},
                    { "on-auth", lambda<bool(str, str, str)>([&](str id, str user, str pass) { return user == "admin" && pass == "admin"; })},
                    { "on-peer", lambda<void(SSHPeer)>      ([&](SSHPeer peer)               { console.log("peer connected: {0}", { peer->id }); })},
                    { "on-recv", lambda<void(SSHPeer, str)> ([&](SSHPeer peer, str msg) {
                        /// relay clients message back
                        ssh.send_message(peer, msg);

                        /// run method in scope of app?
                        /// use-case 1, set props: id.ClassNameWithoutId.another_id.prop = value
                        /// value can be "string", or string
                        /// value could be looked up with #id.here; dont see a lot of point here though.
                        /// we are setting its value by string, same way we do in css
                        /// once a variable is overridden it may be useful to keep it that way, composer-wise
                        /// we are debugging with a certain value so once set, it should stay.  no style or arg should override?
                        /// i just want to do this once though

                        /// use-case 2, get props:
                        /// id.ClassNameWithoutId.another_id.prop
                        /// we get the string from the type table to_string function

                        /// use-case 3, call method:
                        /// id.ClassNameWithoutId.another_id.prop()
                        /// we may technically want to insert this as a to-call, and have the composer call it AFTER updating?

                        /// 
                    })}
                }
            };
        });
        data->services.run();
        return true;
    });
    
}

static adata *_app_instance;

adata *app_instance() {
    return _app_instance;
}

struct CanvasAttribs {
    glm::vec4 pos;
    glm::vec2 uv;
    doubly<prop> meta() {
        return {
            { "pos", pos },
            { "uv",  uv  }
        };
    }
};

int App::run() {
    App &app = *this;

    _app_instance = data;
    data->window = Window::create("", vec2i {512, 512});

    Window &window  = data->window;
    Device  device  = window.device();
    vec2i   size    = window.size();
    Texture texture = device.create_texture(size, Asset::attachment);

    /// we call app_fn() right away to obtain app class
    node ee = data->app_fn(*this);
    style root_style { ee->type }; /// this is a singleton, so anyone else doing style s; will get the style. (those should not exec first, though.)
    window.set_title(ee->type->name);
    
    Pipes canvas_pipeline = Pipes(device, null, array<Graphics> {
        Graphics {
            "canvas", typeof(CanvasAttribs), { texture, Sampling::linear }, "canvas",
            [](mx &vbo, mx &ibo, array<image> &images) {
                static const uint32_t indexData[6] = {
                    0, 1, 2,
                    2, 1, 3
                };
                static const CanvasAttribs vertexData[4] = {
                    {{ -1.0f, -1.0f, 0.0f, 1.0f }, {  0.0f, 0.0f }},
                    {{  1.0f, -1.0f, 0.0f, 1.0f }, {  1.0f, 0.0f }},
                    {{ -1.0f,  1.0f, 0.0f, 1.0f }, { -1.0f, 1.0f }},
                    {{  1.0f,  1.0f, 0.0f, 1.0f }, {  1.0f, 1.0f }}
                };
                // set vbo and ibo
                ibo = mx::wrap<u32>((void*)indexData, 6);
                vbo = mx::wrap<CanvasAttribs>((void*)vertexData, 4);
            }
        }
    });
    
    Canvas canvas;
    
    /// calls OnWindowResize when registered
    window.register_presentation(
        /// presentation: returns the pipelines that renders canvas, also updates the canvas (test code)
        [&]() -> Scene {
            vec2i sz = canvas.size();
            rectd rect { 0, 0, sz.x, sz.y };
            canvas.color("#00f");
            canvas.fill(rect);

            rectd top { 0, 0, sz.x, sz.y / 2 };
            canvas.color("#ff0");
            canvas.fill(top);

            /*
            rgbad c = { 0.0, 0.0, 0.0, 1.0 }; /// todo: background-blur, 1 canvas for each gaussian() call
            canvas.clear(c);
            canvas.save();
            node ee = data->app_fn(*this);
            update_all(ee);
            Element *eroot = (Element*)(
                composer::data->instances->data->children ?
                composer::data->instances->data->children[0] : 
                composer::data->instances);
            if (eroot) {
                eroot->data->bounds = rectd {
                    0, 0,
                    (real)canvas.get_virtual_width(),
                    (real)canvas.get_virtual_height()
                };
                eroot->update_bounds(canvas);
                canvas.save();
                eroot->draw(canvas);
                canvas.restore();
            }*/
            canvas.flush();
            return Scene { canvas_pipeline };
        },
        /// size updated (recreates a texture from device, and canvas)
        [&](vec2i sz) {
            texture.resize(sz);
            canvas = Canvas(device, texture, true);
        });

    window.set_on_key_char([&](uint32_t c, int mods) {
        Element* root  = (Element*)app.composer::data->instances;
        Element* focus = root->Element::data->focused;
        if (focus) {
            event e;
            e->text = str::from_code(c);
            focus->on_text(e);
        }
    });

    window.set_on_cursor_enter([&](int enter) {
        auto cd = app.composer::data;
        if (enter == 1)
            return;
        /// only handling mouse leave, as we get a 'move' anyway for enter
        if (app->hover) {
            Element *n = app->hover;
            n->data->hover = false;
            app->hover = null;
            n->leave();
        }
    });

    window.set_on_cursor_pos([&](double x, double y) {
        auto cd = app.composer::data;
        app->cursor = vec2d { x, y };// / app->data->e->vk_gpu->dpi_scale;

        if (app->hover)
            app->hover->Element::data->hover = false;
        printf("play-pause hover = false\n");
        
        array<Element*> hovers = app.select_at(app->cursor, cd->buttons[0]);
        app->hover = hovers ? hovers[0] : null;

        Element *last = null;
        if (app->hover) {
            Element *n = app->hover;
            if (n->node::data->id == "play-pause") {
                printf("play-pause hover = true\n");
            }
            n->Element::data->hover  = true;
            last = n;
            if (last->data->selectable || last->data->editable) {
                /// compute sel start on mouse click down
                bool is_down = app->active != null;
                if (is_down) {
                    last->data->sel_end = last->get_selection(app->cursor, is_down);
                }
            }
            n->move();
        }
    });

    window.set_on_cursor_scroll([&](double x, double y) {
        if (app->hover) {
            app->hover->scroll(x, y);
        }
    });

    window.set_on_cursor_button([&](int button, int state, int mods) {
        auto cd    = app.composer::data;
        cd->buttons[button] = bool(state);
        
        Element* root = (Element*)app.composer::data->instances; /// for ux this is always single
        Element* prev_active = app->active;

        if (app->active)
            app->active->Element::data->active = false;
        
        if (state) {
            array<Element*> all_active = app.select_at(app->cursor, false);
            app->active = all_active ? all_active[0] : null;
        } else
            app->active = null;

        Element* last = null;
        if (app->active) {
            Element *n = app->active;
            n->Element::data->active = true;
            if (n->Element::data->tab_index >= 0)
                last = n;
        }
        Element *prev = root->Element::data->focused; // identifying your data members is important and you do that with member<> .... you can further meta describe in the type itself; you may also describe outside, but thats not strong because its distant from the implementation
        if (last && prev != last) {
            if (prev) {
                prev->Element::data->focus = false;
                prev->unfocused();
            }
            last->Element::data->focus = true;
            root->Element::data->focused = last;
            last->focused();
        }
        /// set mouse cursel on editable nodes
        if (last) {
            bool is_down = app->active[0];
            if (last->data->selectable || last->data->editable) {
                /// compute sel start on mouse click down
                TextSel s = last->get_selection(app->cursor, is_down);

                /// we are either setting the sel_start, or sel_end (with a swap potential for out of sync selection)
                if (is_down) {
                    /// if shift key, you alter between swapping start and end based on if its before or after
                    last->data->sel_start = s;
                    if (!cd->shift)
                        last->data->sel_end = s;
                } else {
                    last->data->sel_end   = s;
                    if (!cd->shift)
                        last->data->sel_start = s;
                }
            }
        }

        /// new down states
        if (app->active) {
            Element *n = app->active;
            if (prev_active != n) {
                n->down();
                if (n->data->ev.down) {
                    event e { n };
                    n->data->ev.down(e);
                }
            }
        }
        
        /// new up states & handle click
        if (prev_active && app->active != app->active) {
            Element *n = prev_active;
            n->up();
            if (n->data->ev.up) {
                event e { n };
                n->data->ev.up(e);
            }
            bool in_bounds = app->hover == n;
            if (in_bounds && n->data->ev.click) {
                event e { n };
                n->data->ev.click(e);
            }
        }
    });

    window.set_on_key_scancode([&](int key, int scancode, int action, int mods) {
        Element* root  = (Element*)app.composer::data->instances; /// for ux this is always single
        Element* focus = root->Element::data->focused;
        auto        cd = app.composer::data;

        cd->shift = mods & GLFW_MOD_SHIFT;
        printf("shift = %d\n", cd->shift);
        
        if (action != GLFW_PRESS && action != GLFW_REPEAT)
            return;
        
        int code = 0;
        switch (key) {
            case GLFW_KEY_ENTER: {
                code = 10;
                break;
            }
            case GLFW_KEY_SPACE: {
                // this is given as 'char' in glfw
                //code = 32
                break;
            }
            case GLFW_KEY_BACKSPACE: {
                code = 8;
                break;
            }
            case GLFW_KEY_ESCAPE:
                app->window.close();
                break;
        }
        if (code && focus) {
            event e;
            e->text = str::from_code(code);
            focus->on_text(e);
        }
    });

    //shell_server(data->args["debug"]);

    while (window.process()) {
        usleep(1);
	}
    return 0;
}

}