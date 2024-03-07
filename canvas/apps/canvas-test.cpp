
#include <canvas/canvas.hpp>

using namespace ion;

int main(int argc, const char* argv[]) {
    static constexpr uint32_t kWidth = 1024;
    static constexpr uint32_t kHeight = 1024;

    Window window = Window::create(Window::Render::Texture, "dawn", {kWidth, kHeight});
    window.set_on_canvas_render([](Canvas &canvas) {
        vec2i sz = canvas.size();
        rectd rect { 0, 0, sz.x, sz.y };
        canvas.color("#00f");
        canvas.fill(rect);

        rectd top { 0, 0, sz.x, sz.y / 2 };
        canvas.color("#ff0");
        canvas.fill(top);
    });

    i64 s_last = millis() / 1000;
    i64 frames_drawn = 0;

    while (true) {
        window.process();

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
