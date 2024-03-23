
#include <ux/ux.hpp>
#include <trinity/subdiv.hpp>

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

struct HumanState {
    glm::mat4  model;
    glm::mat4  view;
    glm::mat4  proj;
    glm::vec4  eye;
    Light      lights[4];
    u32        light_count;
    u32        padding[3];
    register(HumanState);
};

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv0;
    glm::vec2 uv1;
    glm::vec4 tangent;
    u32       joints0[4];
    u32       joints1[4];
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

    Model m_canvas = Model(device, null, {
        Graphics {
            "canvas",
            typeof(CanvasAttribs), {
                Sampling::linear,
                canvas_texture,
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
    Object o_canvas = m_canvas.instance();

    Model m_human = Model(device, "human", {
        Graphics { "Body", typeof(Vertex), { ObjectUniform(HumanState) }, null, "human" }
    });

    Object o_human = m_human.instance();
    
    Canvas canvas;
    num s = millis();


    states<Clear> clear_states { Clear::Color, Clear::Depth };
    clear_states[Clear::Color] = false;

    window.register_presentation(
        [&]() -> Scene {
            UState &u_canvas   = o_canvas.uniform<UState>("canvas"); /// todo: defaults not set
            float  *test_store = o_canvas.vector <float> ("canvas");
            test_store[0] = 1.0f;
            test_store[1] = 1.0f;
            num diff = millis() - s;
            float  f = (float)diff / 1000.0 * 2.0f * M_PI;
            u_canvas.x_scale = 0.5 + sin(f) * 0.25;
            u_canvas.y_scale = 0.5 + cos(f) * 0.25;
            vec2i sz = canvas.size();
            rectd rect { 0, 0, sz.x, sz.y };
            canvas.color("#00f");
            canvas.fill(rect);
            rectd top { 0, 0, sz.x, sz.y / 2 };
            canvas.color("#ff0");
            canvas.fill(top);
            canvas.flush();

            HumanState &u_human = o_human.uniform<HumanState>("Body");
            glm::vec3 eye    = glm::vec3(0.0f, 1.5f, -0.8f);
            glm::vec3 target = glm::vec3(0.0f, 1.5f, 0.0f);
            glm::vec3 up     = glm::vec3(0.0f, 1.0f, 0.0f);

            
            static float inc = 0;
            inc += 0.0004f;
            glm::quat r = glm::angleAxis(inc, glm::vec3(0.0f, 1.0f, 0.0f));
            u_human.model = glm::mat4_cast(r);
            u_human.view  = glm::lookAt(eye, target, up);
            u_human.proj  = glm::perspective(64.0f, window.aspect(), 0.1f, 100.0f);

            u_human.lights[0].pos = glm::vec4(0.0f, 1.5f, -0.8f, 0.0f);
            u_human.light_count = 1;

            array<Object> scene(1);
            scene.push(o_human);

            return scene;
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
