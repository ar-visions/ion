
#include <ux/ux.hpp>

using namespace ion;

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

struct HumanState {
    glm::mat4  model;
    glm::mat4  view;
    glm::mat4  proj;
    glm::vec4  eye;
    Light      lights[4];
    register(HumanState);
};

struct UState {
    float x_scale = 1.0f;
    float y_scale = 1.0f;
    register(UState);
};

int main(int argc, const char* argv[]) {
    static constexpr uint32_t kWidth = 1024;
    static constexpr uint32_t kHeight = 1024;

    Window window   = Window::create("dawn", {kWidth, kHeight});
    Device  device  = window.device();
    Texture canvas_texture = device.create_texture(window.size(), Asset::attachment);
    window.set_title("Canvas Test");

    /*
    Model human_model = Model(device, "human", {
        Graphics { "Body", typeof(Vertex), { ObjectUniform(HumanState) }, "human" }
    });

    Object human_object = human_model.instance();
    Bones  body_bones   = human_object.bones("Body");

    we need Bones to be a proper class now, because it needs to apply a m44 (it should reference the the same gltf Model function)
    */

    Model canvas_model = Model(device, null, {
        Graphics {
            "canvas",
            typeof(CanvasAttribs), {
                canvas_texture,
                Sampling::linear,
                ObjectUniform(UState),
                ObjectVector(float, 2)
            },
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
    Object canvas_object = canvas_model.instance();
    
    Canvas canvas;
    num s = millis();
 
    window.register_presentation(
        [&]() -> Scene {
            UState &canvas_state = canvas_object.uniform<UState>("canvas"); /// todo: defaults not set
            float *test_store    = canvas_object.vector <float> ("canvas");

            test_store[0] = 1.0f;
            test_store[1] = 0.1f;

            num diff = millis() - s;
            float  f = (float)diff / 1000.0 * 2.0f * M_PI;

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
            return Scene { canvas_object };
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
