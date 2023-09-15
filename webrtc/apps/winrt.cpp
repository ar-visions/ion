#include <webrtc/win/capture.h>

using namespace ion;

int main(int argc, char *argv[]) {
    HWND             hwnd    = FindWindowA(NULL, "abc");
    HWND             hwnd2   = FindWindowA(NULL, "abc2");

    lambda<bool(mx)> on_data = [](mx data) -> bool {
        printf("data encoded: %d bytes\n", (int)data.count());
        return true;
    };

    lambda<bool(mx)> on_data2 = [](mx data) -> bool {
        printf("data2 encoded: %d bytes\n", (int)data.count());
        return true;
    };

    lambda<void(iCapture*)> on_close = [](iCapture *data) -> void {
        printf("capture closed: %p\n", data);
    };

    Capture capture  { hwnd,  on_data,  on_close };
    Capture capture2 { hwnd2, on_data2, on_close };

    for (;;) {
        usleep(1000000);
    }
}