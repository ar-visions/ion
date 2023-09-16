    #include <GL/freeglut.h>
    #include <GL/glew.h>
    #include <GL/gl.h>
    #include <GL/glx.h>
    #include <GL/glxext.h>
    #include <X11/Xlib.h>
    #include <X11/extensions/Xcomposite.h>

    #define GLFW_EXPOSE_NATIVE_X11
    #define GLFW_EXPOSE_NATIVE_GLX
    
    static Display* dpy;
    static int composite_event_base, composite_error_base;

    using  PFNGlXBindTexImageEXTPROC     = void (*)(Display *, GLXDrawable, int, const int *);
    using  PFNGlXReleaseTexImageEXTPROC  = void (*)(Display *, GLXDrawable, int);
    static PFNGlXBindTexImageEXTPROC       glXBindTexImageEXT    = ion::null;
    static PFNGlXReleaseTexImageEXTPROC    glXReleaseTexImageEXT = ion::null;

    static int
    Xwindows_error (Display *xdisplay, XErrorEvent *error) {
        return 0;
    }
