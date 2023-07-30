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
    
    cbase::draw_state *ds;

    ~gfx_memory() {
        if (e)            vkDeviceWaitIdle(e->vkh->device);
        if (ctx)          vkvg_destroy(ctx);
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

/// used with terminal canvas
static map<str> t_text_colors_8 = {
    { "#000000" , "\u001b[30m" },
    { "#ff0000",  "\u001b[31m" },
    { "#00ff00",  "\u001b[32m" },
    { "#ffff00",  "\u001b[33m" },
    { "#0000ff",  "\u001b[34m" },
    { "#ff00ff",  "\u001b[35m" },
    { "#00ffff",  "\u001b[36m" },
    { "#ffffff",  "\u001b[37m" }
};

static map<str> t_bg_colors_8 = {
    { "#000000" , "\u001b[40m" },
    { "#ff0000",  "\u001b[41m" },
    { "#00ff00",  "\u001b[42m" },
    { "#ffff00",  "\u001b[43m" },
    { "#0000ff",  "\u001b[44m" },
    { "#ff00ff",  "\u001b[45m" },
    { "#00ffff",  "\u001b[46m" },
    { "#ffffff",  "\u001b[47m" }
};

str terminal::ansi_color(rgba8 &c, bool text) {
    map<str> &map = text ? t_text_colors_8 : t_bg_colors_8;
    if (c.a < 32)
        return "";
    str hex = str("#") + color_value(c);
    return map->count(hex) ? map[hex] : "";
}

void terminal::draw_state_change(draw_state &ds, cbase::state_change type) {
    data->ds = &ds;
    switch (type) {
        case cbase::state_change::push:
            break;
        case cbase::state_change::pop:
            break;
        default:
            break;
    }
}

terminal::terminal(vec2i sz) : terminal()  {
    cbase::data->size    = sz;
    cbase::data->pixel_t = typeof(char);  
    size_t n_chars = sz.x * sz.y;
    data->glyphs = array<glyph>(ion::size({sz.y, sz.x}));
    for (size_t i = 0; i < n_chars; i++)
        data->glyphs += glyph {};
}

void terminal::set_char(int x, int y, glyph gl) {
    draw_state &ds = *data->ds;
    vec2i sz = cbase::size();
    if (x < 0 || x >= sz.x) return;
    if (y < 0 || y >= sz.y) return;
    //assert(!ds.clip || sk_vshape_is_rect(ds.clip));
    size_t index  = y * sz.y + x;
    data->glyphs[{y, x}] = gl;
    /*
    ----
    change-me: dont think this is required on 'set', but rather a filter on get
    ----
    if (cg.border) data->glyphs[index]->border  = cg->border;
    if (cg.chr) {
        str  gc = data->glyphs[index].chr;
        if ((gc == "+") || (gc == "|" && cg.chr == "-") ||
                            (gc == "-" && cg.chr == "|"))
            data->glyphs[index].chr = "+";
        else
            data->glyphs[index].chr = cg.chr;
    }
    if (cg.fg) data->glyphs[index].fg = cg.fg;
    if (cg.bg) data->glyphs[index].bg = cg.bg;
    */
}

// gets the effective character, including bordering
str terminal::get_char(int x, int y) {
    cbase::draw_state &ds = *data->ds;
    vec2i       &sz = cbase::data->size;
    size_t       ix = math::clamp<size_t>(x, 0, sz.x - 1);
    size_t       iy = math::clamp<size_t>(y, 0, sz.y - 1);
    str       blank = " ";
    
    auto get_str = [&]() -> str {
        auto value_at = [&](int x, int y) -> glyph * {
            if (x < 0 || x >= sz.x) return null;
            if (y < 0 || y >= sz.y) return null;
            return &data->glyphs[y * sz[1] + x];
        };
        
        auto is_border = [&](int x, int y) {
            glyph *cg = value_at(x, y);
            return cg ? ((*cg)->border > 0) : false;
        };
        
        glyph::members &cg = data->glyphs[iy * sz[1] + ix];
        auto   t  = is_border(x, y - 1);
        auto   b  = is_border(x, y + 1);
        auto   l  = is_border(x - 1, y);
        auto   r  = is_border(x + 1, y);
        auto   q  = is_border(x, y);
        
        if (q) {
            if (t && b && l && r) return "\u254B"; //  +
            if (t && b && r)      return "\u2523"; //  |-
            if (t && b && l)      return "\u252B"; // -|
            if (l && r && t)      return "\u253B"; // _|_
            if (l && r && b)      return "\u2533"; //  T
            if (l && t)           return "\u251B";
            if (r && t)           return "\u2517";
            if (l && b)           return "\u2513";
            if (r && b)           return "\u250F";
            if (t && b)           return "\u2503";
            if (l || r)           return "\u2501";
        }
        return cg.chr;
    };
    return get_str();
}

void terminal::text(str s, graphics::shape vrect, alignment align, vec2d voffset, bool ellip) {
    rectd    rect   =  vrect.bounds();
    vec2i   &sz     =  cbase::size();
    vec2d   &offset =  voffset;
    draw_state &ds     = *data->ds;
    int         len    =  int(s.len());
    int         szx    = int(sz.x);
    int         szy    = int(sz.y);
    ///
    if (len == 0)
        return;
    ///
    int x0 = math::clamp(int(math::round(rect.x)), 0,          szx - 1);
    int x1 = math::clamp(int(math::round(rect.x + rect.w)), 0, szx - 1);
    int y0 = math::clamp(int(math::round(rect.y)), 0,          szy - 1);
    int y1 = math::clamp(int(math::round(rect.y + rect.h)), 0, szy - 1);
    int  w = (x1 - x0) + 1;
    int  h = (y1 - y0) + 1;
    ///
    if (!w || !h) return;
    ///
    int tx = align->x == xalign::left   ? (x0) :
             align->x == xalign::middle ? (x0 + (x1 - x0) / 2) - int(std::ceil(len / 2.0)) :
             align->x == xalign::right  ? (x1 - len) : (x0);
    ///
    int ty = align->y == yalign::top    ? (y0) :
             align->y == yalign::middle ? (y0 + (y1 - y0) / 2) - int(std::ceil(len / 2.0)) :
             align->y == yalign::bottom ? (y1 - len) : (y0);
    ///
    tx           = math::clamp(tx, x0, x1);
    ty           = math::clamp(ty, y0, y1);
    size_t ix    = math::clamp(size_t(tx), size_t(0), size_t(szx - 1));
    size_t iy    = math::clamp(size_t(ty), size_t(0), size_t(szy - 1));
    size_t len_w = math::  min(size_t(x1 - tx), size_t(len));
    ///
    for (size_t i = 0; i < len_w; i++) {
        int x = ix + i + offset.x;
        int y = iy + 0 + offset.y;
        if (x < 0 || x >= szx || y < 0 || y >= szy)
            continue;
        str chr = s.mid(i, 1);
        glyph g = glyph::members { .chr = chr, .fg = ds.color };
        set_char(tx + int(i), ty, g);
    }
}

void terminal::outline(graphics::shape sh) {
    draw_state &ds = *data->ds;
    assert(sh);
    rectd     &r = sh.bounds();
    int     ss = math::min(2.0, math::round(ds.outline_sz));
    rgba8  &c = ds.color;
    glyph cg_0 = glyph::members { ss, "-", rgba8 {0,0,0,0}, rgba8 { c.r, c.g, c.b, c.a }};
    glyph cg_1 = glyph::members { ss, "|", rgba8 {0,0,0,0}, rgba8 { c.r, c.g, c.b, c.a }};
    
    for (int ix = 0; ix < int(r.w); ix++) {
        set_char(int(r.x + ix), int(r.y), cg_0);
        if (r.h > 1)
            set_char(int(r.x + ix), int(r.y + r.h - 1), cg_0);
    }
    for (int iy = 0; iy < int(r.h); iy++) {
        set_char(int(r.x), int(r.y + iy), cg_1);
        if (r.w > 1)
            set_char(int(r.x + r.w - 1), int(r.y + iy), cg_1);
    }
}

void terminal::fill(graphics::shape sh) {
    draw_state &ds = *data->ds;
    vec2i      &sz = cbase::size();
    int         sx = int(sz.x);
    int         sy = int(sz.y);
    if (ds.color.a) {
        rectd    &r = sh.bounds();
        str t_color = ansi_color(ds.color, false);
        ///
        int      x0 = math::max(int(0),      int(r.x)),
                 x1 = math::min(int(sx - 1), int(r.x + r.w));
        int      y0 = math::max(int(0),      int(r.y)),
                 y1 = math::min(int(sy - 1), int(r.y + r.h));

        glyph    cg = glyph::members { 0, " ", rgba8(ds.color), rgba8 { 0,0,0,0 } }; /// members = data

        for (int x = x0; x <= x1; x++)
            for (int y = y0; y <= y1; y++)
                set_char(x, y, cg); /// just set mem w grab()
    }
}

struct memory;
struct Texture;

#if 0
void sk_canvas_gaussian(sk_canvas_data* sk_canvas, vec2d* sz, rectd* crop) {
    SkPaint  &sk     = *sk_canvas->state->backend.sk_paint;
    SkCanvas &canvas = *sk_canvas->sk_canvas;
    ///
    SkImageFilters::CropRect crect = { };
    if (crop && crop->w > 0 && crop->h > 0) {
        SkRect rect = { SkScalar(crop->x),          SkScalar(crop->y),
                        SkScalar(crop->x + crop->w), SkScalar(crop->y + crop->h) };
        crect       = SkImageFilters::CropRect(rect);
    }
    sk_sp<SkImageFilter> filter = SkImageFilters::Blur(sks(sz->x), sks(sz->y), nullptr, crect);
    sk.setImageFilter(std::move(filter));
}
#endif

mx_implement(gfx, cbase);

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

    /// gfx just needs a push off the ledge. [/penguin-drops]
    push(); 
    defaults();
}

void gfx::resized() {
    if (data->vg_surface)
        vkvg_surface_drop(data->vg_surface);
    if (data->vkh_image)
        vkh_image_drop(data->vkh_image);
    if (data->ctx)
        vkvg_destroy(data->ctx);

    /// these should be updated (VkEngine can have VkWindow of sort eventually if we want multiple)
    float sx, sy;
    vkh_presenter_get_size(data->vkh_renderer, &data->width, &data->height, &sx, &sy); /// vkh should have both vk engine and glfw facility
    data->width  /= sx; /// we use the virtual size here
    data->height /= sy;
    cbase::data->size.x = data->width;
    cbase::data->size.y = data->height;

    /// canvas w and h is virtual pixels; like dom and things; all user-mode stuff on top of vkvg is virtual
	data->vg_surface = vkvg_surface_create(data->vg_device, data->width, data->height); 
	data->vkh_image  = vkvg_surface_get_image(data->vg_surface);
    data->ctx        = vkvg_create(data->vg_surface);

    vkh_presenter_build_blit_cmd(data->vkh_renderer, data->vkh_image->image, data->width, data->height);
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

/// Quake2: { computer. updated. }
void gfx::draw_state_change(draw_state *ds, cbase::state_change type) {
    data->ds = ds;
}

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
        .line_height =  real(data->ds->line_height)
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
    draw_state   &ds = *data->ds;
    ion::size     sz = img.shape();
    rectd           &r = sh.bounds();
    assert(surf);
    vkvg_set_source_rgba(data->ctx,
        ds.color.r, ds.color.g, ds.color.b, ds.color.a * ds.opacity);
    
    /// now its just of matter of scaling the little guy to fit in the box.
    vec2d vsc = { math::min(1.0, r.w / real(sz[1])), math::min(1.0, r.h / real(sz[0])) };
    real       sc = (vsc.y > vsc.x) ? vsc.x : vsc.y;
    vec2d      va = vec2d(align);
    vec2d     pos = { mix(r.x, r.x + r.w - sz[1] * sc, va.x),
                      mix(r.y, r.y + r.h - sz[0] * sc, va.y) };
    
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

void gfx::push() {
    cbase::push();
    vkvg_save(data->ctx);
}

void gfx::pop() {
    cbase::pop();
    vkvg_restore(data->ctx);
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
/// 
void gfx::text(str text, Rect<double> rect, alignment align, vec2d offset, bool ellip) {
    draw_state &ds = *data->ds;
    rgba8 &c = ds.color;
    ///
    vkvg_save(data->ctx);
    vkvg_set_source_rgba(data->ctx, c.r, c.g, c.b, c.a * ds.opacity);
    ///
    vec2d          pos  = { 0, 0 };
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
    real line_height = (-tm->descent + tm->ascent) / 1.66;
    vec2d           tl = { rect->x,  rect->y + line_height / 2 };
    vec2d           br = { rect->x + rect->w - tm->w,
                            rect->y + rect->h - tm->h - line_height / 2 };
    vec2d           va = vec2d(align);
    pos              = mix(tl, br, va);
    
    vkvg_show_text(data->ctx, (symbol)text.cs());
    vkvg_restore  (data->ctx);
}

void gfx::clip(graphics::shape cl) {
    draw_state  &ds = cur();
    ds.clip  = cl;
    vkvg_path(data->ctx, cl.mem);
    vkvg_clip(data->ctx);
}

VkhImage gfx::texture() { return data->vkh_image; } /// associated with surface

void gfx::flush() {
    vkvg_flush(data->ctx);
}

void gfx::clear(rgba8 c) {
    vkvg_save           (data->ctx);
    vkvg_set_source_rgba(data->ctx, c.r, c.g, c.b, c.a);
    vkvg_paint          (data->ctx);
    vkvg_restore        (data->ctx);
}

void gfx::font(ion::font f) {
    vkvg_select_font_face(data->ctx, f->alias.cs());
    vkvg_set_font_size   (data->ctx, f->sz);
}

void gfx::cap  (graphics::cap    c) { vkvg_set_line_cap (data->ctx, vkvg_line_cap_t (int(c))); }
void gfx::join (graphics::join   j) { vkvg_set_line_join(data->ctx, vkvg_line_join_t(int(j))); }
void gfx::translate(vec2d       tr) { vkvg_translate    (data->ctx, tr.x, tr.y);  }
void gfx::scale    (vec2d       sc) { vkvg_scale        (data->ctx, sc.x, sc.y);  }
void gfx::scale    (real        sc) { vkvg_scale        (data->ctx, sc, sc);        }
void gfx::rotate   (real      degs) { vkvg_rotate       (data->ctx, radians(degs)); }
void gfx::fill(graphics::shape   p) {
    vkvg_path(data->ctx, p.mem);
    vkvg_fill(data->ctx);
}

void gfx::gaussian (vec2d sz, graphics::shape c) { }

void gfx::outline  (graphics::shape p) {
    vkvg_path(data->ctx, p.mem);
    vkvg_stroke(data->ctx);
}

str      gfx::get_char  (int x, int y)       { return str(); }
str      gfx::ansi_color(rgba8 &c, bool text) { return str(); }

ion::image gfx::resample(ion::size sz, real deg, graphics::shape view, vec2d axis) {
    rgba8 *pixels = null;
    int scanline = 0;
    return ion::image(sz, (rgba8*)pixels, scanline);
}

void App::resize(vec2i &sz, App *app) {
    printf("resized: %d, %d\n", sz.x, sz.y);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    App::adata *data = (App::adata *)glfwGetWindowUserPointer(window);
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

static void char_callback (GLFWwindow* window, uint32_t c) { }

static void mouse_move_callback(GLFWwindow* window, double x, double y) {
}

static void scroll_callback(GLFWwindow* window, double x, double y) {
}

static void mouse_button_callback(GLFWwindow* window, int but, int state, int mods) {
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
            (*instance)->style_avail = composer->style.compute(instance);
        }

        /// arg set cache
        bool pset[args_len];
        memset(pset, 0, args_len * sizeof(bool));

        /// iterate through polymorphic meta info
        for (type_t t = e->type; t; t = t->parent) {
            if (!t->meta || !t->schema || !t->parent) /// mx type does not contain a schema in itself
                continue;
            ///
            type_t tdata = t->schema->bind->data;
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
                field<array<style::entry*>> *entries = style_avail->lookup(name);
                if (entries) {
                    /// get best style matching entry for this property
                    style::entry *best = composer->style.best_match(instance, &p, *entries);
                    type_t prop_type = p.member_type;
                    u8    *prop_dst  = &data_origin[p.offset];
                    
                    /// in many cases there will be no match
                    if (best) {
                        style::entry::edata *b = best->data;
                        ///
                        if (prop_type->traits & traits::mx_obj) {
                            /// we lazy load from_string because when style is loaded we do not have types
                            if (!b->mx_instance) b->mx_instance = (mx*)prop_type->functions->from_string(null, b->value.cs());
                            
                            /// set by memory construction (mx_instance holds memory)
                            prop_type->functions->set_memory(prop_dst, b->mx_instance->mem);
                        } else {
                            /// primitive type
                            if (!b->raw_instance) b->raw_instance = prop_type->functions->from_string(null, b->value.cs());
                            
                            /// assign property with data that is of the same type
                            prop_type->functions->assign(null, prop_dst, b->raw_instance);
                        }
                    } else {
                        /// if there is nothing to set to, we must set to its default initialization value (not the T default)
                        /// that is the default the parent type constructs it to
                        /// there remains the case of args but we do not need to handle it now
                        if (prop_type->traits & traits::mx_obj) {
                            prop_type->functions->set_memory(prop_dst, ((mx*)p.init_value)->mem);
                        } else {
                            prop_type->functions->assign(null, prop_dst, p.init_value);
                        }
                    }
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

                    if (strcmp(s_key, "content") == 0) {
                        int test = 0;
                        test++;
                    }

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
                        } else if ((arg_type == typeof(char) || arg_type == typeof(str)) && prop_type != arg_type) {
                            /// general from_string conversion.  the class needs to have a cstr constructor
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
        style::init();
    update(data, null, data->root_instance, e);
}

int App::run() {
    data->e = vkengine_create(1, 2, "ux",
        VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, VK_PRESENT_MODE_FIFO_KHR, VK_SAMPLE_COUNT_4_BIT,
        512, 512, 0, data);
    data->canvas = gfx(data->e, data->e->renderer); /// width and height are fetched from renderer (which updates in vkengine)

	vkengine_set_key_callback       (data->e, key_callback);
	vkengine_set_mouse_but_callback (data->e, mouse_button_callback);
	vkengine_set_cursor_pos_callback(data->e, mouse_move_callback);
	vkengine_set_scroll_callback    (data->e, scroll_callback);
	vkengine_set_title              (data->e, "ux");

	while (!vkengine_should_close(data->e)) {
		glfwPollEvents();

        /// update app with rendered Elements, then draw
        Element e = data->app_fn(*this);
        update_all(e);
        if (composer::data->root_instance)
            composer::data->root_instance->draw(data->canvas);

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

doubly<style::qualifier> parse_qualifiers(style::block &bl, cstr *p) {
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
    doubly<style::qualifier> result;

    ///
    for (str &qs:quals) {
        str  q = qs.trim();
        if (!q) continue;
        style::qualifier &v = result->push();
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
            v->ty = ident::lookup(v->type); /// we will need a bootstrap function that inits all the ux types. lol
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

size_t style::block::score(node *n) {
    double best_sc = 0;
    for (qualifier &q:data->quals) {
        qualifier::members &qd = *q;
        bool    id_match  = qd.id    &&  qd.id == n->data->id;
        bool   id_reject  = qd.id    && !id_match;
        bool  type_match  = qd.type  &&  strcmp((symbol)qd.type.cs(), (symbol)n->mem->type->name) == 0; /// class names are actual type names
        bool type_reject  = qd.type  && !type_match;
        bool state_match  = qd.state &&  n->data->istates[qd.state];
        bool state_reject = qd.state && !state_match;
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
double style::block::match(node *from) {
    block          &bl = *this;
    double total_score = 0;
    int            div = 0;
    node            *n = from;
    ///
    while (bl && n) {
        bool   is_root   = !bl->parent;
        double score     = 0;
        ///
        while (n) { ///
            auto sc = bl.score(n);
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
            bl = block(bl->parent);
        } else {
            /// style not matched
            bl = null;
            total_score = 0;
        }
    }
    return total_score;
}

/// compute available entries for props on a node
style::style_map style::compute(node *n) {
    style_map avail(16);
    ///
    for (type_t type = n->mem->type; type; type = type->parent) {
        if (!type->meta || !type->schema || !type->parent)
            continue;
        ///
        for (prop &p: *(doubly<prop>*)type->meta) {
            array<entry *> all = applicable(n, &p);
            if (all) {
                str   s_name  = p.key->grab(); /// it will free if you dont grab it
                avail[s_name] = all;
            }
        }
    }
    return avail;
}

void style::cache_members() {
    lambda<void(block &)> cache_b;
    ///
    cache_b = [&](block &bl) -> void {
        for (entry &e: bl->entries) {
            bool  found = false;
            ///
            array<block> &cache = members(e->member);
            for (block &cb:cache)
                 found |= cb == bl;
            ///
            if (!found)
                 cache += bl;
        }
        for (block &s:bl->blocks)
            cache_b(s);
    };
    if (data->root)
        for (block &b:data->root)
            cache_b(b);
}

void style::load(str code) {
    for (cstr sc = code.cs(); ws(sc); sc++) {
        lambda<void(block)> parse_block;
        ///
        parse_block = [&](block bl) {
            ws(sc);
            console.test(*sc == '.' || isalpha(*sc), "expected Type[.id], or .id");
            bl->quals = parse_qualifiers(bl, &sc);
            ws(++sc);
            ///
            while (*sc && *sc != '}') {
                /// read up to ;, {, or }
                ws(sc);
                cstr start = sc;
                console.test(scan_to(sc, {';', '{', '}'}), "expected member expression or qualifier");
                if (*sc == '{') {
                    ///
                    block &bl_n = bl->blocks->push();
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
                    if (param) {
                        int test = 0;
                        test++;
                    }
                    style::transition trans = param ? style::transition(param) : null;
                    
                    /// check
                    console.test(member, "member cannot be blank");
                    console.test(value,  "value cannot be blank");
                    bl->entries += entry::edata { member, value, trans, &bl };
                    ws(++sc);
                }
            }
            console.test(!*sc || *sc == '}', "expected closed-brace");
        };
        ///
        block &n_block = data->root.push_default();
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
                st.load(style_str);
            });

        /// store blocks by member, the interface into all style: style::members[name]
        st.cache_members();
        st->loaded = true;
    }
    return st;
}

style::entry *style::best_match(node *n, prop *member, array<style::entry*> &entries) {
    mx         s_member = member->key;
    array<style::block> &blocks = members(s_member); /// instance function when loading and updating style, managed map of [style::block*]
    style::entry *match = null; /// key is always a symbol, and maps are keyed by symbol
    real     best_score = 0;

    /// should narrow down by type used in the various blocks by referring to the qualifier
    /// find top style pair for this member
    type_t type = n->mem->type;
    for (style::entry *e: entries) {
        block  *bl = (*e)->bl;
        real score = bl->match(n);
        if (score > 0 && score >= best_score) {
            match = bl->b_entry(member->key);
            best_score = score;
        }
    }
    return match;
}

/// todo: move functions into itype pattern
array<style::entry*> style::applicable(node *n, prop *member) {
    mx         s_member = member->key;
    array<style::block> &blocks = members(s_member);
    array<style::entry*> result; 

    type_t type = n->mem->type;
    for (style::block &block:blocks)
        if (!block->types || block->types.index_of(type) >= 0)
            if (block.match(n) > 0)
                result += block.b_entry(member->key);

    return result;
}

}
