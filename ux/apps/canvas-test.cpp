
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
struct RubiksState {
    glm::mat4  model;
    glm::mat4  view;
    glm::mat4  proj;
    glm::vec4  eye;
    Light      lights[3];
};

struct UState {
    float x_scale;
    float y_scale;
};

int main(int argc, const char* argv[]) {
    static constexpr uint32_t kWidth = 1024;
    static constexpr uint32_t kHeight = 1024;

    Window window   = Window::create("dawn", {kWidth, kHeight});
    Device  device  = window.device();
    Texture canvas_texture = device.create_texture(window.size(), Asset::attachment);
    window.set_title("Canvas Test");

    UState  canvas_state;
    Uniform canvas_uniform = Uniform::of_state(canvas_state);
    
    /*
    Pipes human_pipeline = Pipes(device, null, array<Graphics> {
        Graphics {
            "human", typeof(Vertex), {
                Sampling::linear, (Asset::color),
                Sampling::linear, (Asset::normal),
                Sampling::linear, (Asset::material),
                Sampling::linear, (Asset::reflect),
                Sampling::linear, (Asset::env)
            }, "human", [](array<image> &images) {
                /// if we provide this lambda, we are letting gltf load the vbo/ibo
            }
        }
    });*/

    Pipes canvas_pipeline = Pipes(device, null, array<Graphics> {
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
