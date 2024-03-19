
#include <ux/ux.hpp>

using namespace ion;

/// dont need to 'register' these structs, because we dont create at runtime
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

/// define structs for rubiks
struct Light {
    glm::vec4 pos;
    glm::vec4 color;
};

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv0;
    glm::vec2 uv1;
    glm::vec4 tangent;
    u16       joints0[4];
    u16       joints1[4];
    float     weights0[4];
    float     weights1[4];

    doubly<prop> meta() const {
        return {
            prop { "POSITION",      pos      },
            prop { "NORMAL",        normal   },
            prop { "TEXCOORD_0",    uv0      },
            prop { "TEXCOORD_1",    uv1      },
            prop { "TANGENT",       tangent  },
            prop { "JOINTS_0",      joints0  },
            prop { "JOINTS_1",      joints1  },
            prop { "WEIGHTS_0",     weights0 },
            prop { "WEIGHTS_1",     weights1 }
        };
    }

    register(Vertex);
};

/// get gltf model output running nominal; there seems to be a skew in the coordinates so it may be being misread
/// uniform has an update method with a pipeline arg
struct HumanState:Uniform {
    glm::mat4  model;
    glm::mat4  view;
    glm::mat4  proj;
    glm::vec4  eye;
    Light      lights[4];
    register(HumanState);
};

struct UState:Uniform {
    float x_scale;
    float y_scale;
    register(UState);
};

int main(int argc, const char* argv[]) {
    static constexpr uint32_t kWidth = 1024;
    static constexpr uint32_t kHeight = 1024;

    Window window   = Window::create("dawn", {kWidth, kHeight});
    Device  device  = window.device();
    Texture canvas_texture = device.create_texture(window.size(), Asset::attachment);
    window.set_title("Canvas Test");

    type_t type = typeof(UState);

    UState     canvas_state;
    mx         canvas_uniform = mx::window(&canvas_state);

    HumanState human_state;
    mx         human_uniform  = mx::window(&human_state); // window memory is not freed at origin (wrap is)

    Model      human_pipeline = Model(device, "human", array<Graphics> {
        Graphics {
            "Body", typeof(Vertex), { human_uniform }, "human" /// contains Body contains head; does not contain teeth, tongue or L/R eyes
        } /// implicit is Skin(0) (node has reference to skin of 0, if not specified, no implicit bindings to Skin buffer) resolved Joint Buffer (bind to var<storage> type) at index 0 (assert that its in range)
    }); /// todo: process Skin to resolve buffers
    Object     human_object = human_pipeline.instance();

    /// we need a render state on Pipes, and i suppose that contains the same set of names


    Model canvas_pipeline = Model(device, null, array<Graphics> {
        Graphics {
            "canvas", typeof(CanvasAttribs),
            { canvas_texture, Sampling::linear, canvas_uniform }, "canvas",
            /// with this lambda, we are providing our own vbo/ibo
            [](mx &vbo, mx &ibo, array<image> &images) {
                static const uint32_t indexData[6] = {
                    0, 1, 2,
                    2, 1, 3
                };
                static CanvasAttribs vertexData[4] = {
                    {{ -1.0f, -1.0f, 0.0f, 1.0f }, {  0.0f, 0.0f }},
                    {{  1.0f, -1.0f, 0.0f, 1.0f }, {  1.0f, 0.0f }},
                    {{ -1.0f,  1.0f, 0.0f, 1.0f }, { -1.0f, 1.0f }},
                    {{  1.0f,  1.0f, 0.0f, 1.0f }, {  1.0f, 1.0f }}
                };
                ibo = mx::wrap<u32>((void*)indexData, 6);
                vbo = mx::wrap<CanvasAttribs>((void*)vertexData, 4);
            }
        }
    });
    Object     canvas_object = canvas_pipeline.instance();
    
    Canvas canvas;
    num s = millis();
 
    window.register_presentation(
        [&]() -> Scene {
            num diff = millis() - s;
            float  f = (float)diff / 1000.0 * 2.0f * M_PI;

            // change Uniform states in Presentation associated to pipelines that use them
            canvas_state.x_scale = 0.5 + sin(f) * 0.25;
            canvas_state.y_scale = 0.5 + cos(f) * 0.25;

            vec2i sz = canvas.size();
            rectd rect { 0, 0, sz.x, sz.y };
            canvas.color("#00f");
            canvas.fill(rect);
            rectd top { 0, 0, sz.x, sz.y / 2 };
            canvas.color("#ff0");
            canvas.fill(top);
            canvas.flush();
            return Scene { canvas_pipeline };
        },
        [&](vec2i sz) {
            canvas_texture.resize(sz);
            canvas = Canvas(canvas_texture);
        });
    
    i64 s_last = millis() / 1000;
    i64 frames_drawn = 0;

    while (window.process()) {
        frames_drawn++;
        usleep(1);
        i64 s_cur = millis() / 1000;
        if (s_cur != s_last) {
            console.log("frames_drawn: {0}", { frames_drawn });
            s_last = s_cur;
            frames_drawn = 0;
        }
    }
}
