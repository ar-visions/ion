#include <ux/ux.hpp>
#include <array>
#include <set>
#include <stack>
#include <queue>
#include <vkh/vkh.h>
#include <vkh/vkh_device.h>
#include <vkh/vkh_image.h>
#include <vkh/vkh_presenter.h>
//#include <vkg/vkg.hpp>
#include <camera/camera.hpp>
#include <ux/canvas.hpp>

namespace ion {

//static NewFn<path> path_fn = NewFn<path>(path::_new);
//static NewFn<array<rgba8>> ari_fn = NewFn<array<rgba8>>(array<rgba8>::_new);
//static NewFn<image> img_fn = NewFn<image>(image::_new);

struct color:mx {
    mx_object(color, mx, rgba8);
};

listener &dispatch::listen(callback cb) {
    data->listeners += new listener::ldata { this, cb };
    listener &last = data->listeners->last(); /// last is invalid offset

    last->detatch = fn_stub([
            ls_mem=last.mem, 
            listeners=data->listeners.data
    ]() -> void {
        size_t i = 0;
        bool found = false;
        for (listener &ls: *listeners) {
            if (ls.mem == ls_mem) {
                found = true;
                break;
            }
            i++;
        }
        console.test(found);
        listeners->remove(num(i)); 
    });

    return last;
}

void dispatch::operator()(event e) {
    for (listener &l: data->listeners)
        l->cb(e);
}

static symbol cstr_copy(symbol s) {
    size_t len = strlen(s);
    cstr     r = (cstr)malloc(len + 1);
    memcpy((void *)r, (void *)s, len + 1);
    return symbol(r);
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
    App app = app_data(user);
	if (action != GLFW_PRESS)
		return;
	switch (key) {
	case GLFW_KEY_SPACE:
		break;
	case GLFW_KEY_ESCAPE:
        vkengine_close(app->e);
		break;
	}
}

static void char_callback (mx &user, uint32_t c) {
    App app = app_data(user);
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
                if (shift) {
                    if (last->data->sel_prev == last->data->sel_end)
                        last->data->sel_end = last->data->sel_start;
                }
                last->data->sel_start = s;
                if (!shift)
                    last->data->sel_end = TextSel(s.copy());
                last->data->sel_prev  = last->data->sel_start;

            } else {
                if (shift) {
                    if (last->data->sel_prev == last->data->sel_start)
                        last->data->sel_start = last->data->sel_end;
                }
                last->data->sel_end   = s;
                if (!shift)
                    last->data->sel_start = TextSel(s.copy());
                last->data->sel_prev  = last->data->sel_end;
            }
        }
    }
}

/// streaming requires that we stage for encoding after presentation
/// do i render to texture, and then run through compression (yes.)
/// the alternate is rendering normally, then output to window as normal, but then you
/// capture the window separately
/// this works with third party apps too
/// works for win32, possibly mac cross platform
/// since we are not drawing to the screen, we should be wasting no time with this shim
/// it requires nvenc, a more standard api than the ones im pulling from nvpro

/// could in theory handle both ssh and telnet
void App::shell_server(uri bind) {
    sock::listen(bind, [this](sock &sc) {
        sc.send("hi\r\n", 4);
        ///
        char command[1024]; /// command buffer is 1024; just exit the session when it goes over no big deal
        char esc[32];
        int  cmd_len = 0;
        int  esc_len = 0;
        bool in_esc  = false;
        ///
        memset(command, 0, sizeof(command));
        memset(esc, 0, sizeof(esc));

        /// loop for client
        for (;;) {
            char byte;
            auto sz = sc.recv(&byte, 1);
            if (sz != 1) break;

            /// todo: reset escape sequence above when required
            if (byte == 0x1B) {
                in_esc = true;
                continue;
            }
            /// handle escape sequences
            else if (in_esc) {
                in_esc = (byte < 64 || byte > 126);
                if (in_esc) {
                    esc[esc_len++] = byte;
                    esc[esc_len]   = 0;
                } else {
                    /// handle escape sequence
                    printf("escape sequence: %s\n", esc);
                }
                continue;
            }
            /// handle commands (we want to handle binding class methods too)
            else if (byte == 8) {
                if (cmd_len) {
                    command[--cmd_len] = 0;
                }
            } else if (byte == 10 || byte == 13) {
                if (cmd_len) {
                    if (!sc.send(command, strlen(command)))
                        break;
                    ///
                    command[0] = 0;
                    cmd_len = 0;
                }
            } else {
                command[cmd_len++] = byte;
                command[cmd_len]   = 0;
                if (cmd_len == sizeof(cmd_len))
                    break;
            }
        }
        return true;
    });
}

int App::run() {
    data->e = vkengine_create(1, 2, "ux",
        VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, VK_PRESENT_MODE_FIFO_KHR, VK_SAMPLE_COUNT_4_BIT,
        512, 512, 0, (mx&)*this);
    
    vk_engine_t *e = data->e;
    Canvas *canvas = data->canvas = new Canvas(e->renderer);

    vkengine_key_callback    (e, key_callback);
	vkengine_button_callback (e, mouse_button_callback);
	vkengine_move_callback   (e, mouse_move_callback);
	vkengine_scroll_callback (e, scroll_callback);
	vkengine_set_title       (e, "ux");

    /// works on macOS currently (needs to present, and update a canvas)
    //cameras  = array<Camera>(32);
    //cameras += Camera { e, 0, 1920, 1080, 30 };
    //cameras[0].start_capture();

    shell_server("ssh://ar-visions.com");

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

/// place text before drawing? or, place text as we draw.
//void Element::layout()

void Element::focused()   { }
void Element::unfocused() { }

array<LineInfo> &Element::get_lines(Canvas *p_canvas) {
    bool  is_cache = data->cache_source.mem == data->content.mem;
    ///
    if (!is_cache) {
        str        s_content = data->content.grab();
        array<str> line_strs = s_content.split("\n");
        size_t    line_count = line_strs.len();
        ///
        data->lines = array<LineInfo>(line_count, line_count);
        ///
        size_t i = 0;
        for (LineInfo &l: data->lines) {
            l.bounds = {}; /// falsey rects have not been computed yet
            l.data   = line_strs[i];
            l.len    = l.data.len();
            l.adv    = data->font.advances(*p_canvas, l.data);
            l.bounds.w = l.adv[l.len - 1];
        }
    }
    return data->lines;
}

array<double> font::advances(Canvas& canvas, str text) {
    num l = text.len();
    array<double> res(l+1, l+1);
    ///
    for (num i = 0; i <= l; i++) {
        str    s   = text.mid(0, i);
        double adv = canvas.measure_advance(s.data, i);
        res[i]     = adv;
    }
    return res;
}

/// we draw the text and plot its placement; the text() function gives us back effectively aligned text
void Element::draw_text(Canvas& canvas, rectd& rect) {
    props::drawing &text = data->drawings[operation::text];
    canvas.save();
    canvas.color(text.color);
    canvas.opacity(effective_opacity());
    canvas.font(data->font);
    text_metrics tm = canvas.measure("W"); /// unit data for line
    const double line_height_basis = 2.0;
    double       lh = tm.line_height * (Element::data->text_spacing.y * line_height_basis); /// this stuff can all transition, best to live update the bounds
    ///
    array<LineInfo> &lines = get_lines(&canvas);

    /// iterate through lines
    int i = 0;
    int n = lines.len();

    /// if in center, the first line is going to be offset by half the height of all the lines n * (lh / 2)
    /// if bottom, the first is going to be offset by total height of lines n * lh

    /// for center y
    double total_h = lh * n;
    data->text_bounds.x = rect.x; /// each line is independently aligned
    data->text_bounds.y = rect.y + ((rect.h * text.align.y) - (total_h * text.align.y));
    data->text_bounds.w = rect.w;
    data->text_bounds.h = total_h;

    TextSel sel_start = data->sel_start;
    TextSel sel_end   = data->sel_end;

    if (sel_start > sel_end) {
        TextSel t = sel_start;
        sel_start = sel_end;
        sel_end   = t;
    }

    bool in_sel = false;
    for (LineInfo &line:lines) {
        /// implement interactive state to detect mouse clicks and set the cursor position
        line.bounds.x = data->text_bounds.x;
        line.bounds.y = data->text_bounds.y + i * lh;
        line.bounds.w = data->text_bounds.w; /// these are set prior
        line.bounds.h = lh;

        if (!in_sel && sel_start->row == i)
            in_sel = true;

        /// get more accurate placement rectangle
        canvas.color(text.color);
        canvas.text(line.data, line.bounds, text.align, {0.0, 0.0}, true, &line.placement);

        if (in_sel) {
            rectd sel_rect = line.bounds;
            str sel;
            num start = 0;

            sel_rect.w = 0;
            if (sel_start->row == i) {
                start      = sel_start->column;
                sel_rect.x = line.placement.x + line.adv[sel_start->column];
            } else {
                sel_rect.x = line.bounds.x;
            }

            if (sel_end->row == i) {
                num st = sel_start->row == i ? sel_start->column : 0;
                if (sel_start->row == i) {
                    sel_rect.x = line.placement.x + line.adv[sel_start->column];
                    if (sel_end->column == line.len)
                        sel_rect.w = (line.bounds.x + line.bounds.w) - sel_rect.x;
                } else {
                    if (sel_end->column == line.len)
                        sel_rect.w = (line.bounds.x + line.bounds.w) - sel_rect.x;
                    else
                        sel_rect.w = line.adv[sel_end->column];
                }
                sel = line.data.mid(start, sel_end->column - start);
            } else {
                sel_rect.w = line.bounds.w - sel_rect.x;
                sel = line.data;
            }
            if (sel_rect.w) {
                canvas.color(data->sel_background);
                canvas.fill(sel_rect);
                canvas.color(data->sel_color);
                canvas.text(sel, sel_rect, alignment { 0, 0 }, { 0.0, text.align.y }, true);
            } else {
                sel_rect.x -= 1;
                sel_rect.w  = 2;
                canvas.color(data->sel_background);
                canvas.fill(sel_rect);
            }
        }

        if (in_sel && sel_end->row == i)
            in_sel = false;

        /// correct these for line-height selectability
        line.placement.y = line.bounds.y;
        line.placement.h = lh;
        i++;
    }
    canvas.restore();
}

void Element::draw(Canvas& canvas) {
    type_t type = mem->type;
    node::edata *edata       = ((node*)this)->data;
    rect<r64>       bounds   = edata->parent ? data->bounds : rect<r64> { 
        0, 0, r64(canvas.get_virtual_width()), r64(canvas.get_virtual_height()) };
    props::drawing &fill     = data->drawings[operation::fill];
    props::drawing &image    = data->drawings[operation::image];
    props::drawing &text     = data->drawings[operation::text];
    props::drawing &outline  = data->drawings[operation::outline]; /// outline is more AR than border.  and border is a bad idea, badly defined and badly understood. outline is on the 0 pt.  just offset it if you want.
    props::drawing &children = data->drawings[operation::child]; /// outline is more AR than border.  and border is a bad idea, badly defined and badly understood. outline is on the 0 pt.  just offset it if you want.
    
    canvas.save();
    data->fill_bounds = fill.area.rect(bounds); /// use this bounds if we do not have specific areas defined
    
    /// if there is a fill color
    if (fill.color) { /// free'd prematurely during style change (not a transition)
        if (fill.radius) {
            vec4d r = fill.radius;
            vec2d tl { r.x, r.x };
            vec2d tr { r.y, r.y };
            vec2d br { r.w, r.w };
            vec2d bl { r.z, r.z };
            data->fill_bounds.set_rounded(tl, tr, br, bl);
        }
        canvas.save();
        canvas.color(fill.color);
        canvas.opacity(effective_opacity());
        canvas.fill(data->fill_bounds);
        canvas.restore();
    }

    /// if there is fill image
    if (image.img) {
        image.shape = image.area.rect(bounds);
        canvas.save();
        canvas.opacity(effective_opacity());
        canvas.image(image.img, image.shape, image.align, {0,0});
        canvas.restore();
    }
    
    /// if there is text (its not alpha 0, and there is text)
    if (data->content && ((data->content.type() == typeof(char)) ||
                          (data->content.type() == typeof(str)))) {
        rectd rect = text.area ? text.area.rect(bounds) : data->fill_bounds;
        draw_text(canvas, rect);
        text.shape = rect;
    }

    /// if there is an effective border to draw
    if (outline.color && outline.border.size > 0.0) {
        rectd outline_rect = outline.area ? outline.area.rect(bounds) : data->fill_bounds;
        if (outline.radius) {
            vec4d r = outline.radius;
            vec2d tl { r.x, r.x };
            vec2d tr { r.y, r.y };
            vec2d br { r.w, r.w };
            vec2d bl { r.z, r.z };
            outline_rect.set_rounded(tl, tr, br, bl);
        }
        canvas.color(outline.border.color);
        canvas.opacity(effective_opacity());
        canvas.outline_sz(outline.border.size);
        canvas.outline(outline_rect); /// this needs to work with vshape, or border
        outline.shape = outline_rect; /// updating for interactive logic
    }

    for (node *n: edata->mounts) {
        Element* c = (Element*)n;
        /// clip to child
        /// translate to child location
        rectd sub_bounds = children.area ? children.area.rect(bounds) : data->fill_bounds;
        canvas.translate(sub_bounds.xy());
        canvas.opacity(effective_opacity());
        (*c)->bounds = rect<r64> { 0, 0, sub_bounds.w, sub_bounds.h };
        //canvas.clip(0, 0, r.w, r.h);
        c->draw(canvas);
    }

    canvas.restore();
}

}
