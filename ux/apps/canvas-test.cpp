
#include <ux/ux.hpp>

using namespace ion;

int main(int argc, const char* argv[]) {
    static constexpr uint32_t kWidth = 1024;
    static constexpr uint32_t kHeight = 1024;

    Window window = Window::create("dawn", {kWidth, kHeight});

    struct Attribs {
        glm::vec4 pos;
        doubly<prop> meta() {
            return {
                { "pos", pos }
            };
        }
    };

    Device device = window.device();
    Pipes test_scene = Pipes(device, null, array<Graphics> {
        Graphics {
            "test", typeof(Attribs), { }, "test", 
            [](mx &vbo, mx &ibo, array<image> &images) {
                static const uint32_t indexData[6] = {
                    0, 1, 2,
                    2, 1, 3
                };

                static const Attribs vertexData[4] = {
                    {{ -1.0f, -1.0f, 0.0f, 1.0f }},
                    {{  1.0f, -1.0f, 0.0f, 1.0f }},
                    {{ -1.0f,  1.0f, 0.0f, 1.0f }},
                    {{  1.0f,  1.0f, 0.0f, 1.0f }}
                };

                ibo = mx::wrap<u32>((void*)indexData, 6);
                vbo = mx::wrap<Attribs>((void*)vertexData, 4);
            }
        }
    });
    
    window.set_on_scene_render([&]() -> Scene {
        return Scene { test_scene };
    });

    /*
    window.set_on_canvas_render([](Canvas &canvas) {
        vec2i sz = canvas.size();
        rectd rect { 0, 0, sz.x, sz.y };
        canvas.color("#00f");
        canvas.fill(rect);

        rectd top { 0, 0, sz.x, sz.y / 2 };
        canvas.color("#ff0");
        canvas.fill(top);
    });
    */

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
