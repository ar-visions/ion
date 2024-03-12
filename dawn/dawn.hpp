#pragma once

#include <async/async.hpp>
#include <watch/watch.hpp>
#include <media/media.hpp>
#include <media/image.hpp>
#include <dawn/gltf.hpp>

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
