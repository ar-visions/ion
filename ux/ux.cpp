#include <ux/ux.hpp>
#include <array>
#include <set>
#include <stack>
#include <queue>
#include <vkh/vkh.h>
#include <vkh/vkh_device.h>
#include <vkh/vkh_image.h>
#include <vkg/vkg.hpp>

namespace ion {

//static NewFn<path> path_fn = NewFn<path>(path::_new);
//static NewFn<array<rgba8>> ari_fn = NewFn<array<rgba8>>(array<rgba8>::_new);
//static NewFn<image> img_fn = NewFn<image>(image::_new);

struct color:mx {
    mx_object(color, mx, rgba8);
};

struct surface:mx {
    struct sdata {
        VkvgSurface surf;
        type_register(sdata);
    };
    ///
    surface(VkvgDevice device, image img) {
        data->surf = vkvg_surface_create_from_bitmap(device, (u8*)img.data, img.width(), img.height());
    }
    ///
    operator bool() {
        return data->surf;
    }
    ///
    mx_object(surface, mx, sdata);
};

struct gfx_memory {
    VkvgSurface    vg_surface;
    VkvgDevice     vg_device;
    VkvgContext    ctx;
    VkhPresenter   vkh_renderer;
    VkhImage       vkh_image;
    Device         device;
    str            font_default;
    VkEngine       e;
    u32            width, height;

    struct state {
        vkvg_matrix_t    mat;
        uint32_t         color;
        ion::font        font;
        double           opacity;
        ion::surface     surface;
        rectd            clip;
        double           line_width;
        double           line_height;
        vkvg_line_cap_t  line_cap;
        vkvg_line_join_t line_join;
        /// needs surface / color indicator for source type (surface & color are both mx types)
    };

    doubly<state> stack;
    state        *top;
    
    ~gfx_memory() {
        if (e)            vkDeviceWaitIdle(e->vkh->device);
        if (vg_device)    vkvg_device_drop(vg_device);
        if (vg_surface)   vkvg_surface_drop(vg_surface);
        if (vkh_renderer) vkh_presenter_drop(vkh_renderer);
        if (e)            vkengine_drop(e);
    }
    type_register(gfx_memory);
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

struct memory;
struct Texture;

mx_implement(gfx, mx);

gfx::gfx(VkEngine e, VkhPresenter vkh_renderer) : gfx() { /// this allocates both gfx_memory and cbase::cdata memory (cbase has data type aliased at cbase::DC)
    data->e             = e;
    data->device        = e->vk_device;
    data->vkh_renderer  = vkh_renderer;

    /// create vkvg surface with vkh image and instance a context
	data->vg_device = vkvg_device_create(e, VK_SAMPLE_COUNT_4_BIT, false);

	vkvg_device_set_dpy(data->vg_device, 96, 96);
	vkvg_device_set_thread_aware(data->vg_device, 1);

    /// create surface, image and presenter
    resized();
}

void gfx::defaults() {
    vkvg_identity_matrix(data->ctx);
    vkvg_get_matrix(data->ctx, &data->top->mat);
    vkvg_reset_clip(data->ctx);

    data->top->clip        = rectd { 0, 0, 0, 0 }; /// null state == no clip
    data->top->color       = 0xff000000;
    data->top->opacity     = 1.0;
    data->top->line_cap    = VKVG_LINE_CAP_BUTT;
    data->top->line_join   = VKVG_LINE_JOIN_MITER;
    data->top->line_width  = 1.0;
    data->top->line_height = 1.0;
    vkvg_set_source_color(data->ctx, 0x00);
    vkvg_set_line_cap    (data->ctx, VKVG_LINE_CAP_BUTT);
    vkvg_set_line_join   (data->ctx, VKVG_LINE_JOIN_MITER);
    vkvg_set_line_width  (data->ctx, data->top->line_width);
    vkvg_set_opacity     (data->ctx, data->top->opacity);
}

void gfx::identity() {
    vkvg_identity_matrix(data->ctx);
}

void gfx::push() {
    gfx_memory::state &st = data->stack->push();
    st        = *data->top;
    data->top = &data->stack->last();
    ///
    vkvg_get_matrix(data->ctx, &data->top->mat);
    data->top->line_cap   = vkvg_get_line_cap  (data->ctx);
    data->top->line_join  = vkvg_get_line_join (data->ctx);
    data->top->line_width = vkvg_get_line_width(data->ctx);
}

void gfx::pop() {
    data->stack->pop();
    data->top = &data->stack->last();
    ///
    vkvg_set_source_color(data->ctx,  data->top->color);
    vkvg_set_matrix      (data->ctx, &data->top->mat);
    vkvg_new_path        (data->ctx); /// ??
    vkvg_set_line_cap    (data->ctx, data->top->line_cap);
    vkvg_set_line_join   (data->ctx, data->top->line_join);
    vkvg_set_line_width  (data->ctx, data->top->line_width);
}

void gfx::resized() {
    if (data->vg_surface)
        vkvg_surface_drop(data->vg_surface);
    if (data->vkh_image)
        vkh_image_drop(data->vkh_image);

    /// these should be updated (VkEngine can have VkWindow of sort eventually if we want multiple)
    float sx, sy;
    vkh_presenter_get_size(data->vkh_renderer, &data->width, &data->height, &sx, &sy); /// vkh should have both vk engine and glfw facility
    data->width  /= sx; /// we use the virtual size here
    data->height /= sy;

    /// canvas w and h is virtual pixels; like dom and things; all user-mode stuff on top of vkvg is virtual
	data->vg_surface = vkvg_surface_create(data->vg_device, data->width, data->height); 
	data->vkh_image  = vkvg_surface_get_image(data->vg_surface);

    vkh_presenter_build_blit_cmd(data->vkh_renderer, data->vkh_image->image, data->width, data->height);
    

    assert(data->stack->len() <= 1);

    while (data->stack)
        data->stack->pop();
    data->stack->push();
    data->top = &data->stack->last();
}

void vkvg_path(VkvgContext ctx, memory *mem) {
    type_t ty = mem->type;
    
    if (ty == typeof(rectd)) {
        rectd &m = mem->ref<rectd>();
        vkvg_rectangle(ctx, m.x, m.y, m.w, m.h);
    }
    else if (ty == typeof(Rounded<double>)) {
        Rounded<double>::rdata &m = mem->ref<Rounded<double>::rdata>();
        vkvg_move_to (ctx, m.tl_x.x, m.tl_x.y);
        vkvg_line_to (ctx, m.tr_x.x, m.tr_x.y);
        vkvg_curve_to(ctx, m.c0.x,   m.c0.y, m.c1.x, m.c1.y, m.tr_y.x, m.tr_y.y);
        vkvg_line_to (ctx, m.br_y.x, m.br_y.y);
        vkvg_curve_to(ctx, m.c0b.x,  m.c0b.y, m.c1b.x, m.c1b.y, m.br_x.x, m.br_x.y);
        vkvg_line_to (ctx, m.bl_x.x, m.bl_x.y);
        vkvg_curve_to(ctx, m.c0c.x,  m.c0c.y, m.c1c.x, m.c1c.y, m.bl_y.x, m.bl_y.y);
        vkvg_line_to (ctx, m.tl_y.x, m.tl_y.y);
        vkvg_curve_to(ctx, m.c0d.x,  m.c0d.y, m.c1d.x, m.c1d.y, m.tl_x.x, m.tl_x.y);
    }
    else if (ty == typeof(graphics::shape)) {
        graphics::shape::sdata &m = mem->ref<graphics::shape::sdata>();
        for (mx &o:m.ops) {
            type_t t = o.type();
            if (t == typeof(Movement)) {
                Movement m(o);
                vkvg_move_to(ctx, m->x, m->y);
            } else if (t == typeof(Vec2<double>)) {
                Vec2<double> l(o);
                vkvg_line_to(ctx, l->x, l->y);
            }
        }
        vkvg_close_path(ctx);
    }
}

VkEngine gfx::engine() { return data->e; }
Device   gfx::device() { return data->device; }

text_metrics gfx::measure(str text) {
    vkvg_text_extents_t ext;
    vkvg_font_extents_t tm;
    ///
    vkvg_text_extents(data->ctx, (symbol)text.cs(), &ext);
    vkvg_font_extents(data->ctx, &tm);
    return text_metrics::tmdata {
        .w           =  real(ext.x_advance),
        .h           =  real(ext.y_advance),
        .ascent      =  real(tm.ascent),
        .descent     =  real(tm.descent),
        .line_height =  real(data->top->line_height)
    };
}

str gfx::format_ellipsis(str text, real w, text_metrics &tm_result) {
    symbol       t  = (symbol)text.cs();
    text_metrics tm = measure(text);
    str result;
    ///
    if (tm->w <= w)
        result = text;
    else {
        symbol e       = "...";
        size_t e_len   = strlen(e);
        r32    e_width = measure(e)->w;
        size_t cur_w   = 0;
        ///
        if (w <= e_width) {
            size_t len = text.len();
            while (len > 0 && tm->w + e_width > w)
                tm = measure(text.mid(0, int(--len)));
            if (len == 0)
                result = str(cstr(e), e_len);
            else {
                cstr trunc = (cstr)malloc(len + e_len + 1);
                strncpy(trunc, text.cs(), len);
                strncpy(trunc + len, e, e_len + 1);
                result = str(trunc, len + e_len);
                free(trunc);
            }
        }
    }
    tm_result = tm;
    return result;
}

void gfx::draw_ellipsis(str text, real w) {
    text_metrics tm;
    str draw = format_ellipsis(text, w, tm);
    if (draw)
        vkvg_show_text(data->ctx, (symbol)draw.cs());
}

void gfx::image(ion::image img, graphics::shape sh, alignment align, vec2d offset, vec2d source) {
    attachment *att = img.find_attachment("vg-surf");
    if (!att) {
        VkvgSurface surf = vkvg_surface_create_from_bitmap(
            data->vg_device, (uint8_t*)img.pixels(), u32(img.width()), u32(img.height()));
        att = img.attach("vg-surf", surf, [surf]() {
            vkvg_surface_drop(surf);
        });
        assert(att);
    }
    VkvgSurface surf = (VkvgSurface)att->data;
    gfx_memory::state &top = *data->top;
    ion::size     sz = img.shape();
    rectd           &r = sh.bounds();
    assert(surf);

    /// now its just of matter of scaling the little guy to fit in the box.
    vec2d     vsc = { math::min(1.0, r.w / real(sz[1])), math::min(1.0, r.h / real(sz[0])) };
    real       sc = (vsc.y > vsc.x) ? vsc.x : vsc.y;
    vec2d     pos = { mix(r.x, r.x + r.w - sz[1] * sc, align.x),
                      mix(r.y, r.y + r.h - sz[0] * sc, align.y) };
    
    push();
    /// translate & scale
    translate(pos + offset);
    scale(sc);
    /// push path
    vkvg_rectangle(data->ctx, r.x, r.y, r.w, r.h);
    /// color & fill
    vkvg_set_source_surface(data->ctx, surf, source.x, source.y);
    vkvg_fill(data->ctx);
    pop();
}

/// would be reasonable to have a rich() method
/// the lines are most definitely just text() calls, it should be up to the user to perform multiline.
///
/// ellipsis needs to be based on align
/// very important to make the canvas have good primitive ops, to allow the user 
/// to leverage high drawing abstracts direct from canvas, make usable everywhere!
///
/// rect for the control area, region for the sizing within
/// important that this info be known at time of output where clipping and ellipsis is concerned

/// a background color would be nice that uses a sort of line height
/// that line-height we would neeed to pass into this function
/// it would just be nice to make things easier at the primitive level for text, as text ops are
/// often repetitive and giving more functionality would improve quality across apps
void gfx::text(str text, Rect<double> rect, alignment align, vec2d offset, bool ellip) {
    gfx_memory::state &top = *data->top;
    ///
    vec2d          pos { 0, 0 };
    text_metrics   tm;
    str            txt;
    ///
    if (ellip) {
        txt = format_ellipsis(text, rect->w, tm);
        /// with -> operator you dont need to know the data ref.
        /// however, it looks like a pointer.  the bigger the front, the bigger the back.
        /// in this case.  they equate to same operations
    } else {
        txt = text;
        tm  = measure(txt);
    }

    /// its probably useful to have a line height in the canvas
    real line_height = (-tm->descent + tm->ascent) / 1.5;

    push();
    real text_x = rect->x + (align.x * (rect->w - tm->w));
    real text_y = rect->y + (line_height + (align.y * (rect->h - tm->h)));
    translate(vec2d { text_x, text_y });
    vkvg_show_text(data->ctx, (symbol)text.cs()); /// we need a custom shader here for accelerated drawing.  that is, not render passes per character color but rather just vertices that contain everything we need
    pop();
}

// supporting only rect clips for the time being
void gfx::clip(graphics::shape cl) {
    data->top->clip = cl.bounds();
    rectd &r = data->top->clip;
    vkvg_rectangle(data->ctx, r.x, r.y, r.w, r.h);
    vkvg_clip(data->ctx);
}

VkhImage gfx::texture() {
    return data->vkh_image;
} /// associated with surface

void gfx::flush() {
    vkvg_flush(data->ctx);
}

void gfx::clear(rgba8 c) {
    vkvg_paint(data->ctx);
}

void gfx::opacity(real o) {
    vkvg_set_opacity(data->ctx, o);
}

void gfx::font(ion::font &f) {
    data->top->font = f;
    path fpath = f.get_path();
    assert(fpath.exists());

    if (!f->loaded) {
        f->loaded = true;
        vkvg_load_font_from_path(data->ctx, fpath.cs(), f->name.cs());
    }
    
    vkvg_select_font_face(data->ctx, f->name.cs());
    vkvg_set_font_size(data->ctx, f->sz);
}

void gfx::cap(graphics::cap c) {
    vkvg_set_line_cap (data->ctx, vkvg_line_cap_t (int(c)));
}

void gfx::join (graphics::join j) {
    vkvg_set_line_join(data->ctx, vkvg_line_join_t(int(j)));
}

void gfx::translate(vec2d tr) {
    vkvg_translate(data->ctx, tr.x, tr.y);  }

void gfx::scale(vec2d sc) {
    vkvg_scale(data->ctx, sc.x, sc.y);
}

void gfx::scale(real sc) {
    vkvg_scale(data->ctx, sc, sc);
}

void gfx::rotate(real degs) {
    vkvg_rotate(data->ctx, radians(degs));
}

void gfx::color(rgba8 &c) {
    vkvg_set_source_rgba(data->ctx, c.r / 255.0, c.g / 255.0, c.b / 255.0, c.a / 255.0);
}

void gfx::fill(rectd &p) {
    vkvg_rounded_rectangle(data->ctx, p.x, p.y, p.w, p.h, p.r_tl.x); /// vkvg needs a 4 component rounded rect as described in ux
    vkvg_fill(data->ctx);
}

void gfx::fill(graphics::shape p) {
    vkvg_path(data->ctx, p.mem);
    vkvg_fill(data->ctx);
}

void gfx::gaussian(vec2d sz, graphics::shape c) { }

void gfx::outline_sz(real line_width) {
    data->top->line_width = line_width;
}

void gfx::outline(graphics::shape p) {
    vkvg_path(data->ctx, p.mem);
    vkvg_stroke(data->ctx);
}

ion::image gfx::resample(ion::size sz, real deg, graphics::shape view, vec2d axis) {
    rgba8 *pixels = null;
    int scanline = 0;
    return ion::image(sz, (rgba8*)pixels, scanline);
}

void App::resize(vec2i &sz, App *app) {
    printf("resized: %d, %d\n", sz.x, sz.y);
}

App *app_data(GLFWwindow *window) {
    return (App*)glfwGetWindowUserPointer(window);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    App *app = app_data(window);
    App::adata *data = app->data;
	if (action != GLFW_PRESS)
		return;
	switch (key) {
	case GLFW_KEY_SPACE:
		break;
	case GLFW_KEY_ESCAPE:
        vkengine_should_close(data->e);
		break;
	}
}

static void char_callback (GLFWwindow* window, uint32_t c) {
    App *app = app_data(window);
}

static void mouse_move_callback(GLFWwindow* window, double x, double y) {
    App *app            = app_data(window);
    app->data->cursor   = vec2d { x, y };

    for (node* n: app->data->hover)
        n->Element::data->hover = false;
    
    app->data->hover    = app->select_at(app->data->cursor, app->data->buttons[0]);

    for (node* n: app->data->hover)
        n->Element::data->hover = true;
}

static void scroll_callback(GLFWwindow* window, double x, double y) {
    App* app = app_data(window);
}

static void mouse_button_callback(GLFWwindow* window, int but, int state, int mods) {
    App* app = app_data(window);
    app->data->buttons[but] = bool(state);

    for (node* n: app->data->active)
        n->Element::data->active = false;
    
    if (state)
        app->data->active = app->select_at(app->data->cursor, false);
    else
        app->data->active = {};

    for (node* n: app->data->active)
        n->Element::data->active = true;
}

/// id's can be dynamic so we cant enforce symbols, and it would be a bit lame to make the caller symbolize
str element_id(Element &e) {
    array<arg> &args = e->args;
    for (arg &a: args) {
        if (strcmp((symbol)a.key.mem->origin, "id") == 0)
            return (cstr)a.value.mem->origin; /// convert to str
    }
    return e->type->name;
}

void composer::update(composer::cdata *composer, node *parent, node *&instance, Element &e) {
    bool       diff = !instance;
    size_t args_len = e->args.len();
    i64         now = millis();

    if (!diff) {
        /// instance != null, so we can check attributes
        /// compare args for diffs
        array<arg>    &p = (*(Element*)instance)->args; /// previous args
        array<arg>    &n = e->args; /// new args
        diff = args_len != p.len();
        if (!diff) {
            /// no reason to check against a map
            for (size_t i = 0; i < args_len; i++) {
                arg &p_pair = p[i];
                arg &n_pair = n[i];
                if (p_pair.key   != n_pair.key ||
                    p_pair.value != n_pair.value) {
                    diff = true;
                    break;
                }
            }
        }
    }
    if (diff) {
        /// if we get this far, its expected that we are a class with schema, and have data associated
        assert(e->type->schema);
        assert(e->type->schema->bind && e->type->schema->bind->data);

        /// create instance if its not there, or the type differs (if the type differs we shall delete the node)
        if (!instance || (e->type != instance->mem->type)) {
            if (instance)
                delete instance; /// Element data must also delete the nodes
            instance = e.new_instance();
            str   id = element_id(e); /// needed for style computation of available entries in the style blocks
            (*(Element*)instance)->parent = parent;
            (*(Element*)instance)->id     = id.grab();
        
            /// compute available properties for this node given its type, placement, and props styled 
            (*instance)->style_avail = composer->style->compute(instance);
        }

        /// arg set cache
        bool pset[args_len];
        memset(pset, 0, args_len * sizeof(bool));

        /// iterate through polymorphic meta info on the schema bindings on these context types (the user instantiates context, not data)
        for (type_t ctx = e->type; ctx; ctx = ctx->parent) {
            if (!ctx->schema)
                continue;
            type_t tdata = ctx->schema->bind->data;
            if (!tdata->meta) /// mx type does not contain a schema in itself
                continue;
            u8* data_origin = (u8*)instance->mem->typed_data(tdata, 0);
            hmap<ion::symbol, prop>* meta_map = (hmap<ion::symbol, prop> *)tdata->meta_map;
            doubly<prop>* props = (doubly<prop>*)tdata->meta;

            /// its possible some classes may not have meta information defined, just skip
            if (!meta_map)
                continue;

            /// todo: init a changed map or list here

            /// apply style to props (only do this when we need to, as this will be inefficient to do per update)
            /// dom uses invalidation for this such as property changed in a parent, element added, etc
            /// it does not need to be perfect we are not making a web browser
            style::style_map &style_avail = (*instance)->style_avail;
            for (prop &p: *props) {
                str &name = *p.s_key;
                field<array<style::entry*>> *entries = style_avail->lookup(name); // ctx name is Button, name == id, and it has a null entry for entries[0] == null with count > 
                if (entries) {
                    /// get best style matching entry for this property
                    style::entry *best = composer->style->best_match(instance, &p, *entries);
                    
                    ///
                    type_t prop_type = p.member_type;
                    u8    *prop_dst  = &data_origin[p.offset];
                    
                    /// dont repeat this; store best in transitions lookup for this member name
                    node::selection &sel = instance->data->selections[name];

                    /// in cases there will be no match, in other cases we have already selected
                    if (best && (best != sel.entry)) {

                        /// create instance and immediately assign if there is no transition
                        if (prop_type->traits & traits::mx_obj) {
                            if (!best->mx_instance) best->mx_instance = (mx*)prop_type->functions->from_string(null, best->value.cs());
                            if (!best->trans) prop_type->functions->set_memory(prop_dst, best->mx_instance->mem);
                        } else {
                            if (!best->raw_instance) best->raw_instance = prop_type->functions->from_string(null, best->value.cs());
                            if (!best->trans) prop_type->functions->assign(null, prop_dst, best->raw_instance);
                        }

                        /// handle transitions in the selection
                        if (best->trans) {
                            /// if we had a prior transition, delete the memory
                            if (sel.from) prop_type->functions->del(null, sel.from);

                            /// get copy of current value (new instance, and assign from current)
                            raw_t cur = prop_type->functions->alloc_new(null, null);
                            prop_type->functions->assign(null, cur, prop_dst);

                            /// setup data
                            sel.member = &p;
                            sel.start  = now;
                            sel.end    = now + i64(real(best->trans.dur));
                            sel.from   = cur;
                            sel.to     = best->mx_instance ? best->mx_instance : best->raw_instance; /// redundant
                            sel.entry  = best;
                        }

                    } else if (!best) {
                        /// if there is nothing to set to, we must set to its default initialization value
                        if (prop_type->traits & traits::mx_obj) {
                            prop_type->functions->set_memory(prop_dst, ((mx*)p.init_value)->mem);
                        } else {
                            prop_type->functions->assign(null, prop_dst, p.init_value);
                        }
                        sel.entry = null;
                    }
                }
            }

            /// handle selected transitions (if args are set, they will be overwritten)
            for (auto &field: instance->data->selections) {
                node::selection &sel = field.value;
                if (sel.start > 0) {
                    real amount = math::clamp(real(now - sel.start) / real(sel.end - sel.start), 0.0, 1.0);
                    assert(sel.entry->member->type() == sel.member->member_type);
                    type_t prop_type = sel.member->member_type;
                    u8    *prop_dst  = &data_origin[sel.member->offset];

                    assert(prop_type->functions->mix);
                    raw_t temp = prop_type->functions->mix(sel.from, sel.to, amount);

                    ///
                    if (prop_type->traits & traits::mx_obj) {
                        prop_type->functions->set_memory(prop_dst, ((mx*)p.init_value)->mem);
                    } else {
                        prop_type->functions->assign(null, prop_dst, p.init_value);
                    }

                    prop_type->functions->del(null, temp);
                }
            }
            
            /// iterate through args, skip those we have already set
            for (size_t i = 0; i < args_len; i++) {
                if (pset[i]) 
                    continue;
                arg &a = e->args[i];
                memory *key = a.key.mem;
                /// only support a key type of char.  we can support variables that convert to char array
                if (key->type == typeof(char)) {
                    symbol s_key = (symbol)key->origin;
                    prop *def = meta_map->lookup(s_key);
                    if (def) {
                        type_t prop_type = def->member_type;
                        type_t arg_type  = a.value.type();
                        u8    *prop_dst  = &data_origin[def->offset];
                        u8    *arg_src   = (u8*)a.value.mem->typed_data(arg_type, 0); /// passing int into mx recovers int, but passing lambda will give data inside.  we want to store a context
                        u8    *conv_inst = null;

                        /// if prop type is mx, we can generalize
                        if (prop_type == typeof(mx)) {
                            if (arg_type != prop_type) {
                                conv_inst = (u8*)new mx(a.value);
                                arg_src   = (u8*)conv_inst;
                                arg_type  = typeof(mx);
                            }
                        }
                        /// if going to string and arg is not, we convert
                        else if (prop_type == typeof(str) && arg_type != prop_type) {
                            /// general to_string conversion (memory of char)
                            assert(arg_type->functions->to_string);
                            memory *m_str = arg_type->functions->to_string(arg_src);
                            conv_inst = (u8*)new str(m_str); /// no grab here
                            arg_src   = (u8*)conv_inst;
                            arg_type  = typeof(str);
                        }
                        /// general from_string conversion.  the class needs to have a cstr constructor
                        else if ((arg_type == typeof(char) || arg_type == typeof(str)) && prop_type != arg_type) {
                            assert(prop_type->functions->from_string);
                            conv_inst = (u8*)prop_type->functions->from_string(null,
                                arg_type == typeof(str) ? (cstr)a.value.mem->origin : (cstr)arg_src);
                            arg_src = (u8*)conv_inst;
                            arg_type = prop_type;
                        }
                        /// types should match
                        assert(arg_type == prop_type);

                        if (prop_type->traits & traits::mx_obj) {
                            /// set by memory construction (cast conv_inst as mx which it must be)
                            assert(!conv_inst || (arg_type->traits & traits::mx_obj) ||
                                                 (arg_type->traits & traits::mx));
                            prop_type->functions->set_memory(prop_dst, conv_inst ? ((mx*)conv_inst)->mem : a.value.mem);
                        } else {
                            /// assign property with data that is of the same type
                            prop_type->functions->assign(null, prop_dst, arg_src);
                        }
                        pset[i] = true;
                        if (conv_inst) arg_type->functions->del(null, conv_inst);
                    }
                } else {
                    console.fault("unsupported key type");
                }
            }
        }

        /// blending the update w props changed and the render abstract
        /// needs array<Element> support
        /// that in itsself is still an 'Element', with an instance to a kind of placeholder node
        /// the issue with that is its id-less nature, its typeless too in a way.
        /// 

        /// support transitions after first light.

        Element render = instance->update(); /// needs a 'changed' arg
        if (render) {
            /// contains mount info (node instance Element data)
            Element &edata = *(Element*)instance;
            if (render->children) {
                /// each child is mounted
                for (Element &e: render->children) {
                    str id = element_id(e);
                    update(composer, instance, edata->mounts[id], e);
                }
            } else {
                str id = element_id(render);
                update(composer, instance, edata->mounts[id], render);
            }
        }
    }
}

void composer::update_all(Element e) {
    /// style is a singleton and this initializes the data
    /// i dont favor multiple style databases. thats too complicated and not needed
    if (!data->root_instance)
        data->style = style::init();
    
    update(data, null, data->root_instance, e);
}

int App::run() {
    data->e = vkengine_create(1, 2, "ux",
        VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, VK_PRESENT_MODE_FIFO_KHR, VK_SAMPLE_COUNT_4_BIT,
        512, 512, 0, this);
    data->canvas = gfx(data->e, data->e->renderer); /// width and height are fetched from renderer (which updates in vkengine)

	vkengine_set_key_callback       (data->e, key_callback);
	vkengine_set_mouse_but_callback (data->e, mouse_button_callback);
	vkengine_set_cursor_pos_callback(data->e, mouse_move_callback);
	vkengine_set_scroll_callback    (data->e, scroll_callback);
	vkengine_set_title              (data->e, "ux");

	while (!vkengine_should_close(data->e)) {
		glfwPollEvents();

        data->canvas->ctx = vkvg_create(data->canvas->vg_surface);
        assert(data->canvas->stack->len() == 1);
        data->canvas.defaults();
        
        /// update app with rendered Elements, then draw
        Element e = data->app_fn(*this);
        update_all(e);
        if (composer::data->root_instance) {
            /// update rect
            composer::data->root_instance->data->bounds = rectd {
                0, 0,
                (real)data->canvas->width,
                (real)data->canvas->height
            };
            composer::data->root_instance->draw(data->canvas);
        }

        vkvg_destroy(data->canvas->ctx);

        /// we need an array of renderers/presenters; must work with a 3D scene, bloom shading etc
		if (!vkh_presenter_draw(data->e->renderer)) { 
            data->canvas.resized();
            vkDeviceWaitIdle(data->e->vkh->device);
			continue;
		}
	}
    return 0;
}

bool ws(cstr &cursor) {
    auto is_cmt = [](symbol c) -> bool { return c[0] == '/' && c[1] == '*'; };
    while (isspace(*cursor) || is_cmt(cursor)) {
        while (isspace(*cursor))
            cursor++;
        if (is_cmt(cursor)) {
            cstr f = strstr(cursor, "*/");
            cursor = f ? &f[2] : &cursor[strlen(cursor) - 1];
        }
    }
    return *cursor != 0;
}

bool scan_to(cstr &cursor, array<char> chars) {
    bool sl  = false;
    bool qt  = false;
    bool qt2 = false;
    for (; *cursor; cursor++) {
        if (!sl) {
            if (*cursor == '"')
                qt = !qt;
            else if (*cursor == '\'')
                qt2 = !qt2;
        }
        sl = *cursor == '\\';
        if (!qt && !qt2)
            for (char &c: chars)
                if (*cursor == c)
                    return true;
    }
    return false;
}

doubly<style::qualifier*> parse_qualifiers(style::block &bl, cstr *p) {
    str   qstr;
    cstr start = *p;
    cstr end   = null;
    cstr scan  =  start;
    
    /// read ahead to {
    do {
        if (!*scan || *scan == '{') {
            end  = scan;
            qstr = str(start, std::distance(start, scan));
            break;
        }
    } while (*++scan);
    
    ///
    if (!qstr) {
        end = scan;
        *p  = end;
        return null;
    }
    
    ///
    auto  quals = qstr.split(",");
    doubly<style::qualifier*> result;

    ///
    for (str &qs:quals) {
        str  q = qs.trim();
        if (!q) continue;
        style::qualifier *v = new style::qualifier();
        result->push(v);

        int idot = q.index_of(".");
        int icol = q.index_of(":");
        str tail;
        ///
        if (idot >= 0) {
            array<str> sp = q.split(".");
            v->type   = sp[0];
            v->id     = sp[1];
            if (icol >= 0)
                tail  = q.mid(icol + 1).trim();
        } else {
            if (icol  >= 0) {
                v->type = q.mid(0, icol);
                tail   = q.mid(icol + 1).trim();
            } else
                v->type = q;
        }
        if (v->type) {
            v->ty = types::lookup(v->type);
            if (bl.types.index_of(v->ty) == -1)
                bl.types += v->ty;
        }
        array<str> ops {"!=",">=","<=",">","<","="};
        if (tail) {
            // check for ops
            bool is_op = false;
            for (str &op:ops) {
                if (tail.index_of(op.cs()) >= 0) {
                    is_op   = true;
                    auto sp = tail.split(op);
                    v->state = sp[0].trim();
                    v->oper  = op;
                    v->value = tail.mid(sp[0].len() + op.len()).trim();
                    break;
                }
            }
            if (!is_op)
                v->state = tail;
        }
    }
    *p = end;
    return result;
}

size_t style::block::score(node *pn, bool score_state) {
    node &n = *pn;
    double best_sc = 0;
    Element::edata *edata = ((Element*)pn)->data;
    for (qualifier *q:quals) {
        qualifier &qd = *q;
        bool    id_match  = qd.id    &&  qd.id == n->id;
        bool   id_reject  = qd.id    && !id_match;
        bool  type_match  = qd.type  &&  strcmp((symbol)qd.type.cs(), (symbol)n.mem->type->name) == 0; /// class names are actual type names
        bool type_reject  = qd.type  && !type_match;
        bool state_match  = score_state && qd.state && get_bool(typeof(Element::edata), edata, qd.state); /// a useful thing for string const input would be to flag that its already symbolized.  that way you dont look it up when you operate it back
        bool state_reject = score_state && qd.state && !state_match;

        ///
        if (!id_reject && !type_reject && !state_reject) {
            double sc = size_t(   id_match) << 1 |
                        size_t( type_match) << 0 |
                        size_t(state_match) << 2;
            best_sc = math::max(sc, best_sc);
        }
    }
    return best_sc;
};

/// each qualifier is defined as it, and all of the blocked qualifiers below.
/// there are more optimal data structures to use for this matching routine
/// state > state2 would be a nifty one, no operation indicates bool, as-is current normal syntax
double style::block::match(node *from, bool match_state) {
    block          *bl = this;
    double total_score = 0;
    int            div = 0;
    node            *n = from;
    ///
    while (bl && n) {
        bool   is_root = !bl->parent;
        double score   = 0;
        ///
        while (n) { ///
            auto sc = bl->score(n, match_state);
            n       = n->Element::data->parent;
            if (sc == 0 && is_root) {
                score = 0;
                break;
            } else if (sc > 0) {
                score += double(sc) / ++div;
                break;
            }
        }
        total_score += score;
        ///
        if (score > 0) {
            /// proceed...
            bl = bl->parent;
        } else {
            /// style not matched
            bl = null;
            total_score = 0;
        }
    }
    return total_score;
}

/// compute available entries for props on a node
style::style_map style::impl::compute(node *n) {
    style_map avail(16);
    ///
    for (type_t ctx = n->mem->type; ctx; ctx = ctx->parent) {
        if (!ctx->schema || !ctx->parent)
            continue;
        type_t data = ctx->schema->bind->data;
        if (!data->meta)
            continue;

        ///
        array<entry *> all(32);
        for (prop &p: *(doubly<prop>*)data->meta) { /// the blocks can contain a map of types with entries associated, with a * key for all
            if (applicable(n, &p, all)) {
                str   s_name  = p.key->grab(); /// it will free if you dont grab it
                avail[s_name] = all;
                all = array<entry *>(32);
            }
        }
    }
    return avail;
}

void style::impl::cache_members() {
    lambda<void(block*)> cache_b;
    ///
    cache_b = [&](block *bl) -> void {
        for (entry *e: bl->entries) {
            bool  found = false;
            ///
            array<block*> &cache = members[e->member];
            for (block *cb:cache)
                 found |= cb == bl;
            ///
            if (!found)
                 cache += bl;
        }
        for (block *s:bl->blocks)
            cache_b(s);
    };
    if (root)
        for (block *b:root)
            cache_b(b);
}

void style::impl::load(str code) {
    for (cstr sc = code.cs(); ws(sc); sc++) {
        lambda<void(block*)> parse_block;
        ///
        parse_block = [&](block *bl) {
            ws(sc);
            console.test(*sc == '.' || isalpha(*sc), "expected Type[.id], or .id", {});
            bl->quals = parse_qualifiers(*bl, &sc);
            ws(++sc);
            ///
            while (*sc && *sc != '}') {
                /// read up to ;, {, or }
                ws(sc);
                cstr start = sc;
                console.test(scan_to(sc, {';', '{', '}'}), "expected member expression or qualifier", {});
                if (*sc == '{') {
                    ///
                    block *bl_n = new style::block();
                    bl->blocks->push(bl_n);
                    bl_n->parent = bl;

                    /// parse sub-block
                    sc = start;
                    parse_block(bl_n);
                    assert(*sc == '}');
                    ws(++sc);
                    ///
                } else if (*sc == ';') {
                    /// read member
                    cstr cur = start;
                    console.test(scan_to(cur, {':'}) && (cur < sc), "expected [member:]value;");
                    str  member = str(start, std::distance(start, cur));
                    ws(++cur);

                    /// read value
                    cstr vstart = cur;
                    console.test(scan_to(cur, {';'}), "expected member:[value;]");
                    
                    /// needs escape sequencing?
                    size_t len = std::distance(vstart, cur);
                    str  cb_value = str(vstart, len).trim();
                    str       end = cb_value.mid(-1, 1);
                    bool       qs = cb_value.mid( 0, 1) == "\"";
                    bool       qe = cb_value.mid(-1, 1) == "\"";

                    if (qs && qe) {
                        cstr   cs = cb_value.cs();
                        cb_value  = str::parse_quoted(&cs, cb_value.len());
                    }

                    int         i = cb_value.index_of(",");
                    str     param = i >= 0 ? cb_value.mid(i + 1).trim() : "";
                    str     value = i >= 0 ? cb_value.mid(0, i).trim()  : cb_value;
                    style::transition trans = param ? style::transition(param) : null;
                    
                    /// check
                    console.test(member, "member cannot be blank");
                    console.test(value,  "value cannot be blank");
                    bl->entries[member] = new entry { member, value, trans, bl };
                    /// 
                    ws(++sc);
                }
            }
            console.test(!*sc || *sc == '}', "expected closed-brace");
        };
        ///
        block *n_block = new block();
        root.push(n_block);
        parse_block(n_block);
    }
}

style style::init() {
    style st;
    if (!st->loaded) {
        path base_path = "style";
        /// there could be sub-dirs here with an argument
        base_path.resources({".css"}, {},
            [&](path css_file) -> void {
                str style_str = css_file.read<str>();
                st->load(style_str);
            });

        /// store blocks by member, the interface into all style: style::members[name]
        st->cache_members();
        st->loaded = true;
    }
    return st;
}

style::entry *style::impl::best_match(node *n, prop *member, array<style::entry*> &entries) {
    array<style::block*> &blocks = members[*member->s_key]; /// instance function when loading and updating style, managed map of [style::block*]
    style::entry *match = null; /// key is always a symbol, and maps are keyed by symbol
    real     best_score = 0;

    /// should narrow down by type used in the various blocks by referring to the qualifier
    /// find top style pair for this member
    type_t type = n->mem->type;
    for (style::entry *e: entries) {
        block *bl = e->bl;
        real score = bl->match(n, true);
        if (score > 0 && score >= best_score) {
            match = bl->entries[*member->s_key];
            assert(match);
            best_score = score;
        }
    }
    return match;
}

/// todo: move functions into itype pattern
bool style::impl::applicable(node *n, prop *member, array<style::entry*> &result) {
    array<style::block*> &blocks = members[*member->s_key];
    type_t type = n->mem->type;
    bool ret = false;

    for (style::block *block:blocks) {
        if (!block->types || block->types.index_of(type) >= 0) {
            auto f = block->entries->lookup(*member->s_key);
            if (f && block->match(n, false) > 0) {
                result += f->value;
                ret = true;
            }
        }
    }
    return ret;
}

void node::draw(gfx& canvas) {
    Element::edata *edata    = ((Element*)this)->data;
    rect<r64>       bounds   = edata->parent ? data->bounds : rect<r64> { 0, 0, r64(canvas->width), r64(canvas->height) };
    props::drawing &fill     = data->drawings[operation::fill];
    props::drawing &image    = data->drawings[operation::image];
    props::drawing &text     = data->drawings[operation::text];
    props::drawing &outline  = data->drawings[operation::outline]; /// outline is more AR than border.  and border is a bad idea, badly defined and badly understood. outline is on the 0 pt.  just offset it if you want.
    props::drawing &children = data->drawings[operation::child]; /// outline is more AR than border.  and border is a bad idea, badly defined and badly understood. outline is on the 0 pt.  just offset it if you want.
    
    canvas.push();
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
        canvas.push();
        canvas.color(fill.color);
        canvas.opacity(effective_opacity());
        canvas.fill(data->fill_bounds);
        canvas.pop();
    }

    /// if there is fill image
    if (image.img) {
        image.shape = image.area.rect(bounds);
        canvas.push();
        canvas.opacity(effective_opacity());
        canvas.image(image.img, image.shape, image.align, {0,0}, {0,0});
        canvas.pop();
    }
    
    /// if there is text (its not alpha 0, and there is text)
    if (data->content && ((data->content.type() == typeof(char)) ||
                          (data->content.type() == typeof(str)))) {
        rectd text_rect = text.area ? text.area.rect(bounds) : data->fill_bounds;
        canvas.push();
        canvas.color(text.color);
        canvas.opacity(effective_opacity());
        canvas.font(data->font);
        canvas.text(
            data->content, text_rect, /// useful to have control over text area but having a fill placement alone should make that the text area as well, unless specified as text-area
            text.align, {0.0, 0.0}, true); // align is undefined here
        canvas.pop();
        text.shape = text_rect;
    }

    /// if there is an effective border to draw
    if (outline.color && outline.border->size > 0.0) {
        rectd outline_rect = outline.area ? outline.area.rect(bounds) : data->fill_bounds;
        if (outline.radius) {
            vec4d r = outline.radius;
            vec2d tl { r.x, r.x };
            vec2d tr { r.y, r.y };
            vec2d br { r.w, r.w };
            vec2d bl { r.z, r.z };
            outline_rect.set_rounded(tl, tr, br, bl);
        }
        canvas.color(outline.border->color);
        canvas.opacity(effective_opacity());
        canvas.outline_sz(outline.border->size);
        canvas.outline(outline_rect); /// this needs to work with vshape, or border
        outline.shape = outline_rect; /// updating for interactive logic
    }

    for (node *c: edata->mounts) {
        /// clip to child
        /// translate to child location
        rectd sub_bounds = children.area ? children.area.rect(bounds) : data->fill_bounds;
        canvas.translate(sub_bounds.xy());
        canvas.opacity(effective_opacity());
        (*c)->bounds = rect<r64> { 0, 0, sub_bounds.w, sub_bounds.h };
        //canvas.clip(0, 0, r.w, r.h);
        c->draw(canvas);
    }

    canvas.pop();
}

}
