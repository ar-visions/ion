#include <mx/mx.hpp>
#include <webrtc/win/capture.h>
#include <windows.h>

using namespace ion;

int main(int argc, char *argv[]) {
    HWND             hwnd    = FindWindowA(NULL, "brianwd");
    lambda<bool(mx)> on_data = [](mx data) -> bool {
        printf("data encoded: %d bytes\n", data.count());
        return true;
    };

    Capture capture { hwnd, on_data };

    for (;;) {
        usleep(1000000);
    }
}