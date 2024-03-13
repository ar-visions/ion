
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

int main(int argc, const char* argv[]) {
    static constexpr uint32_t kWidth = 1024;
    static constexpr uint32_t kHeight = 1024;

    Window window   = Window::create("dawn", {kWidth, kHeight});
    Device  device  = window.device();
    vec2i   size    = window.size();
    Texture texture = device.create_texture(size, Asset::attachment);
    window.set_title("Canvas Test");
    
    Pipes canvas_pipeline = Pipes(device, null, array<Graphics> {
        Graphics {
            "canvas", typeof(CanvasAttribs), { texture, Sampling::linear }, "canvas",
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
                for (int i = 0; i < 4; i++) {
                    vertexData[i].pos.x *= 0.1;
                    vertexData[i].pos.y *= 0.1;
                }
                // set vbo and ibo
                ibo = mx::wrap<u32>((void*)indexData, 6);
                vbo = mx::wrap<CanvasAttribs>((void*)vertexData, 4);
            }
        }
    });
    
    Canvas canvas;

    /// calls OnWindowResize when registered
    window.register_presentation(
        /// presentation: returns the pipelines that renders canvas, also updates the canvas (test code)
        [&]() -> Scene {
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
        /// size updated (recreates a texture from device, and canvas)
        [&](vec2i sz) {
            texture.resize(sz);
            canvas = Canvas(device, texture, false);
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
