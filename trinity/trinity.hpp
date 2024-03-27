#pragma once

#include <async/async.hpp>
#include <watch/watch.hpp>
#include <media/media.hpp>
#include <media/image.hpp>

struct GLFWwindow;

namespace ion {

enums(Clear, Undefined,
    Undefined, Color, Depth, Stencil);

#undef DELETE

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

/// having Asset lets us abstract away the usage, format and even dimension
/// the asset we wouldnt have in resource is depth_stencil
enums(Asset, undefined, 
     color, normal, material, reflect, env, attachment, depth_stencil, multisample, undefined); /// populate from objects normal map first, and then adjust by equirect if its provided

struct Device;

enums(Sampling, undefined, undefined, nearest, linear, ansio);

/// Storage is more complex, because we need cases covered for Bone data
/// It may not be generalizable due to the dynamically indexed nature of skin model

/// we dont expose Usage/Format because we have Asset, path (from name); thats enough context to make things
struct Texture:mx {
    mx_declare(Texture, mx, struct ITexture);

    using OnTextureResize = lambda<void(vec2i)>;

    static Texture    load(Device &dev, symbol name, Asset type);
    static ion::image asset_image(symbol name, Asset type);
    static Texture    from_image(Device &dev, image img, Asset type);
    
    void    set_content(mx content);
    void    resize(vec2i sz);
    vec2i   size();
    void    on_resize(str user, OnTextureResize fn);
    void    cleanup_resize(str user);
    void*   handle();
    Device &device();
};


/// used to declare all shader variables, provided to Model
/// can be useful for Compute shader if thats something we want to implement
struct ShaderVar:mx {
    enums(Flag, undefined, 
        undefined = 0,
        object = 1,
        uniform = 2,
        read_only = 4,
        vertex = 8,
        fragment = 16);
    
    struct M {
        Sampling sampling;
        Texture  tx;
        type_t   type;
        size_t   count;
        u32      flags;
        mx       cache; /// if set, this is provided for data (otherwise a unique allocation is given)
    };
    /// handle cases of non-object, or model-based allocation
    mx alloc() {
        if (data->cache)
            return data->cache;
        if (data->sampling)
            return data->sampling;
        if (data->tx)
            return data->tx;
        return memory::alloc(data->type, data->count, 0, null);
    }
    void prepare_data() {
        if (!(data->flags & Flag::object))
            data->cache = memory::alloc(data->type, data->count, 0, null);
    }
    size_t size() {
        return data->type->base_sz * data->count;
    }
    void prepare_flags() {
        if (!(data->flags & (Flag::vertex | Flag::fragment)))
            data->flags  |= (Flag::vertex | Flag::fragment); // both are implied if none are given
    }
    ShaderVar(Sampling sampling) : ShaderVar() {
        data->type = typeof(Sampling);
        data->sampling = sampling;
        prepare_flags();
    }
    ShaderVar(Sampling::etype sampling) : ShaderVar() {
        data->type = typeof(Sampling);
        data->sampling = Sampling(sampling);
        prepare_flags();
    }
    ShaderVar(Texture tx, u32 flags) : ShaderVar() {
        data->type = typeof(Texture);
        data->tx = tx;
        data->flags = flags;
        prepare_flags();
    }
    ShaderVar(Texture tx) : ShaderVar() {
        data->type = typeof(Texture);
        data->tx = tx;
        prepare_flags();
    }
    ShaderVar(type_t type, size_t count, u32 flags) : ShaderVar() {
        data->type  = type;
        data->count = count;
        data->flags = flags;
        prepare_flags();
    }
    mx_basic(ShaderVar);
};

#define   ModelUniform(utype)     ShaderVar(typeof(utype), 1,  ShaderVar::Flag::uniform)
#define  ObjectUniform(utype)     ShaderVar(typeof(utype), 1,  ShaderVar::Flag::uniform | ShaderVar::Flag::object)
#define    ModelVector(utype, sz) ShaderVar(typeof(utype), sz, ShaderVar::Flag::read_only)
#define   ObjectVector(utype, sz) ShaderVar(typeof(utype), sz, ShaderVar::Flag::read_only | ShaderVar::Flag::object)

struct IDevice;
struct Device:mx {
    void*           handle();
    void            get_dpi(float *, float *);
    Texture         create_texture(vec2i sz, Asset asset_type);
    mx_declare(Device, mx, IDevice)
};

struct Pipeline;
struct Model;

enums(ShaderModule, undefined,
     undefined, vertex, fragment, compute);

using GraphicsGen = lambda<void(mx&,mx&,array<image>&)>;

struct GraphicsData {
    str         name;
    str         shader;
    type_t      vtype;
    GraphicsGen gen;
    array<ShaderVar> bindings; /// ordered the same as shader
};

struct Graphics:mx {
    Graphics(str name, type_t vtype, array<ShaderVar> bindings, GraphicsGen gen = null, str shader = null) : Graphics() {
        data->name      = name;
        data->shader    = shader ? shader : name;
        data->vtype     = vtype;
        data->gen       = gen;
        data->bindings  = bindings;
    }
    mx_object(Graphics, mx, GraphicsData);
};

struct Pipeline:mx {
    mx_declare(Pipeline, mx, struct IPipeline)
};

struct Model;
struct IObject;

/// our Object impl needs to access IObject internal, so we forward declare
mx &object_data(IObject *o, str name, type_t type);

using Bones = glm::mat4x4*;

struct Object:mx {
    Model &model(); /// if operator on Model is called, you may obtain Model by calling this
    
    /// lookup instanced data on object; can be model-based if its not flagged as object
    template <typename T>
    T &uniform(str name) {
        mx &o_data = object_data(data, name, typeof(T));
        return *o_data.origin<T>();
    }
    template <typename T>
    T *vector(str name) {
        mx &o_data = object_data(data, name, typeof(T));
        return o_data.origin<T>();
    }
    Bones bones(str name) {
        return vector<glm::mat4x4>(name);
    }
    mx_declare(Object, mx, struct IObject);
};

struct Model:mx {
    mx_declare(Model, mx, struct IModel);
    Model(Device &device, symbol model, array<Graphics> select); /// select Graphics in the Model with user defined functionality
    Pipeline &operator[](str s);
    Object instance(); /// instance Object for rendering
    operator Object(); /// Model ref is stored on Object, so its useful
};

using Scene           = array<Object>; /// this must change to be array<Object> so we render instances (change in progress)
using OnWindowResize  = lambda<void(vec2i)>;
using OnWindowPresent = lambda<Scene()>;

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
    float aspect();

    void  run();
    void *handle();
    bool  process();
    void *user_data();
    void  set_user_data(void *);
    vec2i size();

    operator bool();

    Device device();

    mx_declare(Window, mx, IWindow)
};

}
