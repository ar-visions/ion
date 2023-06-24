
#include <ux/ux.hpp>
#include <vke/vke.h>
#include <array>
#include <set>
#include <stack>
#include <queue>

static VkeApp vke_app;

namespace ion {

using VkAttribs = array<VkVertexInputAttributeDescription>;

VkDescriptorSetLayoutBinding Asset_descriptor(Asset::etype t) {
    return { uint32_t(t), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, nullptr };
}

/// VkAttribs (array of VkAttribute) [data] from VAttribs (state flags) [model]
/// import this format and its tangent / bi-tangent
void Vertex::attribs(VAttribs attr, void *res) {
    VkAttribs &vk_attr = (*(VkAttribs*)res = VkAttribs(6));
    if (attr[VA::Position])  vk_attr += { 0, 0, VK_FORMAT_R32G32B32_SFLOAT,    offsetof(Vertex,  pos)  };
    if (attr[VA::Normal])    vk_attr += { 1, 0, VK_FORMAT_R32G32B32_SFLOAT,    offsetof(Vertex,  norm) };
    if (attr[VA::UV])        vk_attr += { 2, 0, VK_FORMAT_R32G32_SFLOAT,       offsetof(Vertex,  uv)   };
    if (attr[VA::Color])     vk_attr += { 3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex,  clr)  };
    if (attr[VA::Tangent])   vk_attr += { 4, 0, VK_FORMAT_R32G32B32_SFLOAT,    offsetof(Vertex,  ta)   };
    if (attr[VA::BiTangent]) vk_attr += { 5, 0, VK_FORMAT_R32G32B32_SFLOAT,    offsetof(Vertex,  bt)   };
}

struct Internal;

struct window_data {
    VkeWindow        vke_window;
    VkePhyInfo       vke_phyinfo;
    VkeDevice        vke_device;
    size             sz;
    vec2             cursor;
    bool             headless;
    memory          *control;
    str              title;
    event            state;
    Texture          tx_skia;
    vec2             dpi_scale = { 1, 1 };
    bool             repaint;
    event            ev;
    lambda<void()>   fn_resize;
    bool             pristine;

    struct dispatchers {
        dispatch on_resize;
        dispatch on_char;
        dispatch on_button;
        dispatch on_cursor;
        dispatch on_key;
    } events;

    void button_event(states<mouse> mouse_state) {
        ev->buttons = mouse_state;
        events.on_button(ev); /// this should not compile?
    }

    void char_event(user::chr unicode) {
        ev->unicode = unicode;
        events.on_char(ev);
    }

    void wheel_event(vec2 wheel_delta) {
        ev->wheel_delta = wheel_delta;
        events.on_cursor(ev);
    }

    void key_event  (user::key::data key) {
        ev->key = key;
        events.on_cursor(ev);
    }

    /// app composer needs to bind to these
    void cursor_event(vec2 cursor) {
        ev->cursor = cursor;
        events.on_cursor(ev);
    }

    void resize_event(size resize) {
        ev->resize = resize;
        events.on_resize(ev);
    }

    ~window_data() {
        if (vke_window) {
            vke_destroy_window(vke_window);
        }
    }
};

ptr_impl(window, mx, window_data, w);

/// remove void even in lambda
/// [](args) is fine.  even (args) is fine
void window::repaint() {

}

size window::size() { return w->sz; }

vec2 window::cursor() {
    return w->cursor;
}

str window::clipboard() {
    return vke_clipboard(w->vke_window);
}

void window::set_clipboard(str text) {
    vke_set_clipboard_string(w->vke_window, text.cs());
}

void window::set_title(str s) {
    w->title = s;
    vke_set_window_title(w->vke_window, w->title.cs());
}

void window::show() { vke_show_window(w->vke_window); }
void window::hide() { vke_hide_window(w->vke_window); }

window::operator bool() { return bool(w->vke_window); }

listener &dispatch::listen(callback cb) {
    m.listeners   += listener({ this, cb });
    listener &last = m.listeners.last(); /// last is invalid offset

    last->detatch = fn_stub([ls_mem=last.mem, listeners=&m.listeners]() -> void {
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
    for (listener &l: m.listeners)
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

enums(ShadeModule, Undefined,
    "Undefined, Vertex, Fragment, Compute",
     Undefined, Vertex, Fragment, Compute);

VkSampleCountFlagBits max_sampling(VkPhysicalDevice gpu);

/**
Device *create_device(GPU &p_gpu, bool aa) {
    VkSampleCountFlagBits max_bits = max_sampling(p_gpu); /// max is 4bit on this little m2, its 8 on the AMD R580
    Device  *dmem = new Device {
        .sampling = max_bits,
        .gpu      = p_gpu
    };
    auto qcreate        = array<VkDeviceQueueCreateInfo>(2);
    float priority      = 1.0f;
    uint32_t i_gfx      = dmem->gpu.index(GPU::Graphics);
    uint32_t i_present  = dmem->gpu.index(GPU::Present);
    qcreate            += VkDeviceQueueCreateInfo  { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, null, 0, i_gfx,     1, &priority };
    if (i_present != i_gfx)
        qcreate        += VkDeviceQueueCreateInfo  { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, null, 0, i_present, 1, &priority };
    auto features       = VkPhysicalDeviceFeatures { .logicOp = VK_TRUE, .samplerAnisotropy = aa ? VK_TRUE : VK_FALSE };
    bool is_apple = false;
    ///
    /// silver: macros should be better, not removed. wizard once told me.
#ifdef __APPLE__
    is_apple = true;
#endif
    array<cchar_t *> ext = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        "VK_KHR_portability_subset"
    };

    VkDeviceCreateInfo ci {};
#if !defined(NDEBUG)
    //ext += VK_EXT_DEBUG_UTILS_EXTENSION_NAME; # not supported on macOS with lunarg vk
    static cchar_t *debug   = "VK_LAYER_KHRONOS_validation";
    ci.enabledLayerCount       = 1;
    ci.ppEnabledLayerNames     = &debug;
#endif
    ci.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    ci.queueCreateInfoCount    = uint32_t(qcreate.len());
    ci.pQueueCreateInfos       = qcreate.data();
    ci.pEnabledFeatures        = &features; /// logicOp wasnt enabled
    ci.enabledExtensionCount   = uint32_t(ext.len() - size_t(!is_apple));
    ci.ppEnabledExtensionNames = ext.data();

    auto res = vkCreateDevice(dmem->gpu, &ci, nullptr, &dmem->device);
    assert(res == VK_SUCCESS);
    vkGetDeviceQueue(dmem->device, dmem->gpu.index(GPU::Graphics), 0, &dmem->queues[GPU::Graphics]); /// switch between like-queues if this is a hinderance
    vkGetDeviceQueue(dmem->device, dmem->gpu.index(GPU::Present),  0, &dmem->queues[GPU::Present]);
    /// create command pool (i think the pooling should be on each Frame)
    auto pool_info = VkCommandPoolCreateInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = dmem->gpu.index(GPU::Graphics)
    };
    assert(vkCreateCommandPool(dmem->device, &pool_info, nullptr, &dmem->command) == VK_SUCCESS);
    return dmem;
}
*/


void window::loop(lambda<void()> fn) {
    while (!vke_should_close(w->vke_window)) {
        if(!w->pristine) {
            fn();
            w->pristine = true;
        }
        vke_poll_events();
    }
}

VkSampleCountFlagBits max_sampling(VkPhysicalDevice gpu) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(gpu, &props);
    VkSampleCountFlags counts = props.limits.framebufferColorSampleCounts &
                                props.limits.framebufferDepthSampleCounts;
    if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
    if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
    if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
    if (counts & VK_SAMPLE_COUNT_8_BIT)  { return VK_SAMPLE_COUNT_8_BIT;  }
    if (counts & VK_SAMPLE_COUNT_4_BIT)  { return VK_SAMPLE_COUNT_4_BIT;  }
    if (counts & VK_SAMPLE_COUNT_2_BIT)  { return VK_SAMPLE_COUNT_2_BIT;  }
    return VK_SAMPLE_COUNT_1_BIT;
}

str terminal::ansi_color(rgba &c, bool text) {
    map<str> &map = text ? t_text_colors_8 : t_bg_colors_8;
    if (c->a < 32)
        return "";
    str hex = str("#") + str(c);
    return map.count(hex) ? map[hex] : "";
}

void terminal::draw_state_change(draw_state &ds, cbase::state_change type) {
    t.ds = &ds;
    switch (type) {
        case cbase::state_change::push:
            break;
        case cbase::state_change::pop:
            break;
        default:
            break;
    }
}

terminal::terminal(ion::size sz) : terminal()  {
    m.size    = sz;
    m.pixel_t = typeof(char);  
    size_t n_chars = sz.area();
    t.glyphs = array<glyph>(sz);
    for (size_t i = 0; i < n_chars; i++)
        t.glyphs += glyph {};
}

void *terminal::data() { return t.glyphs.data(); }

void terminal::set_char(int x, int y, glyph gl) {
    draw_state &ds = *t.ds;
    ion::size sz = cbase::size();
    if (x < 0 || x >= sz[1]) return;
    if (y < 0 || y >= sz[0]) return;
    //assert(!ds.clip || sk_vshape_is_rect(ds.clip));
    size_t index  = y * sz[1] + x;
    t.glyphs[{y, x}] = gl;
    /*
    ----
    change-me: dont think this is required on 'set', but rather a filter on get
    ----
    if (cg.border) t.glyphs[index]->border  = cg->border;
    if (cg.chr) {
        str  gc = t.glyphs[index].chr;
        if ((gc == "+") || (gc == "|" && cg.chr == "-") ||
                            (gc == "-" && cg.chr == "|"))
            t.glyphs[index].chr = "+";
        else
            t.glyphs[index].chr = cg.chr;
    }
    if (cg.fg) t.glyphs[index].fg = cg.fg;
    if (cg.bg) t.glyphs[index].bg = cg.bg;
    */
}

// gets the effective character, including bordering
str terminal::get_char(int x, int y) {
    cbase::draw_state &ds = *t.ds;
    ion::size   &sz = m.size;
    size_t       ix = math::clamp<size_t>(x, 0, sz[1] - 1);
    size_t       iy = math::clamp<size_t>(y, 0, sz[0] - 1);
    str       blank = " ";
    
    auto get_str = [&]() -> str {
        auto value_at = [&](int x, int y) -> glyph * {
            if (x < 0 || x >= sz[1]) return null;
            if (y < 0 || y >= sz[0]) return null;
            return &t.glyphs[y * sz[1] + x];
        };
        
        auto is_border = [&](int x, int y) {
            glyph *cg = value_at(x, y);
            return cg ? ((*cg)->border > 0) : false;
        };
        
        glyph::members &cg = t.glyphs[iy * sz[1] + ix];
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

void terminal::text(str s, graphics::shape vrect, alignment::data &align, vec2 voffset, bool ellip) {
    r4<real>    rect   =  vrect.bounds();
    ion::size    &sz   =  cbase::size();
    v2<real>   &offset =  voffset;
    draw_state &ds     = *t.ds;
    int         len    =  int(s.len());
    int         szx    = int(sz[1]);
    int         szy    = int(sz[0]);
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
    int tx = align.x == xalign::left   ? (x0) :
                align.x == xalign::middle ? (x0 + (x1 - x0) / 2) - int(std::ceil(len / 2.0)) :
                align.x == xalign::right  ? (x1 - len) : (x0);
    ///
    int ty = align.y == yalign::top    ? (y0) :
                align.y == yalign::middle ? (y0 + (y1 - y0) / 2) - int(std::ceil(len / 2.0)) :
                align.y == yalign::bottom ? (y1 - len) : (y0);
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
        str     chr  = s.mid(i, 1);
        set_char(tx + int(i), ty,
            glyph::members {
                0, chr, {0,0,0,0}, rgba { ds.color }
            });
    }
}

void terminal::outline(graphics::shape sh) {
    draw_state &ds = *t.ds;
    assert(sh);
    r4r     &r = sh.bounds();
    int     ss = math::min(2.0, math::round(ds.outline_sz));
    c4<u8>  &c = ds.color;
    glyph cg_0 = glyph::members { ss, "-", null, rgba { c.r, c.g, c.b, c.a }};
    glyph cg_1 = glyph::members { ss, "|", null, rgba { c.r, c.g, c.b, c.a }};
    
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
    draw_state &ds = *t.ds;
    ion::size  &sz = cbase::size();
    int         sx = int(sz[1]);
    int         sy = int(sz[0]);
    if (ds.color) {
        r4r &r       = sh.bounds();
        str  t_color = ansi_color(ds.color, false);
        ///
        int      x0 = math::max(int(0),        int(r.x)),
                    x1 = math::min(int(sx - 1),   int(r.x + r.w));
        int      y0 = math::max(int(0),        int(r.y)),
                    y1 = math::min(int(sy - 1),   int(r.y + r.h));

        glyph    cg = glyph::members { 0, " ", rgba(ds.color), null }; /// members = data

        for (int x = x0; x <= x1; x++)
            for (int y = y0; y <= y1; y++)
                set_char(x, y, cg); /// just set mem w grab()
    }
}

struct memory;
struct Texture;

/**
 * integrate similarly:
VkShaderModule Device::module(Path path, Assets &assets, ShadeModule type) {
    str    key = path;
    auto &modules = type == ShadeModule::Vertex ? v_modules : f_modules;
    Path   spv = fmt {"{0}.spv", { key }};
    
#if !defined(NDEBUG)
    if (!spv.exists() || path.modified_at() > spv.modified_at()) {
        if (modules.count(key))
            modules.remove(key);
        str   defs = "";
        auto remap = map<str> {
            { Asset::Color,    " -DCOLOR"    },
            { Asset::Specular, " -DSPECULAR" },
            { Asset::Displace, " -DDISPLACE" },
            { Asset::Normal,   " -DNORMAL"   }
        };
        
        for (auto &[type, tx]: assets) /// always use type when you can, over id (id is genuinely instanced things)
            defs += remap[type];
        
        
        /// path was /usr/local/bin/ ; it should just be in path, and to support native windows        
        exec command = fmt {"glslc {1} {0} -o {0}.spv", { key, defs }};
        async run { command };
        int  code = int(run.sync()[0]);
        Path tmp  = "./.tmp/"; ///
        if (spv.exists() && spv.modified_at() >= path.modified_at()) {
            spv.copy(tmp);
        } else {
            // look for tmp. if this does not exist we can not proceed.
            // compilation failure, so look for temp which worked before.
        }
        
        /// if it succeeds, the spv is written and that will have a greater modified-at
    }
#endif

    if (!modules.count(key)) {
        auto mc     = VkShaderModuleCreateInfo { };
        str code    = str::read_file(spv);
        mc.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        mc.codeSize = code.len();
        mc.pCode    = reinterpret_cast<const uint32_t *>(code.cs());
        assert (vkCreateShaderModule(device, &mc, null, &modules[key]) == VK_SUCCESS);
    }
    return modules[key];
}
*/

//#include <gpu/gl/GrGLInterface.h>

struct Internal {
    window  *win;
    VkInstance vk = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT dmsg;
    array<VkLayerProperties> layers;
    bool     resize;
    array<VkFence> fence;
    array<VkFence> image_fence;
    Internal &bootstrap();
    void destroy();
    static Internal &handle();
};

VkeApp vke_bootstrap() {
    if (!vke_app) {
        static const symbol validation_layer = "VK_LAYER_KHRONOS_validation";
        array<symbol> extensions(32);

        extensions += "VK_KHR_surface";
        extensions += "VK_KHR_portability_enumeration";

        if      constexpr (is_debug()) { extensions += VK_EXT_DEBUG_UTILS_EXTENSION_NAME; }
        if      constexpr (is_apple()) { extensions += "VK_MVK_macos_surface"; }
        else if constexpr (is_win())   { extensions += "VK_KHR_win32_surface"; }
        else                           { extensions += "VK_KHR_xcb_surface";   }

        extensions += symbol(null);
        vke_app     = vke_app_create(1, 2, "ion:ux", 1, &validation_layer, extensions.data(), extensions.count());
    }
    return vke_app;
}

#if 0
Assets Pipes::cache_assets(Pipes::data &d, str model, str skin, states<Asset> &atypes) {
    auto   &cache = d.device->tx_cache; /// ::map<Path, Device::Pair>
    Assets assets = Assets(Asset::count);
    ///
    auto load_tx = [&](Path p) -> Texture * {
        /// load texture if not already loaded
        if (!cache.count(p)) {
            cache[p].img      = new image(p);
            cache[p].texture  = new Texture {
                new TextureMemory { d.device, *cache[p].img,
                    VK_IMAGE_USAGE_SAMPLED_BIT      | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                    VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_COLOR_BIT, false, VK_FORMAT_R8G8B8A8_UNORM, -1
                }
            };
            cache[p].texture->push_stage(Texture::Stage::Shader);
        };
        return cache[p].texture;
    };
    
    #if 0
    static auto names = ::map<str> {
        { Asset::Color,    "color"    },
        { Asset::Specular, "specular" },
        { Asset::Displace, "displace" },
        { Asset::Normal,   "normal"   }
    };
    #endif

    doubly<memory*> &enums = Asset::symbols();
    ///
    for (memory *sym:enums) {
        str         name = sym->symbol(); /// no allocation and it just holds onto this symbol memory, not mutable
        Asset type { Asset::etype(sym->id) };
        if (!atypes[type]) continue;
        /// prefer png over jpg, if either one exists
        Path png = form_path(model, skin, ".png");
        Path jpg = form_path(model, skin, ".jpg");
        if (!png.exists() && !jpg.exists()) continue;
        assets[type] = load_tx(png.exists() ? png : jpg);
    }
    return assets;
}
#endif

#if 0
/// todo: vulkan render to texture
void sk_canvas_gaussian(sk_canvas_data* sk_canvas, v2r* sz, r4r* crop) {
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

struct gfx_memory {
    VkvgSurface    vg_surface;
    VkvgDevice     vg_device;
    VkvgContext    ctx;
    VkeDevice      vke_device;
    VkeImage       vke_image;
    str            font_default;
    window        *win;
    Texture        tx;
    cbase::draw_state *ds;

    ~gfx_memory() {
        if (ctx)        vkvg_drop        (ctx);
        if (vg_device)  vkvg_device_drop(vg_device);
        if (vg_surface) vkvg_surface_drop(vg_surface);
    }
};

//ptr_impl(gfx, cbase,  gfx_memory, g);

gfx::gfx(memory* mem)  : cbase(mem), g(mem ? mx::data<gfx_memory>() : null) { }\
gfx::gfx(mx      o)    : gfx(o.mem->grab())   { }\
gfx::gfx()             : gfx(mx::alloc<gfx>()) { }\
gfx::gfx(gfx_memory *data) : gfx(memory::wrap<gfx_memory>(data, 1)) { }\
gfx::operator gfx_memory * () { return g; }\
gfx &gfx::operator=(const gfx b) { return (gfx &)assign_mx(*this, b); }\
gfx_memory *gfx::operator=(const gfx_memory *b) {\
    drop();\
    mem = memory::wrap<gfx_memory>((gfx_memory*)b, 1);\
    g = (gfx_memory*)mem->origin;\
    return g;\
}

/// 
gfx::gfx(ion::window &win) : gfx(mx::alloc<gfx>()) { /// this allocates both gfx_memory and cbase::cdata memory (cbase has data type aliased at cbase::DC)
    g->win = &win;
    m.size = win.w->sz;
    assert(m.size[1] > 0 && m.size[0] > 0);
    ///
    g->tx = win.texture(m.size); 
    TextureMemory *tmem = g->tx;

    /// not sure if the import is getting the VK_SAMPLE_COUNT_8_BIT and VkSampler, likely not.
    
    g->vg_device        = vkvg_device_create_multisample(win->vke_device, VK_SAMPLE_COUNT_8_BIT);
    g->vke_image        = vke_image_import(win->vke_device, tmem->vk_image, tmem->format, u32(tmem->sz[1]), u32(tmem->sz[0]));

    vke_image_create_view(g->vke_image, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT); // VK_IMAGE_ASPECT_COLOR_BIT questionable bit
    g->vg_surface       = vkvg_surface_create_for_VkhImage(g->vg_device, (void*)g->vke_image); // attachment #0 in VkFramebufferCreateInfo has 8bit samples that do not match VkRenderPass's attachment at same rank
    g->ctx              = vkvg_create(g->vg_surface);
    push(); /// gfx just needs a push off the ledge. [/penguin-drops]
    defaults();
}

window::window(ion::size sz, mode::etype wmode, memory *control) : window() {
    VkInstance vk = Internal::handle().vk;
    w->sz          = sz;
    w->headless    = wmode == mode::headless;
    w->control     = control->grab();
    w->glfw        = glfwCreateWindow(int(sz[1]), int(sz[0]), (symbol)"ion:ux", null, null);
    w->vk_surface  = 0;
    
    VkResult code = glfwCreateWindowSurface(vk, w->glfw, null, &w->vk_surface);

    console.test(code == VK_SUCCESS, "initial surface creation failure.");

    /// set glfw callbacks
    glfwSetWindowUserPointer(w->glfw, (void*)mem->grab()); /// this is another reference on the window, which must be unreferenced when the window closes
    glfwSetErrorCallback    (glfw_error);

    if (!w->headless) {
        glfwSetFramebufferSizeCallback(w->glfw, glfw_resize);
        glfwSetKeyCallback            (w->glfw, glfw_key);
        glfwSetCursorPosCallback      (w->glfw, glfw_cursor);
        glfwSetCharCallback           (w->glfw, glfw_char);
        glfwSetMouseButtonCallback    (w->glfw, glfw_button);
    }

    array<VkPhysicalDevice> hw = available_gpus();

    /// a fast algorithm for choosing top gpu
    const size_t top_pick = 0; 
    w->gpu = create_gpu(hw[top_pick], w->vk_surface);
    
    /// create device with window, selected gpu and surface
    w->dev = create_device(w->gpu, true);
    w->dev->initialize(w);
}

void vkvg_path(VkvgContext ctx, memory *mem) {
    type_t ty = mem->type;
    
    if (ty == typeof(r4<real>)) {
        r4<real> &m = mem->ref<r4<real>>();
        vkvg_rectangle(ctx, m.x, m.y, m.w, m.h);
    }
    else if (ty == typeof(graphics::rounded::data)) {
        graphics::rounded::data &m = mem->ref<graphics::rounded::data>();
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
    else if (ty == typeof(graphics::shape::data)) {
        graphics::shape::data &m = mem->ref<graphics::shape::data>();
        for (mx &o:m.ops) {
            type_t t = o.type();
            if (t == typeof(graphics::movement)) {
                graphics::movement m(o);
                vkvg_move_to(ctx, m.data.x, m.data.y); /// todo: convert vkvg to double.  simply not using float!
            } else if (t == typeof(vec2)) {
                vec2 l(o);
                vkvg_line_to(ctx, l.data.x, l.data.y);
            }
        }
        vkvg_close_path(ctx);
    }
}

window &gfx::window() { return *g->win; }
Device &gfx::device() { return *g->win->w->dev; }

/// Quake2: { computer. updated. }
void gfx::draw_state_change(draw_state *ds, cbase::state_change type) {
    g->ds = ds;
}

text_metrics gfx::measure(str text) {
    vkvg_text_extents_t ext;
    vkvg_font_extents_t tm;
    ///
    vkvg_text_extents(g->ctx, (symbol)text.cs(), &ext);
    vkvg_font_extents(g->ctx, &tm);
    return text_metrics::data {
        .w           =  real(ext.x_advance),
        .h           =  real(ext.y_advance),
        .ascent      =  real(tm.ascent),
        .descent     =  real(tm.descent),
        .line_height =  real(g->ds->line_height)
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
        vkvg_show_text(g->ctx, (symbol)draw.cs());
}

void gfx::image(ion::image img, graphics::shape sh, vec2 align, vec2 offset, vec2 source) {
    attachment *att = img.find_attachment("vg-surf");
    if (!att) {
        VkvgSurface surf = vkvg_surface_create_from_bitmap(
            g->vg_device, (uint8_t*)img.pixels(), u32(img.width()), u32(img.height()));
        att = img.attach("vg-surf", surf, [surf]() {
            vkvg_surface_drop(surf);
        });
        assert(att);
    }
    VkvgSurface surf = (VkvgSurface)att->data;
    draw_state   &ds = *g->ds;
    ion::size     sz = img.shape();
    r4r           &r = sh.bounds();
    assert(surf);
    vkvg_set_source_rgba(g->ctx,
        ds.color->r, ds.color->g, ds.color->b, ds.color->a * ds.opacity);
    
    /// now its just of matter of scaling the little guy to fit in the box.
    v2<real> vsc = { math::min(1.0, r.w / real(sz[1])), math::min(1.0, r.h / real(sz[0])) };
    real      sc = (vsc.y > vsc.x) ? vsc.x : vsc.y;
    vec2     pos = { mix(r.x, r.x + r.w - sz[1] * sc, align.data.x),
                        mix(r.y, r.y + r.h - sz[0] * sc, align.data.y) };
    
    push();
    /// translate & scale
    translate(pos + offset);
    scale(sc);
    /// push path
    vkvg_rectangle(g->ctx, r.x, r.y, r.w, r.h);
    /// color & fill
    vkvg_set_source_surface(g->ctx, surf, source.data.x, source.data.y);
    vkvg_fill(g->ctx);
    pop();
}

void gfx::push() {
    cbase::push();
    vkvg_save(g->ctx);
}

void gfx::pop() {
    cbase::pop();
    vkvg_restore(g->ctx);
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
void gfx::text(str text, graphics::rect rect, alignment align, vec2 offset, bool ellip) {
    draw_state &ds = *g->ds;
    rgba::data & c = ds.color;
    ///
    vkvg_save(g->ctx);
    vkvg_set_source_rgba(g->ctx, c.r, c.g, c.b, c.a * ds.opacity);
    ///
    v2r            pos  = { 0, 0 };
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
    v2r           tl = { rect->x,  rect->y + line_height / 2 };
    v2r           br = { rect->x + rect->w - tm->w,
                            rect->y + rect->h - tm->h - line_height / 2 };
    v2r           va = vec2(align);
    pos              = mix(tl, br, va);
    
    vkvg_show_text(g->ctx, (symbol)text.cs());
    vkvg_restore  (g->ctx);
}

void gfx::clip(graphics::shape cl) {
    draw_state  &ds = cur();
    ds.clip  = cl;
    vkvg_path(g->ctx, cl.mem);
    vkvg_clip(g->ctx);
}

Texture gfx::texture() { return g->tx; } /// associated with surface

void gfx::flush() {
    vkvg_flush(g->ctx);
}

void gfx::clear(rgba c) {
    vkvg_save           (g->ctx);
    vkvg_set_source_rgba(g->ctx, c->r, c->g, c->b, c->a);
    vkvg_paint          (g->ctx);
    vkvg_restore        (g->ctx);
}

void gfx::font(ion::font f) {
    vkvg_select_font_face(g->ctx, f->alias.cs());
    vkvg_set_font_size   (g->ctx, f->sz);
}

void gfx::cap  (graphics::cap   c) { vkvg_set_line_cap (g->ctx, vkvg_line_cap_t (int(c))); }
void gfx::join (graphics::join  j) { vkvg_set_line_join(g->ctx, vkvg_line_join_t(int(j))); }
void gfx::translate(vec2       tr) { vkvg_translate    (g->ctx, tr->x, tr->y);  }
void gfx::scale    (vec2       sc) { vkvg_scale        (g->ctx, sc->x, sc->y);  }
void gfx::scale    (real       sc) { vkvg_scale        (g->ctx, sc, sc);        }
void gfx::rotate   (real     degs) { vkvg_rotate       (g->ctx, radians(degs)); }
void gfx::fill(graphics::shape  p) {
    vkvg_path(g->ctx, p.mem);
    vkvg_fill(g->ctx);
}

void gfx::gaussian (vec2 sz, graphics::shape c) { }

void gfx::outline  (graphics::shape p) {
    vkvg_path(g->ctx, p.mem);
    vkvg_stroke(g->ctx);
}

void*    gfx::data      ()                   { return null; }
str      gfx::get_char  (int x, int y)       { return str(); }
str      gfx::ansi_color(rgba &c, bool text) { return str(); }

ion::image gfx::resample(ion::size sz, real deg, graphics::shape view, vec2 axis) {
    c4<u8> *pixels = null;
    int scanline = 0;
    return ion::image(sz, (rgba::data*)pixels, scanline);
}

int app::run() {
    /// this was debugging a memory issue
    m.win    = window {size { 512, 512 }, mode::regular, mx::mem };
    m.canvas = gfx(m.win);
    Device &dev = m.canvas.device();
    window &win = m.canvas.window();

    ///
    auto   vertices = Vertex::square (); /// doesnt always get past this.
    auto    indices = array<uint16_t> { 0, 1, 2, 2, 3, 0 };

    auto   textures = Assets {{ Asset::Color, &win.texture() }}; /// quirk in that passing Asset::Color enumerable does not fetch memory with id, but rather memory of int(id)
    auto      vattr = VAttribs { VA::Position, VA::Normal, VA::UV, VA::Color };
    auto        vbo = VertexBuffer<Vertex>  { &dev, vertices, vattr };
    auto        ibo = IndexBuffer<uint16_t> { &dev, indices  };
    auto        uni = UniformBuffer<MVP>    { &dev, [&](void *mvp) {
        *(MVP*)mvp  = MVP {
             .model = m44f::ident(), //m44f::identity(),
             .view  = m44f::ident(), //m44f::identity(),
             .proj  = m44f::ortho({ -0.5f, 0.5f }, { -0.5f, 0.5f }, { 0.5f, -0.5f })
        };
    }}; /// assets dont seem to quite store Asset::Color and its texture ref
    PipelineData pl = Pipeline<Vertex> {
        &dev, uni, vbo, ibo, textures,
        { 0.0, 0.0, 0.0, 0.0 }, std::string("main") /// transparent canvas overlay; are we clear? crystal.
    };
    listener &on_key = win.w->events.on_key.listen(
        [&](event e) {
            win.w->pristine = false;
            win.w->ev->key  = e->key; /// dont need to store button_id, thats just for handlers. window is stateless
        });
    listener &on_button = win.w->events.on_button.listen(
        [&](event e) {
            win.w->pristine    = false;
            win.w->ev->buttons = e->buttons;
        });
    listener &on_cursor = win->events.on_cursor.listen(
        [&](event e) {
            win.w->pristine   = false;
            win.w->ev->cursor = e->cursor;
        });
    
    memory *pmem = pl.mem;

    ///
    win.show();
    
    /// prototypes add a Window&
    /// w->fn_cursor  = [&](double x, double y)         { composer->cursor(w, x, y);    };
    /// w->fn_mbutton = [&](int b, int a, int m)        { composer->button(w, b, a, m); };
    /// w->fn_resize  = [&]()                           { composer->resize(w);          };
    /// w->fn_key     = [&](int k, int s, int a, int m) { composer->key(w, k, s, a, m); };
    /// w->fn_char    = [&](uint32_t c)                 { composer->character(w, c);    };
    
    /// uniforms are meant to be managed by the app, passed into pipelines.
    win.loop([&]() {
        Element e = m.app_fn(*this);
        //canvas.clear(rgba {0.0, 0.0, 0.0, 0.0});
        //canvas.flush();

        // this should only paint the 3D pipeline, no canvas
        //i.tx_skia.push_stage(Texture::Stage::Shader);
        //dev.render.push(pl); /// Any 3D object would have passed its pipeline prior to this call (in render)
        //dev.render.present();
        //i.tx_skia.pop_stage();
        
        //if (composer->d.root) w->set_title(composer->d.root->m.text.label);

        // process animations
        //composer->process();
        
        //glfwWaitEventsTimeout(1.0);
    });

    /// automatic when it falls out of scope
    on_cursor.cancel();

    glfwTerminate();
    return 0;
}


/*
    member data: (member would be map<style_value> )

    StyleValue
        NMember<T, MT> *host         = null; /// needs to be 
        transition     *trans        = null;
        bool            manual_trans = false;
        int64_t         timer_start  = 0;
        sh<T>           t_start;       // transition start value (could be in middle of prior transition, for example, whatever its start is)
        sh<T>           t_value;       // current transition value
        sh<T>           s_value;       // style value
        sh<T>           d_value;       // default value

    size_t        cache = 0;
    sh<T>         d_value;
    StyleValue    style;
*/

// we just need to make compositor with transitions a bit cleaner than previous attempt.  i want to use all mx-pattern




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

doubly<style::qualifier> parse_qualifiers(cstr *p) {
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
        style::qualifier &v = result.push();
        int idot = q.index_of(".");
        int icol = q.index_of(":");
        str tail;
        ///
        if (idot >= 0) {
            array<str> sp = q.split(".");
            v.data.type   = sp[0];
            v.data.id     = sp[1];
            if (icol >= 0)
                tail  = q.mid(icol + 1).trim();
        } else {
            if (icol  >= 0) {
                v.data.type = q.mid(0, icol);
                tail   = q.mid(icol + 1).trim();
            } else
                v.data.type = q;
        }
        array<str> ops {"!=",">=","<=",">","<","="};
        if (tail) {
            // check for ops
            bool is_op = false;
            for (str &op:ops) {
                if (tail.index_of(op.cs()) >= 0) {
                    is_op   = true;
                    auto sp = tail.split(op);
                    v.data.state = sp[0].trim();
                    v.data.oper  = op;
                    v.data.value = tail.mid(sp[0].len() + op.len()).trim();
                    break;
                }
            }
            if (!is_op)
                v.data.state = tail;
        }
    }
    *p = end;
    return result;
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
    if (m.root)
        for (block &b:m.root)
            cache_b(b);
}

style::style(str code) : style(mx::alloc<style>()) {
    ///
    if (code) {
        for (cstr sc = code.cs(); ws(sc); sc++) {
            lambda<void(block)> parse_block;
            ///
            parse_block = [&](block bl) {
                ws(sc);
                console.test(*sc == '.' || isalpha(*sc), "expected Type, or .name");
                bl->quals = parse_qualifiers(&sc);
                ws(++sc);
                ///
                while (*sc && *sc != '}') {
                    /// read up to ;, {, or }
                    ws(sc);
                    cstr start = sc;
                    console.test(scan_to(sc, {';', '{', '}'}), "expected member expression or qualifier");
                    if (*sc == '{') {
                        ///
                        block &bl_n = bl->blocks.push();
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
                        
                        /// this should use the regex standard api, will convert when its feasible.
                        str  cb_value = str(vstart, std::distance(vstart, cur)).trim();
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
                        style::transition trans = param  ? style::transition(param) : null;
                        
                        /// check
                        console.test(member, "member cannot be blank");
                        console.test(value,  "value cannot be blank");
                        bl->entries += entry::data { member, value, trans };
                        ws(++sc);
                    }
                }
                console.test(!*sc || *sc == '}', "expected closed-brace");
            };
            ///
            block &n_block = m.root.push_default();
            parse_block(n_block);
        }
        /// store blocks by member, the interface into all style: style::members[name]
        cache_members();
    }
}

style style::load(path p) {
    return cache.count(p) ? cache[p] : (cache[p] = style(p));
}

style style::for_class(symbol class_name) {
    path p = str::format("style/{0}.css", { class_name });
    return load(p);
}


//void Vertex::calculate_tangents(Vertices &verts, array<ngon> &faces)

/*

vke can host a vertex api in C. we shouldnt!

void Vertex::calculate_tangents(Vertices& verts, mesh& input_mesh) {
    for (ngon& f: input_mesh) {
        for (u32 i = 0; i < f.size; i++) {
            /// indices for 3 verts from face
            u32 index1    = f.indices[i];
            u32 index2    = f.indices[(i + 1) % f.size];
            u32 index3    = f.indices[(i + 2) % f.size];
            ///
            v3f deltaPos1 = verts[index2].pos - verts[index1].pos;
            v3f deltaPos2 = verts[index3].pos - verts[index1].pos;
            v2f deltaUV1  = verts[index2].uv  - verts[index1].uv;
            v2f deltaUV2  = verts[index3].uv  - verts[index1].uv;

            /// set uv-weight
            float uv_weight = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
            
            /// calc tangent and bi-tangent
            v3f   ta {(deltaPos1 *  deltaUV2.y - deltaPos2 * deltaUV1.y) * uv_weight},
                  bt {(deltaPos1 * -deltaUV2.x + deltaPos2 * deltaUV1.x) * uv_weight};
            
            /// normalize as n_ta and n_bt
            v3f n_ta = ta.normalize();
            v3f n_bt = bt.normalize();
            
            /// accumulate n_ta and n_bt across 3 verts
            verts[index1].ta += n_ta;
            verts[index2].ta += n_ta;
            verts[index3].ta += n_ta;
            verts[index1].bt += n_bt;
            verts[index2].bt += n_bt;
            verts[index3].bt += n_bt;
        }
    }
    /// compute final product
    for (Vertex &v: verts) {
        v.ta = v.ta.normalize();
        v.bt = v.bt.normalize();
    }
}
*/

}
