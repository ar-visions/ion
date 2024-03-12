#pragma once
#include <mx/mx.hpp>
#include <async/async.hpp>
#include <watch/watch.hpp>
#include <media/media.hpp>
#include <media/image.hpp>
#include <ux/gltf.hpp>

struct GLFWwindow;

namespace ion {

struct IDawn;
struct Dawn:mx {
    void process_events();
    static float get_dpi();
    mx_declare(Dawn, mx, IDawn);
};

template <> struct is_singleton<IDawn> : true_type { };
template <> struct is_singleton<Dawn> : true_type { };

enums(Clear, Undefined, Undefined = 0, Color = 1, Depth = 2, Stencil = 4);

enums_declare(TextureFormat, Undefined, WGPUTextureFormat,
    Undefined = 0x00000000,
    R8Unorm = 0x00000001,
    R8Snorm = 0x00000002,
    R8Uint = 0x00000003,
    R8Sint = 0x00000004,
    R16Uint = 0x00000005,
    R16Sint = 0x00000006,
    R16Float = 0x00000007,
    RG8Unorm = 0x00000008,
    RG8Snorm = 0x00000009,
    RG8Uint = 0x0000000A,
    RG8Sint = 0x0000000B,
    R32Float = 0x0000000C,
    R32Uint = 0x0000000D,
    R32Sint = 0x0000000E,
    RG16Uint = 0x0000000F,
    RG16Sint = 0x00000010,
    RG16Float = 0x00000011,
    RGBA8Unorm = 0x00000012,
    RGBA8UnormSrgb = 0x00000013,
    RGBA8Snorm = 0x00000014,
    RGBA8Uint = 0x00000015,
    RGBA8Sint = 0x00000016,
    BGRA8Unorm = 0x00000017,
    BGRA8UnormSrgb = 0x00000018,
    RGB10A2Uint = 0x00000019,
    RGB10A2Unorm = 0x0000001A,
    RG11B10Ufloat = 0x0000001B,
    RGB9E5Ufloat = 0x0000001C,
    RG32Float = 0x0000001D,
    RG32Uint = 0x0000001E,
    RG32Sint = 0x0000001F,
    RGBA16Uint = 0x00000020,
    RGBA16Sint = 0x00000021,
    RGBA16Float = 0x00000022,
    RGBA32Float = 0x00000023,
    RGBA32Uint = 0x00000024,
    RGBA32Sint = 0x00000025,
    Stencil8 = 0x00000026,
    Depth16Unorm = 0x00000027,
    Depth24Plus = 0x00000028,
    Depth24PlusStencil8 = 0x00000029,
    Depth32Float = 0x0000002A,
    Depth32FloatStencil8 = 0x0000002B,
    BC1RGBAUnorm = 0x0000002C,
    BC1RGBAUnormSrgb = 0x0000002D,
    BC2RGBAUnorm = 0x0000002E,
    BC2RGBAUnormSrgb = 0x0000002F,
    BC3RGBAUnorm = 0x00000030,
    BC3RGBAUnormSrgb = 0x00000031,
    BC4RUnorm = 0x00000032,
    BC4RSnorm = 0x00000033,
    BC5RGUnorm = 0x00000034,
    BC5RGSnorm = 0x00000035,
    BC6HRGBUfloat = 0x00000036,
    BC6HRGBFloat = 0x00000037,
    BC7RGBAUnorm = 0x00000038,
    BC7RGBAUnormSrgb = 0x00000039,
    ETC2RGB8Unorm = 0x0000003A,
    ETC2RGB8UnormSrgb = 0x0000003B,
    ETC2RGB8A1Unorm = 0x0000003C,
    ETC2RGB8A1UnormSrgb = 0x0000003D,
    ETC2RGBA8Unorm = 0x0000003E,
    ETC2RGBA8UnormSrgb = 0x0000003F,
    EACR11Unorm = 0x00000040,
    EACR11Snorm = 0x00000041,
    EACRG11Unorm = 0x00000042,
    EACRG11Snorm = 0x00000043,
    ASTC4x4Unorm = 0x00000044,
    ASTC4x4UnormSrgb = 0x00000045,
    ASTC5x4Unorm = 0x00000046,
    ASTC5x4UnormSrgb = 0x00000047,
    ASTC5x5Unorm = 0x00000048,
    ASTC5x5UnormSrgb = 0x00000049,
    ASTC6x5Unorm = 0x0000004A,
    ASTC6x5UnormSrgb = 0x0000004B,
    ASTC6x6Unorm = 0x0000004C,
    ASTC6x6UnormSrgb = 0x0000004D,
    ASTC8x5Unorm = 0x0000004E,
    ASTC8x5UnormSrgb = 0x0000004F,
    ASTC8x6Unorm = 0x00000050,
    ASTC8x6UnormSrgb = 0x00000051,
    ASTC8x8Unorm = 0x00000052,
    ASTC8x8UnormSrgb = 0x00000053,
    ASTC10x5Unorm = 0x00000054,
    ASTC10x5UnormSrgb = 0x00000055,
    ASTC10x6Unorm = 0x00000056,
    ASTC10x6UnormSrgb = 0x00000057,
    ASTC10x8Unorm = 0x00000058,
    ASTC10x8UnormSrgb = 0x00000059,
    ASTC10x10Unorm = 0x0000005A,
    ASTC10x10UnormSrgb = 0x0000005B,
    ASTC12x10Unorm = 0x0000005C,
    ASTC12x10UnormSrgb = 0x0000005D,
    ASTC12x12Unorm = 0x0000005E,
    ASTC12x12UnormSrgb = 0x0000005F,
    R16Unorm = 0x00000060,
    RG16Unorm = 0x00000061,
    RGBA16Unorm = 0x00000062,
    R16Snorm = 0x00000063,
    RG16Snorm = 0x00000064,
    RGBA16Snorm = 0x00000065,
    R8BG8Biplanar420Unorm = 0x00000066,
    R10X6BG10X6Biplanar420Unorm = 0x00000067,
    R8BG8A8Triplanar420Unorm = 0x00000068,
    Force32 = 0x7FFFFFFF
)

enums(Key, undefined,
    undefined=0, Space=32, Apostrophe=39, Comma=44, Minus=45, Period=46, Slash=47,
    k_0=48, k_1=49, k_2=50, k_3=51, k_4=52, k_5=53, k_6=54, k_7=55, k_8=56, k_9=57,
    SemiColon=59, Equal=61,
    A=65, B=66, C=67, D=68, E=69, F=70, G=71, H=72, I=73, J=74, K=75, L=76, M=77,
    N=78, O=79, P=80, Q=81, R=82, S=83, T=84, U=85, V=86, W=87, X=88, Y=89, Z=90,
    LeftBracket=91, BackSlash=92, RightBracket=93, GraveAccent=96, World1 = 161, World2 = 162,
    Escape=256, ENTER=257, TAB=258, BACKSPACE=259, INSERT=260, DELETE=261, RIGHT=262, LEFT=263, DOWN=264, UP=265,
    PAGE_UP=266, PAGE_DOWN=267, HOME=268, END=269, CAPS_LOCK=280, SCROLL_LOCK=281, NUM_LOCK=282, PRINT_SCREEN=283, PAUSE=284,
    F1=290,  F2=291,  F3=292,  F4=293,  F5=294,  F6=295,  F7=296,  F8=297,  F9=298,  F10=299, F11=300, F12=301, F13=302, F14=303,
    F15=304, F16=305, F17=306, F18=307, F19=308, F20=309, F21=310, F22=311, F23=312, F24=313, F25=314,
    KP_0=320, KP_1=321, KP_2=322, KP_3=323, KP_4=324, KP_5=325, KP_6=326, KP_7=327, KP_8=328, KP_9=329,
    KP_DECIMAL=330, KP_DIVIDE=331, KP_MULTIPLY=332, KP_SUBTRACT=333, KP_ADD=334, KP_ENTER=335, KP_EQUAL=336,
    LEFT_SHIFT=340, LEFT_CONTROL=341, LEFT_ALT=342, LEFT_SUPER=343,
    RIGHT_SHIFT=344, RIGHT_CONTROL=345, RIGHT_ALT=346, RIGHT_SUPER=347, MENU=348
)

/// texture is using this; todo: make a dawn module
enums(Asset, undefined, 
     color, normal, material, reflect, env, undefined); /// populate from objects normal map first, and then adjust by equirect if its provided

struct Device;
struct Texture:mx {
    mx_declare(Texture, mx, struct ITexture);

    using OnTextureResize = lambda<void(vec2i)>;

    static Texture    load(Device &dev, symbol name, Asset type);
    static ion::image asset_image(symbol name, Asset type);
    static Texture    from_image(Device &dev, image img, Asset type);
    static Texture    of_size(Device &dev, vec2i size, TextureFormat f);
    
    void update(image img);
    void resize(vec2i sz);
    vec2i  size();
    void on_resize(str user, OnTextureResize fn);
    void* handle();
};

struct IDevice;
struct Device:mx {
    Texture         create_texture(vec2i sz);
    void*           handle();
    void            get_dpi(float *, float *);
    mx_declare(Device, mx, IDevice)
};

struct Pipeline;
struct Pipes;

enums(ShaderModule, undefined,
     undefined, vertex, fragment, compute);

using GraphicsGen = lambda<void(mx&,mx&,array<image>&)>;

struct GraphicsData {
    symbol      name;
    symbol      shader;
    type_t      vtype;
    GraphicsGen gen;
    array<mx>   bindings;
    ///
    type_register(GraphicsData);
};

struct Graphics:mx {
    Graphics(symbol name, type_t vtype, array<mx> bindings, symbol shader = "pbr", GraphicsGen gen = null) : Graphics() {
        data->name      = name;
        data->shader    = shader;
        data->vtype     = vtype;
        data->gen       = gen;
        data->bindings  = bindings;
    }
    mx_object(Graphics, mx, GraphicsData);
};

enums(Sampling, nearest, nearest, linear, ansio);

using MG = map<Graphics>;

struct Pipeline:mx {
    mx_declare(Pipeline, mx, struct IPipeline)
    Pipeline(Device &device, gltf::Model &m, Graphics graphics);
    static void assemble_graphics(IPipeline *pipeline, ion::gltf::Model &m, Graphics &gfx);
};

/// for Daniel to call back.. they got tired of calling
struct Pipes:mx {
    mx_declare(Pipes, mx, struct IPipes);
    Pipes(Device &device, symbol model, array<Graphics> parts);
    Pipeline &operator[](str s);
};

using Scene           = array<Pipes>;
using OnWindowResize  = lambda<void(vec2i)>;
using OnWindowPresent = lambda<Scene()>;

struct ngon {
    size_t size;
    u32   *indices;
};

using mesh  = array<ngon>;
using face  = ngon;
using vpair = std::pair<int, int>;

struct vpair_hash {
    std::size_t operator()(const vpair& p) const {
        return std::hash<int>()(p.first) ^ std::hash<int>()(p.second);
    }
};

struct Canvas;
struct IWindow;
struct Window:mx {
    using OnResize       = ion::lambda<void(int width, int height)>;
    using OnCursorEnter  = ion::lambda<void(int enter)>;
    using OnCursorPos    = ion::lambda<void(double x, double y)>;
    using OnCursorScroll = ion::lambda<void(double x, double y)>;
    using OnCursorButton = ion::lambda<void(int button, int action, int mods)>;
    using OnKeyChar      = ion::lambda<void(u32 code, int mods)>;
    using OnKeyScanCode  = ion::lambda<void(int key, int scancode, int action, int mods)>;
    using OnCanvasRender = ion::lambda<void(Canvas&)>;
    using OnSceneRender  = ion::lambda<Scene()>;
    
    static Window create(str title, vec2i sz);

    void set_visibility(bool v);
    void close();
    str  title();
    void set_title(str title);

    void set_on_cursor_enter    (OnCursorEnter cursor_enter);
    void set_on_cursor_pos      (OnCursorPos cursor_pos);
    void set_on_cursor_scroll   (OnCursorScroll cursor_scroll);
    void set_on_cursor_button   (OnCursorButton cursor_button);
    void set_on_key_char        (OnKeyChar key_char);
    void set_on_key_scancode    (OnKeyScanCode key_scancode) ;

    void register_presentation(OnWindowPresent on_present, OnWindowResize on_resize);

    void  run();
    void *handle();
    bool  process();
    void *user_data();
    void  set_user_data(void *);
    vec2i size();

    operator bool();

    Device device();
    Texture texture();

    mx_declare(Window, mx, IWindow)
};

}

struct SkCanvas;

namespace ion {

struct Canvas;

struct text_metrics {
    real            w = 0,
                    h = 0;
    real       ascent = 0,
              descent = 0;
    real  line_height = 0,
           cap_height = 0;
};

using tm_t = text_metrics;

namespace graphics {
    enums(cap, none,
        none, blunt, round);
    
    enums(join, miter,
        miter, round, bevel);

    struct shape:mx {
        using Rect    = ion::Rect   <r64>;
        using Rounded = ion::Rounded<r64>;

        ///
        struct sdata {
            type_t      type;           /// type of shape (null for operations)
            Rectd       bounds;         /// boundaries, or identity of shape primitive
            doubly<mx>  ops;            /// operation list (or verbs)
            vec2d       mv;             /// movement cursor
            void*       sk_path;        /// saved sk_path; (invalidated when changed)
            void*       sk_offset;      /// applied offset; this only works for positive offset
            real        cache_offset;   /// when sk_path/sk_offset made, this is set
            real        offset;         /// current state for offset; an sk_path is not made because it may not be completed by the user
            type_register(sdata);
        };
        
        ///
        mx_object(shape, mx, sdata);

        operator rectd &() {
            if (!data->bounds)
                bounds();
            return data->bounds;
        }

        bool contains(vec2d p) {
            if (!data->bounds)
                bounds();
            
            return data->bounds->contains(p);
        }
        ///
        rectd &bounds() {
            real min_x = 0, max_x = 0;
            real min_y = 0, max_y = 0;
            int  index = 0;
            ///
            if (!data->bounds && data->ops) {
                /// get aabb from vec2d
                for (mx &v:data->ops) {
                    vec2d *vd = v.get<vec2d>(0); 
                    if (!vd) continue;
                    if (!index || vd->x < min_x) min_x = vd->x;
                    if (!index || vd->y < min_y) min_y = vd->y;
                    if (!index || vd->x > max_x) max_x = vd->x;
                    if (!index || vd->y > max_y) max_y = vd->y;
                    index++;
                }
            }
            return data->bounds;
        }

        shape(rectd r4, real rx = nan<real>(), real ry = nan<real>()) : shape() {
            bool use_rect = std::isnan(rx) || rx == 0 || ry == 0;
            data->type   = use_rect ? typeof(Rectd) : typeof(Rounded);
            data->bounds = use_rect ? Rectd(r4) : 
                                      Rectd(Rounded(r4, rx, std::isnan(ry) ? rx : ry)); /// type forwards
        }

        bool is_rect () { return data->bounds.type() == typeof(rectd)          && !data->ops; }
        bool is_round() { return data->bounds.type() == typeof(Rounded::rdata) && !data->ops; }    

        /// following ops are easier this way than having a last position which has its exceptions for arc/line primitives
        inline void line    (vec2d  l) {
            Line::ldata ld { data->mv, l };
            data->ops += ion::Line(ld);
            data->mv = l;
        }
        inline void bezier  (Bezier b) { data->ops += b; }
        inline void move    (vec2d  v) {
            data->mv = v;
            data->ops += ion::Movement(v);
        }

        //void quad    (graphics::quad   q) { data->ops += q; }
        inline void arc(Arc a) { data->ops += a; }
        operator        bool() { bounds(); return bool(data->bounds); }
        bool       operator!() { return !operator bool(); }
        shape::sdata *handle() { return data; }
    };

    struct border {
        real size, tl, tr, bl, br;
        rgbad color;
        inline bool operator==(const border &b) const { return b.tl == tl && b.tr == tr && b.bl == bl && b.br == br; }
        inline bool operator!=(const border &b) const { return !operator==(b); }

        /// members are always null from the memory::allocation
        border() { }
        ///
        border(str raw) : border() {
            str        trimmed = raw.trim();
            size_t     tlen    = trimmed.len();
            array<str> values  = raw.split();
            size_t     ncomps  = values.len();
            ///
            if (tlen > 0) {
                size = values[0].real_value<real>();
                if (ncomps == 5) {
                    tl = values[1].real_value<real>();
                    tr = values[2].real_value<real>();
                    br = values[3].real_value<real>();
                    bl = values[4].real_value<real>();
                } else if (tlen > 0 && ncomps == 2)
                    tl = tr = bl = br = values[1].real_value<real>();
                else
                    console.fault("border requires 1 (size) or 2 (size, roundness) or 5 (size, tl tr br bl) values");
            }
        }
        type_register(border);
    };
};

enums(nil, none, none);

/// todo: needs ability to set a middle in coord/region
enums(xalign, left,
     left, middle, right, width);
    
enums(yalign, top,
     top, middle, bottom, height);

enums(blending, undefined,
     undefined, clear, src, dst, src_over, dst_over, src_in, dst_in, src_out, dst_out, src_atop, dst_atop, Xor, plus, modulate, screen, overlay, color_dodge, color_burn, hard_light, soft_light, difference, exclusion, multiply, hue, saturation, color);

enums(filter, none,
     none, blur, brightness, contrast, drop_shadow, grayscale, hue_rotate, invert, opacity, saturate, sepia);

/// need interpolation with enumerable type.  the alignment itsself will represent an axis
/// alignment doesnt use the full enumerate set in xalign/yalign because its use-case is inside of the scope of that
struct alignment {
    real x, y;
    bool is_default = false;

    alignment() { is_default = true; }

    alignment(real x, real y) : x(x), y(y), is_default(false) { }

    alignment(str s) : is_default(false) {
        array<str> a = s.split();
        str s0 = a[0];
        str s1 = a.len() > 1 ? a[1] : a[0];

        if (s0.numeric()) {
            x = s0.real_value<real>();
        } else {
            xalign xa { s0 };
            switch (xa.value) {
                case xalign::left:   x = 0.0; break;
                case xalign::middle: x = 0.5; break; /// todo: needs ability to set a middle in coord/region
                case xalign::right:  x = 1.0; break;
                default: assert(false);       break;
            }
        }

        if (s1.numeric()) {
            y = s1.real_value<real>();
        } else {
            yalign ya { s1 };
            switch (ya.value) {
                case yalign::top:    y = 0.0; break;
                case yalign::middle: y = 0.5; break; /// todo: needs ability to set a middle in coord/region
                case yalign::bottom: y = 1.0; break;
                default: assert(false);       break;
            }
        }
    }

    alignment(cstr cs) : alignment(str(cs)) { }

    alignment mix(alignment &b, double f) {
        return { x * (1.0 - f) + b.x * f,
                 y * (1.0 - f) + b.y * f };
    }

    type_register(alignment);
};

struct font:mx {
    struct fdata {
        real sz = 22;
        str  name = "RobotoMono-Regular";
        bool loaded = false;
        void *sk_font = null;
        type_register(fdata);
    };

    mx_object(font, mx, fdata);

    font(real sz, str name) : font() {
        data->sz = sz;
        data->name = name;
    }

    font(str s):font() {
        array<str> sp = s.split();
        data->sz   = sp[0].real_value<real>();
        if (sp.len() > 1)
            data->name = sp[1];
    }

    font(cstr s):font(str(s)) { }

    font(real sz):font() {
        data->sz = sz;
    }

    path get_path() {
        assert(data->name);
        return fmt { "fonts/{0}.ttf", { data->name } };
    }

    font update_sz(real sz) {
        fdata fd { sz, data->name };
        return font(fd);
    }

    array<double> advances(Canvas& canvas);
    array<double> advances(Canvas& canvas, str line);

            operator bool()    { return  data->sz; }
    bool    operator!()        { return !data->sz; }
};


enums(mode, regular,
      regular, headless);

struct Canvas;

enums(operation, fill,
     fill, image, outline, text, child);

template <typename> struct simple_content : true_type { };

struct LineInfo {
    str            data;
    num            len;
    array<double>  adv;
    rectd          bounds;     /// bounds of area of the text line
    rectd          placement;  /// effective bounds of the aligned text, with y and h same as bounds
};

struct TextSel {
    num     column = 0;
    num     row = 0;

    /// common function for TextSel used in 2 parts; updates the sel_start and sel_end
    static void replace(doubly<LineInfo> &lines, TextSel &sel_start, TextSel &sel_end, doubly<LineInfo> &text) {
        assert(sel_start.row <= sel_end.row);
        assert(sel_start.row != sel_end.row || sel_start.column <= sel_end.column);
        LineInfo &ls = lines[sel_start.row];
        LineInfo &le = lines[sel_end  .row]; /// this is fine to do before, because its a doubly
        bool line_insert = text->len() > 1;

        /// a buffer with \n is len == 2.  make sense because you start at 1 line, and a new line gives you 2.
        str left   = str(&ls.data[0], sel_start.column);
        str right  = str(&le.data[sel_end.column]);

        /// delete LineInfo's inbetween start and end
        for (int i = sel_start.row + 1; i <= sel_end.row; i++)
            lines->remove(sel_start.row + 1);

        /// manage lines when inserting new ones
        if (line_insert) {
            /// first line is set to text to left of sel + first line of sel
            ls.data = left + text[0].data;
            ls.len  = ls.data.len();

            /// recompute advances by clearing
            ls.adv.clear();

            /// insert lines[1] and above
            int index = 0;
            int last  = text->len();

            for (LineInfo &l: text) {
                item<LineInfo> *item;
                if (index) {
                    item = lines->insert(sel_start.row + index, l); // this should insert on end, it doesnt with a 1 item list with index = 1
                    /// this function insert before, unless index == count then its a new tail
                    /// always runs as final op, this merges last line with the data we read on right side prior
                    if (index + 1 == last) {
                        LineInfo &n = item->data;
                        n.data = l.data + right;
                        n.len  = n.data.len();
                        sel_start.column = l.data.len();
                        break;
                    }
                    sel_start.row++; /// we set both of these
                }
                index++;
            }
        } else {
            /// result is lines removed from ls+1 to le, ls.data = ls[left] + data + le[right]
            ls.data    = left + text[0].data + right;
            ls.len     = ls.data.len();
            ls.adv.clear();
        }
        sel_end.row    = sel_start.row;
        sel_end.column = sel_start.column;
    }
    ///
    bool operator<= (const TextSel &b) { return !operator>(b); }
    bool operator>  (const TextSel &b) { return row > b.row || ((row == b.row) && column > b.column); }
};

#ifdef CANVAS_IMPL
    using svgdom_t = sk_sp<SkSVGDOM>*;
#else
    using svgdom_t = void*;
#endif

struct Canvas;

struct EStr {
    double value;
    str    suffix;

    EStr() { }

    EStr(str combined) {
        for (int i = 0; i < combined.len(); i++) {
            char ch = combined[i];
            if ((ch >= 'a' && ch <= 'z') || ch == '%') {
                suffix = combined.mid(i);
                value  = combined.mid(0, i).real_value<real>();
                break;
            }
        }
        if (!suffix)
            value = combined.real_value<real>();
    }
    
    EStr(double value, str suffix) : value(value), suffix(suffix) { }

    EStr mix(EStr &b, double f) {
        assert(suffix == b.suffix);
        return EStr {
            value * (1.0 - f) + b.value * f, suffix
        };
    }

    operator str() {
        return fmt { "{0}{1}", { value, suffix } };
    }

    register(EStr);
};

/// external props, used to manage SVG
/// may have variable suffix but cannot interpolate between units as they are not known here, so dont do 1px to 3cm or 1% to 100px
struct EProps:mx {
    struct M {
        map<EStr> eprops;
        operator bool() {
            return eprops.len() > 0;
        }
        register(M)
    };
    mx_basic(EProps);

    EProps(str s) : EProps() {
        array<str> a = s.split("|");
        for (str &v: a) {
            array<str> kv = v.split("=");
            assert(kv.len() == 2);
            data->eprops[kv[0]] = EStr(kv[1]);
        }
    }
    
    EProps(cstr cs) : EProps(str(cs)) { }

    EProps mix(EProps &b, double f) {
        EProps &a = *this;
        EProps r;
        assert(a->eprops.len() == b->eprops.len());
        for(field<EStr> &af: a->eprops) {
            assert(b->eprops->count(af.key));
            r->eprops[af.key] = af.value.mix(b->eprops[af.key], f);
        }
        return r;
    }
};

struct SVG:mx {
    struct M {
        svgdom_t   svg_dom;
        int        w, h;
        int       rw, rh;
        operator bool();
        register(M);
    };

    SVG(path p);
    SVG(cstr p);
    void render(SkCanvas *sk_canvas, int w = -1, int h = -1);
    void set_props(EProps &eprops);
    vec2i sz();

    mx_basic(SVG);
};


struct ICanvas;
struct iSVG;
struct Window;

struct Canvas:mx {
    mx_declare(Canvas, mx, ICanvas);

    Canvas(Device device, Texture texture, bool use_hidpi);

    u32 get_virtual_width();
    u32 get_virtual_height();

    void font(ion::font f);
    void save();
    void clear();
    void clear(rgbad c);
    void flush();
    void restore();
    void color(rgbad c);
    void opacity(double o);
    vec2i size();
    text_metrics measure(str text);
    double measure_advance(char *text, size_t len);
    str ellipsis(str text, rectd rect, text_metrics &tm);
    void image(SVG   img, rectd rect, alignment align, vec2d offset);
    void image(ion::image img, rectd rect, alignment align, vec2d offset, bool attach_tx = false);
    void text(str text, rectd rect, alignment align, vec2d offset, bool ellip, rectd *placement = null);
    void clip(rectd path);
    void projection(glm::mat4 &m, glm::mat4 &v, glm::mat4 &p);
    void outline(array<glm::vec3> v3);
    void outline(array<glm::vec2> v2);
    void line(glm::vec3 &a, glm::vec3 &b);
    void outline(rectd rect);
    void outline_sz(double sz);
    void cap(graphics::cap c);
    void join(graphics::join j);
    void translate(vec2d tr);
    void scale(vec2d sc);
    void rotate(double degs);
    void fill(rectd rect);
    void fill(graphics::shape path);
    void clip(graphics::shape path);
    void gaussian(vec2d sz, rectd crop);
    void arc(glm::vec3 pos, real radius, real startAngle, real endAngle, bool is_fill = false);
};

}
