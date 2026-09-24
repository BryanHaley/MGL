/*
 * Michael Larson on 1/6/2022
 *
 * mgl_context.m
 * GLFW
 *
 */

#import <QuartzCore/QuartzCore.h>

#include "MGLContext.h"
#include "internal.h"
#include "MGLRenderer.h"

#include <unistd.h>
#include <math.h>

#define GL_BGRA                           0x80E1
#define GL_UNSIGNED_INT_8_8_8_8_REV       0x8367
#define GL_DEPTH_COMPONENT                0x1902
#define GL_FLOAT                          0x1406


GLMContext createGLMContext(GLenum format, GLenum type,
                        GLenum depth_format, GLenum depth_type,
                        GLenum stencil_format, GLenum stencil_type);

void MGLsetCurrentContext(GLMContext ctx);
void MGLswapBuffers(GLMContext ctx);
void MGLsetSwapInterval(GLMContext ctx, int interval);

static void makeContextCurrentMGL(_GLFWwindow* window)
{
    @autoreleasepool {

    if (window)
    {
        MGLsetCurrentContext(window->context.mgl.ctx);

        _glfwPlatformSetTls(&_glfw.contextSlot, window);
    }
    else
    {
        // Just so we have jump tables. One is made and kept: a new one each
        // time was never freed, and GLFW does this on every window it destroys.
        static GLMContext placeholder = NULL;

        if (placeholder == NULL)
            placeholder = createGLMContext(0, 0, 0, 0, 0, 0);

        MGLsetCurrentContext(placeholder);

        _glfwPlatformSetTls(&_glfw.contextSlot, NULL);
    }

    } // autoreleasepool
}

static void swapBuffersMGL(_GLFWwindow* window)
{
    @autoreleasepool {

    // Metal only paces a layer the system is showing, and a window that is
    // hidden or on another space is not, so the frame is paced here instead.
    // Measuring from the last present means this adds nothing when Metal
    // already did the waiting.
    if (window->context.mgl.interval > 0)
    {
        double framerate = 60.0;

        if (@available(macOS 12.0, *))
        {
            NSScreen *screen = ((NSWindow *) window->ns.object).screen;
            NSInteger screen_rate = screen ? screen.maximumFramesPerSecond : 0;

            if (screen_rate > 0)
                framerate = (double) screen_rate;
        }

        const double period = window->context.mgl.interval / framerate;
        const uint64_t frequency = _glfwPlatformGetTimerFrequency();
        const double now = _glfwPlatformGetTimerValue() / (double) frequency;
        double due = window->context.mgl.last_swap;

        // a deadline rather than a delay, so sleeping a little long once does
        // not push every later frame out with it
        if (due <= 0.0 || due + period < now)
            due = now;
        else if (now < due)
            usleep((useconds_t) ((due - now) * 1e6));

        window->context.mgl.last_swap = due + period;
    }

    MGLswapBuffers(window->context.mgl.ctx);

    } // autoreleasepool
}

static void swapIntervalMGL(int interval)
{
    _GLFWwindow* window = _glfwPlatformGetTls(&_glfw.contextSlot);

    if (window == NULL)
        return;

    window->context.mgl.interval = interval;
    MGLsetSwapInterval(window->context.mgl.ctx, interval);
}

static int extensionSupportedMGL(const char* extension)
{
    // There are no MGL extensions
    return GLFW_FALSE;
}

static GLFWglproc getProcAddressMGL(const char* procname)
{
    GLFWproc symbol;

    assert(_glfw.mgl.handle);

    symbol = _glfwPlatformGetModuleSymbol(_glfw.mgl.handle, procname);

    // NULL is the right answer for a function MGL does not have. Loaders ask
    // for every extension entry point they know about and expect to be told no;
    // asserting here killed the process instead.
    if (symbol == NULL && getenv("MGL_DEBUG_CONTEXT"))
        fprintf(stderr, "MGLCTX: no entry point for %s\n", procname);

    return symbol;
}

static void destroyContextMGL(_GLFWwindow* window)
{
    @autoreleasepool {

    if (window->context.mgl.ctx)
        destroyGLMContext(window->context.mgl.ctx);

    // the reference this file took when it made the renderer; the context
    // gave back its own
    [window->context.mgl.renderer release];

    window->context.mgl.ctx = NULL;
    window->context.mgl.renderer = nil;

    } // autoreleasepool
}


//////////////////////////////////////////////////////////////////////////
//////                       GLFW internal API                      //////
//////////////////////////////////////////////////////////////////////////

// Initialize OpenGL support
//
GLFWbool _glfwInitMGL(void)
{
    if (_glfw.mgl.handle)
        return GLFW_TRUE;

    _glfw.mgl.handle = _glfwPlatformLoadModule("libmgl.dylib");
    assert(_glfw.mgl.handle);

    if (_glfw.mgl.handle == NULL)
    {
        _glfwInputError(GLFW_API_UNAVAILABLE,
                        "MGL: Failed to locate libmgl.dylib");
        return GLFW_FALSE;
    }

    return GLFW_TRUE;
}

// Terminate OpenGL support
//
void _glfwTerminateMGL(void)
{
}

// Create the OpenGL context
//
GLFWbool _glfwCreateContextMGL(_GLFWwindow* window,
                                const _GLFWctxconfig* ctxconfig,
                                const _GLFWfbconfig* fbconfig)
{
    if (ctxconfig->client == GLFW_OPENGL_ES_API)
    {
        _glfwInputError(GLFW_API_UNAVAILABLE,
                        "MGL: OpenGL ES is not available on macOS");
        return GLFW_FALSE;
    }

    if (ctxconfig->major < 4)
    {
        _glfwInputError(GLFW_VERSION_UNAVAILABLE,
                        "MGL: OpenGL 4.6 and above supported on MGL");
        return GLFW_FALSE;
    }

    if (ctxconfig->minor < 6)
    {
        _glfwInputError(GLFW_VERSION_UNAVAILABLE,
                        "MGL: OpenGL 4.6 and above supported on MGL");
        return GLFW_FALSE;
    }

    window->context.mgl.ctx = createGLMContext(GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                                               GL_DEPTH_COMPONENT, GL_FLOAT,
                                               0, 0);
    assert(window->context.mgl.ctx);

    if (window->context.mgl.ctx == nil)
    {
        _glfwInputError(GLFW_VERSION_UNAVAILABLE,
                        "MGL: Failed to create MGL context");
        return GLFW_FALSE;
    }

    // must be a setter: without a layer-backed view the CAMetalLayer below
    // is never composited and the window just stays black
    [window->ns.view setWantsLayer:YES];

    MGLRenderer *renderer = [[MGLRenderer alloc] init];
    assert(renderer);

    window->context.mgl.renderer = renderer;

    [window->context.mgl.renderer createMGLRendererAndBindToContext: window->context.mgl.ctx view: window->ns.view];

    //[window->context.mgl.object setView: window->ns.view];

    if (getenv("MGL_DEBUG_CONTEXT"))
        fprintf(stderr, "MGLCTX: _glfwCreateContextMGL ran, ctx=%p renderer=%p view=%p\n",
                (void *)window->context.mgl.ctx, (__bridge void *)renderer,
                (__bridge void *)window->ns.view);

    window->context.makeCurrent = makeContextCurrentMGL;
    window->context.swapBuffers = swapBuffersMGL;
    window->context.swapInterval = swapIntervalMGL;
    window->context.extensionSupported = extensionSupportedMGL;
    window->context.getProcAddress = getProcAddressMGL;
    window->context.destroy = destroyContextMGL;

    return GLFW_TRUE;
}


//////////////////////////////////////////////////////////////////////////
//////                        GLFW native API                       //////
//////////////////////////////////////////////////////////////////////////

GLFWAPI void * glfwGetMGLContext(GLFWwindow* handle)
{
    _GLFWwindow* window = (_GLFWwindow*) handle;
    _GLFW_REQUIRE_INIT_OR_RETURN(nil);

    if (_glfw.platform.platformID != GLFW_PLATFORM_COCOA)
    {
        _glfwInputError(GLFW_PLATFORM_UNAVAILABLE,
                        "MGL: Platform not initialized");
        return nil;
    }

    if (window->context.source != GLFW_NATIVE_CONTEXT_API)
    {
        _glfwInputError(GLFW_NO_WINDOW_CONTEXT, NULL);
        return nil;
    }

    return window->context.mgl.ctx;
}

