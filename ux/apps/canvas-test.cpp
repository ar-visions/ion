
#include <ux/ux.hpp>
#include <trinity/trinity.hpp>

//#include <glm/glm.hpp>
//#include <glm/gtc/matrix_transform.hpp> // Includes perspective and lookAt
//#include <glm/gtc/quaternion.hpp> // For glm::quat
//#include <glm/gtx/quaternion.hpp> // For more quaternion operations

using namespace ion;

struct CanvasAttribs {
    vec4f pos;
    vec2f uv;
    properties meta() {
        return {
            prop { "pos", pos },
            prop { "uv",  uv  }
        };
    }
};

struct Light {
    vec4f pos;
    vec4f color;
};

struct HumanState {
    m44f       model;
    m44f       view;
    m44f       proj;
    vec4f  eye;
    Light      lights[4];
    u32        light_count;
    u32        padding[3];
};


struct UState {
    float x_scale = 1.0f;
    float y_scale = 1.0f;
};

int main(int argc, const char* argv[]) {
    Mesh mesh;

    type_t hv_type = typeof(HumanVertex);
    
    static constexpr uint32_t kWidth  = 1024;
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
            /// with this lambda, we are providing our own vertices and triangles (we want to support quads as soon as extension to glTF plugin is working)
            [](Mesh &mesh, Array<image> &images) {
                /// set vertices
                mesh->verts = Array<CanvasAttribs> {
                    {{ -1.0f, -1.0f, 0.0f, 1.0f }, {  0.0f, 0.0f }},
                    {{  1.0f, -1.0f, 0.0f, 1.0f }, {  1.0f, 0.0f }},
                    {{ -1.0f,  1.0f, 0.0f, 1.0f }, { -1.0f, 1.0f }},
                    {{  1.0f,  1.0f, 0.0f, 1.0f }, {  1.0f, 1.0f }}
                };
                /// set triangles
                mesh->tris = Array<u32> {
                    0, 1, 2, // ABC
                    0, 2, 3  // ACD (same as blender)
                };
            }
        }
    });
    Object o_canvas = m_canvas.instance();

    Model m_human = Model(device, "human", {
        Graphics { "Body", typeof(HumanVertex), { ObjectUniform(HumanState) }, null, "human" }
    });

    Object o_human = m_human.instance();
    Canvas canvas;
    num s = millis();

    states<Clear> clear_states { Clear::Color, Clear::Depth };
    clear_states.clear(Clear::Color);

    window.register_presentation(
        [&]() -> Scene {
            /// todo: defaults not set
            UState &u_canvas   = o_canvas.uniform<UState>("canvas"); 
            Array<float> test_store = o_canvas.vector <float> ("canvas");
            test_store[0] = 1.0f;
            test_store[1] = 1.0f;
            num diff = millis() - s;
            float  f = (float)diff / 1000.0 * 2.0f * M_PI;
            u_canvas.x_scale = 0.5 + sin(f) * 0.25;
            u_canvas.y_scale = 0.5 + cos(f) * 0.25;
            vec2i sz = canvas.size();
            ion::rect rect { 0, 0, sz.x, sz.y };
            canvas.color("#00f");
            canvas.fill(rect);
            ion::rect top { 0, 0, sz.x, sz.y / 2 };
            canvas.color("#ff0");
            canvas.fill(top);
            canvas.flush();

            HumanState &u_human = o_human.uniform<HumanState>("Body");
            vec3f eye    = vec3f(0.0f, 1.5f, -0.8f);
            vec3f target = vec3f(0.0f, 1.5f, 0.0f);
            vec3f up     = vec3f(0.0f, 1.0f, 0.0f);

            /*
            vec3f eye     = vec3f(0.0f, 1.0f, -0.8f) * 4.0f;
            vec3f target  = vec3f(0.0f, 0.0f, 0.0f);
            vec3f forward = normalize(target - eye);
            vec3f w_up    = vec3f(0.0f, 1.0f, 0.0f);
            vec3f right   = normalize(cross(forward, w_up));
            vec3f up      = cross(right, forward);
            */
            
            static float inc = 0;
            inc += 0.0004f;
            quatf       r = quatf(vec3f(0.0f, 1.0f, 0.0f), inc);
  
            u_human.model = r;
            u_human.view  = m44f::look_at(eye, target, up);
            u_human.proj  = m44f::perspective(64.0f, window.aspect(), 0.1f, 100.0f);

            u_human.lights[0].pos = vec4f(0.0f, 1.5f, -0.8f, 0.0f);
            u_human.light_count = 1;

            Array<Object> scene(1);
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
        usleep(2);
        i64 s_cur = millis() / 1000;
        if (s_cur != s_last) {
            console.log("frames_drawn: {0} ", { frames_drawn });
            s_last = s_cur;
            frames_drawn = 0;
        }
    }
}
