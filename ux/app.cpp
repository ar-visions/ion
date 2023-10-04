#include <ux/app.hpp>
#include <ssh/ssh.hpp>
#include <vkh/vkh.h>
#include <vkh/vkh_device.h>
#include <vkh/vkh_image.h>
#include <vkh/vkh_presenter.h>
//#include <vkg/vkg.hpp>
#include <camera/camera.hpp>

#ifdef __APPLE__
extern "C" void AllowKeyRepeats(void);
#endif

using namespace ion;

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

inline App app_data(mx &user) {
    return App(user);
}

static void key_callback(mx &user, int key, int scancode, int action, int mods) {
    App      app   = app_data(user);
    Element* root  = (Element*)app.composer::data->instances; /// for ux this is always single
    Element* focus = root->Element::data->focused;

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
            vkengine_close(app->e);
            break;
	}
    if (code && focus) {
        event e;
        e->text = str::from_code(code);
        focus->on_text(e);
    }
}

static void char_callback (mx &user, uint32_t c) {
    App app = app_data(user);
    Element* root  = (Element*)app.composer::data->instances;
    Element* focus = root->Element::data->focused;
    if (focus) {
        event e;
        e->text = str::from_code(c);
        focus->on_text(e);
    }
}

static void mouse_move_callback(mx &user, double x, double y) {
    App app = app_data(user);
    app->cursor = vec2d { x, y };// / app->data->e->vk_gpu->dpi_scale;

    for (Element* n: app->hover)
        n->Element::data->hover = false;
    
    app->hover = app.select_at(app->cursor, app->buttons[0]);
    Element *last = null;
    for (Element* n: app->hover) {
        n->Element::data->hover  = true;
        //n->Element::data->cursor = app->data->cursor; /// this type could be bool true when the point is in bounds
        last = n;
        if (last->data->selectable || last->data->editable) {
            /// compute sel start on mouse click down
            bool is_down = app->active[0];
            if (is_down) {
                last->data->sel_end = last->get_selection(app->cursor, is_down);
            }
        }
    }
}

static void scroll_callback(mx &user, double x, double y) {
    App app = app_data(user);
}

static void mouse_button_callback(mx &user, int button, int state, int mods) {
    App  app   = app_data(user);
    bool shift = mods & GLFW_MOD_SHIFT;

    app->buttons[button] = bool(state);
    Element* root = (Element*)app.composer::data->instances; /// for ux this is always single
    
    for (Element* n: app->active)
        n->Element::data->active = false;
    
    if (state)
        app->active = app.select_at(app->cursor, false);
    else
        app->active = {};

    Element* last = null;
    for (Element* n: app->active) {
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
        if (last->data->selectable || last->data->editable) {
            /// compute sel start on mouse click down
            bool    is_down = app->active[0];
            TextSel s = last->get_selection(app->cursor, is_down);

            /// we are either setting the sel_start, or sel_end (with a swap potential for out of sync selection)
            if (is_down) {
                /// if shift key, you alter between swapping start and end based on if its before or after
                last->data->sel_start = s;
                if (!shift)
                    last->data->sel_end = s;
            } else {
                last->data->sel_end   = s;
                if (!shift)
                    last->data->sel_start = s;
            }
        }
    }
}

void App::shell_server(uri bind) {
    ///
    async([&](runtime *rt, int i) -> mx {
        SSHService ssh;
        logger::service = [p=&ssh](mx msg) -> void {
            p->send_message(null, msg.grab());
        };
        data->services = Services({}, [&](Services &app) {
            return array<node> {
                SSHService {
                    { "id",   "ssh" },
                    { "bind",  bind },
                    { "ref",     lambda<void(mx)>           ([&](mx obj)                     { ssh = obj.grab(); })},
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

int App::run() {
    data->e = vkengine_create(1, 2, "ux",
        VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, VK_PRESENT_MODE_FIFO_KHR, VK_SAMPLE_COUNT_4_BIT,
        512, 512, 0, (mx&)*this);
    
    vk_engine_t *e = data->e;
    Canvas *canvas = data->canvas = new Canvas(e->renderer);

    /// allows keys to repeat on OSX, and thus allow glfw to function normally for UX
    /// todo: we still need backspace to repeat
    #ifdef __APPLE__
    AllowKeyRepeats();
    #endif

    vkengine_key_callback    (e,          key_callback);
	vkengine_button_callback (e, mouse_button_callback);
	vkengine_move_callback   (e,   mouse_move_callback);
    vkengine_char_callback   (e,         char_callback);
	vkengine_scroll_callback (e,       scroll_callback);
	vkengine_set_title       (e, "ux");

    /// works on macOS currently (needs to present, and update a canvas)
    //cameras  = array<Camera>(32);
    //cameras += Camera { e, 0, 1920, 1080, 30 };
    //cameras[0].start_capture();

    shell_server(data->args["debug"]);

	while (!vkengine_should_close(e)) {
        vkengine_poll_events(e);

        e->vk_device->mtx.lock();

        rgbad c = { 0.0, 0.0, 0.0, 1.0 };
        canvas->clear(c);

        node ee = data->app_fn(*this);

        update_all(ee);
        Element *eroot = (Element*)(
            composer::data->instances->data->children ?
            composer::data->instances->data->children[0] : 
            composer::data->instances);
        if (eroot) {
            /// update rect need to get eroot->children[0]
            eroot->data->bounds = rectd {
                0, 0,
                (real)canvas->get_virtual_width(),
                (real)canvas->get_virtual_height()
            };
            auto &bounds = eroot->data->bounds;
            eroot->draw(*canvas);
        }

        //rectd bounds = rectd { 0, 0, 64, 64 };
        //canvas->color({0.0, 0.0, 1.0, 1.0});
        //canvas->fill(bounds);

        canvas->flush();

        /// blt this to VkImage, bound to a canvas that we can perform subsequent drawing ops on
        /// canvas needs to draw other canvas too (Sk has this)
        ///
        //if (data->cameras[0].image)
        //    vkh_presenter_build_blit_cmd(data->e->renderer,
        //        data->cameras[0].image->image,
        //        data->cameras[0].width / 2, data->cameras[0].height / 2); 

        /// we need an array of renderers/presenters; must work with a 3D scene, bloom shading etc
		if (!vkh_presenter_draw(e->renderer)) {
            canvas->app_resize();
            vkDeviceWaitIdle(e->vkh->device);
		}
        e->vk_device->mtx.unlock();
        if (data->loop_fn)
            if (!data->loop_fn(*this)) {
                glfwDestroyWindow(e->window);
                break;
            }
	}

    //cameras[0].stop_capture();
    return 0;
}