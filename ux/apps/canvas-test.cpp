
#include <ux/ux.hpp>

using namespace ion;

int main(int argc, const char* argv[]) {
    static constexpr uint32_t kWidth = 1024;
    static constexpr uint32_t kHeight = 1024;

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
    struct UState {
        float x_scale;
        float y_scale;
    };

    Window window   = Window::create("dawn", {kWidth, kHeight});
    Device  device  = window.device();
    vec2i   size    = window.size();
    Texture texture = device.create_texture(size, Asset::attachment);
    window.set_title("Canvas Test");

    UState  ustate;
    Uniform uniform = Uniform::of_state(ustate);
    
    Pipes canvas_pipeline = Pipes(device, null, array<Graphics> {
        Graphics {
            "canvas", typeof(CanvasAttribs), { texture, Sampling::linear, uniform }, "canvas",
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
                // set vbo and ibo
                ibo = mx::wrap<u32>((void*)indexData, 6);
                vbo = mx::wrap<CanvasAttribs>((void*)vertexData, 4);
            }
        }
    });
    
    Canvas canvas;

    num s = millis();

    /// calls OnWindowResize when registered
    window.register_presentation(
        /// presentation: returns the pipelines that renders canvas, also updates the canvas (test code)
        [&]() -> Scene {

            num diff = millis() - s;
            float  f = (float)diff / 1000.0 * 2.0f * M_PI;

            // change Uniform states in Presentation associated to pipelines that use them
            ustate.x_scale = 0.5 + sin(f) * 0.25;
            ustate.y_scale = 0.5 + cos(f) * 0.25;

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
        /// size updated (resizes texture, and recreates Canvas)
        [&](vec2i sz) {
            texture.resize(sz);
            canvas = Canvas(texture);
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
