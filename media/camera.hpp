#if   defined(__APPLE__)
#include <media/camera-apple.hpp>
#elif defined(_WIN32)
#include <media/camera-win.hpp>
#else
#include <media/camera-linux.hpp>
#endif