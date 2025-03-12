/**
 * @file egl.c
 * @brief EGL OpenGL extension module
 */
/*****************************************************************************
 * Copyright © 2010-2014 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <assert.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_opengl.h>
#include <vlc_vout_window.h>
#ifdef USE_PLATFORM_X11
# include <vlc_xlib.h>
#endif
#ifdef USE_PLATFORM_WAYLAND
# include <wayland-egl.h>
#endif
#if defined (USE_PLATFORM_ANDROID)
# include "../android/utils.h"
#endif
#if defined (USE_PLATFORM_QNX)
# include <screen/screen.h>
#endif

typedef struct vlc_gl_sys_t
{
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
#if defined (USE_PLATFORM_X11)
    Display *x11;
    bool restore_forget_gravity;
#endif
#if defined (USE_PLATFORM_WAYLAND)
    struct wl_egl_window *window;
    unsigned width, height;
#endif
#if defined (USE_PLATFORM_QNX)
    screen_window_t window;
    EGLConfig cfg;
#endif
    PFNEGLCREATEIMAGEKHRPROC    eglCreateImageKHR;
    PFNEGLDESTROYIMAGEKHRPROC   eglDestroyImageKHR;
} vlc_gl_sys_t;

static EGLSurface CreateWindowSurface(EGLDisplay dpy, EGLConfig config, void *window, const EGLint *attrs);

static int MakeCurrent (vlc_gl_t *gl)
{
    vlc_gl_sys_t *sys = gl->sys;

    if (eglMakeCurrent (sys->display, sys->surface, sys->surface,
                        sys->context) != EGL_TRUE)
        return VLC_EGENERIC;
    return VLC_SUCCESS;
}

static void ReleaseCurrent (vlc_gl_t *gl)
{
    vlc_gl_sys_t *sys = gl->sys;

    eglMakeCurrent (sys->display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                    EGL_NO_CONTEXT);
}

#ifdef USE_PLATFORM_WAYLAND
static void Resize (vlc_gl_t *gl, unsigned width, unsigned height)
{
    vlc_gl_sys_t *sys = gl->sys;

    wl_egl_window_resize(sys->window, width, height,
                         (sys->width - width) / 2,
                         (sys->height - height) / 2);
    sys->width = width;
    sys->height = height;
}

#elif defined(USE_PLATFORM_QNX)
static void Resize (vlc_gl_t *gl, unsigned width, unsigned height)
{
    vlc_gl_sys_t *sys = gl->sys;
    int dims[2] = {0, 0};
    int buf_dims[2] = {0, 0};
    EGLint surf_dims[2] = {0, 0};
    int make_surface = 0;
    int make_buffers;

    /* The width/height passed into this function represent the width/height of the
     * (scaled) video image on the display. NOT the surface that image is rendered to.
     * Query to get the current size of the underlying window and use that instead
     * as that seems to be the actual size of the entire window (video + letterboxing)
     * that is supposed to be visible.
     */
    (void)width;
    (void)height;
    screen_get_window_property_iv(sys->window, SCREEN_PROPERTY_SIZE, dims);

    /* Figure out if the window's buffers actually need a resize */
    screen_get_window_property_iv(sys->window, SCREEN_PROPERTY_BUFFER_SIZE, buf_dims);
    make_buffers = dims[0] != buf_dims[0] || dims[1] != buf_dims[1];

    /* If there already is a surface, see if it needs to be changed */
    if (sys->surface != EGL_NO_SURFACE) {
        eglQuerySurface(sys->display, sys->surface, EGL_WIDTH, &surf_dims[0]);
        eglQuerySurface(sys->display, sys->surface, EGL_HEIGHT, &surf_dims[1]);

        if (make_buffers || dims[0] != surf_dims[0] || dims[1] != surf_dims[1]) {
            eglDestroySurface(sys->display, sys->surface);
            sys->surface = EGL_NO_SURFACE;
            make_surface = 1;
        }
    }

    /* Recreate the screen buffers if required */
    if (make_buffers) {
        screen_destroy_window_buffers(sys->window);
        screen_set_window_property_iv(sys->window, SCREEN_PROPERTY_BUFFER_SIZE, dims);
        screen_create_window_buffers(sys->window, 2);
    }

    /* Re-create the surface (if required) */
    if (make_surface) {
        sys->surface = CreateWindowSurface(sys->display, sys->cfg, &sys->window, NULL);
    }
}

#else
# define Resize (NULL)
#endif

static void SwapBuffers (vlc_gl_t *gl)
{
    vlc_gl_sys_t *sys = gl->sys;

    eglSwapBuffers (sys->display, sys->surface);
}

static void *GetSymbol(vlc_gl_t *gl, const char *procname)
{
    (void) gl;
    return (void *)eglGetProcAddress (procname);
}

static const char *QueryString(vlc_gl_t *gl, int32_t name)
{
    vlc_gl_sys_t *sys = gl->sys;

    return eglQueryString(sys->display, name);
}

static void *CreateImageKHR(vlc_gl_t *gl, unsigned target, void *buffer,
                            const int32_t *attrib_list)
{
    vlc_gl_sys_t *sys = gl->sys;

    return sys->eglCreateImageKHR(sys->display, NULL, target, buffer,
                                  attrib_list);
}

static bool DestroyImageKHR(vlc_gl_t *gl, void *image)
{
    vlc_gl_sys_t *sys = gl->sys;

    return sys->eglDestroyImageKHR(sys->display, image);
}

static bool CheckToken(const char *haystack, const char *needle)
{
    size_t len = strlen(needle);

    while (haystack != NULL)
    {
        while (*haystack == ' ')
            haystack++;
        if (!strncmp(haystack, needle, len)
         && (memchr(" ", haystack[len], 2) != NULL))
            return true;

        haystack = strchr(haystack, ' ');
    }
    return false;
}

static bool CheckAPI (EGLDisplay dpy, const char *api)
{
    const char *apis = eglQueryString (dpy, EGL_CLIENT_APIS);
    return CheckToken(apis, api);
}

static bool CheckClientExt(const char *name)
{
    const char *exts = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    return CheckToken(exts, name);
}

struct gl_api
{
   const char name[10];
   EGLenum    api;
   EGLint     min_minor;
   EGLint     render_bit;
   EGLint     attr[3];
};

#ifdef EGL_EXT_platform_base
static EGLDisplay GetDisplayEXT(EGLenum plat, void *dpy, const EGLint *attrs)
{
    PFNEGLGETPLATFORMDISPLAYEXTPROC getDisplay =
        (PFNEGLGETPLATFORMDISPLAYEXTPROC)
        eglGetProcAddress("eglGetPlatformDisplayEXT");

    assert(getDisplay != NULL);
    return getDisplay(plat, dpy, attrs);
}

static EGLSurface CreateWindowSurfaceEXT(EGLDisplay dpy, EGLConfig config,
                                         void *window, const EGLint *attrs)
{
    PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC createSurface =
        (PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC)
        eglGetProcAddress("eglCreatePlatformWindowSurfaceEXT");

    assert(createSurface != NULL);
    return createSurface(dpy, config, window, attrs);
}
#endif

static EGLSurface CreateWindowSurface(EGLDisplay dpy, EGLConfig config,
                                      void *window, const EGLint *attrs)
{
    EGLNativeWindowType *native = window;

    return eglCreateWindowSurface(dpy, config, *native, attrs);
}

static void Close (vlc_object_t *obj)
{
    vlc_gl_t *gl = (vlc_gl_t *)obj;
    vlc_gl_sys_t *sys = gl->sys;

    if (sys->display != EGL_NO_DISPLAY)
    {
        if (sys->context != EGL_NO_CONTEXT)
            eglDestroyContext(sys->display, sys->context);
        if (sys->surface != EGL_NO_SURFACE)
            eglDestroySurface(sys->display, sys->surface);
        eglTerminate(sys->display);
    }
#ifdef USE_PLATFORM_X11
    if (sys->x11 != NULL) {
        if (sys->restore_forget_gravity) {
            XSetWindowAttributes swa;
            swa.bit_gravity = ForgetGravity;
            XChangeWindowAttributes (sys->x11, gl->surface->handle.xid,
                                     CWBitGravity, &swa);
        }
        XCloseDisplay(sys->x11);
    }
#endif
#ifdef USE_PLATFORM_WAYLAND
    if (sys->window != NULL)
        wl_egl_window_destroy(sys->window);
#endif
#ifdef USE_PLATFORM_ANDROID
    AWindowHandler_releaseANativeWindow(gl->surface->handle.anativewindow,
                                        AWindow_Video);
#endif
    free (sys);
}

/**
 * Probe EGL display availability
 */
static int Open (vlc_object_t *obj, const struct gl_api *api)
{
    vlc_gl_t *gl = (vlc_gl_t *)obj;
    vlc_gl_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    gl->sys = sys;
    sys->display = EGL_NO_DISPLAY;
    sys->surface = EGL_NO_SURFACE;
    sys->context = EGL_NO_CONTEXT;
    sys->eglCreateImageKHR = NULL;
    sys->eglDestroyImageKHR = NULL;

    vout_window_t *wnd = gl->surface;
    EGLSurface (*createSurface)(EGLDisplay, EGLConfig, void *, const EGLint *)
        = CreateWindowSurface;
    void *window;

#ifdef USE_PLATFORM_X11
    sys->x11 = NULL;

    if (wnd->type != VOUT_WINDOW_TYPE_XID || !vlc_xlib_init(obj))
        goto error;

    window = &wnd->handle.xid;
    sys->x11 = XOpenDisplay(wnd->display.x11);
    if (sys->x11 == NULL)
        goto error;

    int snum;
    {
        XWindowAttributes wa;
        XSetWindowAttributes swa;

        sys->restore_forget_gravity = false;
        if (!XGetWindowAttributes(sys->x11, wnd->handle.xid, &wa))
            goto error;
        snum = XScreenNumberOfScreen(wa.screen);
        if (wa.bit_gravity == ForgetGravity) {
            swa.bit_gravity = NorthWestGravity;
            XChangeWindowAttributes(sys->x11, wnd->handle.xid, CWBitGravity,
                                    &swa);
            sys->restore_forget_gravity = true;
        }
    }
# ifdef EGL_EXT_platform_x11
    if (CheckClientExt("EGL_EXT_platform_x11"))
    {
        const EGLint attrs[] = {
            EGL_PLATFORM_X11_SCREEN_EXT, snum,
            EGL_NONE
        };
        sys->display = GetDisplayEXT(EGL_PLATFORM_X11_EXT, sys->x11, attrs);
        createSurface = CreateWindowSurfaceEXT;
    }
    else
# endif
    {
# ifdef __unix__
        if (snum == XDefaultScreen(sys->x11))
            sys->display = eglGetDisplay(sys->x11);
# endif
    }

#elif defined (USE_PLATFORM_WAYLAND)
    sys->window = NULL;

    if (wnd->type != VOUT_WINDOW_TYPE_WAYLAND)
        goto error;

# ifdef EGL_EXT_platform_wayland
    if (!CheckClientExt("EGL_EXT_platform_wayland"))
        goto error;

    /* Resize() should be called with the proper size before Swap() */
    window = wl_egl_window_create(wnd->handle.wl, 1, 1);
    if (window == NULL)
        goto error;
    sys->window = window;

    sys->display = GetDisplayEXT(EGL_PLATFORM_WAYLAND_EXT, wnd->display.wl,
                                 NULL);
    createSurface = CreateWindowSurfaceEXT;

# endif

#elif defined (USE_PLATFORM_WIN32)
    if (wnd->type != VOUT_WINDOW_TYPE_HWND)
        goto error;

    window = &wnd->handle.hwnd;
# if defined (_WIN32) || defined (__VC32__) \
  && !defined (__CYGWIN__) && !defined (__SCITECH_SNAP__)
    sys->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
# endif

#elif defined (USE_PLATFORM_ANDROID)
    if (wnd->type != VOUT_WINDOW_TYPE_ANDROID_NATIVE)
        goto error;

    ANativeWindow *anw =
        AWindowHandler_getANativeWindow(wnd->handle.anativewindow,
                                        AWindow_Video);
    if (anw == NULL)
        goto error;
    window = &anw;
# if defined (__ANDROID__) || defined (ANDROID)
    sys->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
# endif

#elif defined (USE_PLATFORM_QNX)
    if (wnd->type != VOUT_WINDOW_TYPE_QNX)
        goto error;
    window = &wnd->handle.scrWin;

    /* Make sure that the window supports GLES2 */
    int iprop;
    if (screen_get_window_property_iv(wnd->handle.scrWin, SCREEN_PROPERTY_USAGE, &iprop))
        goto error;
    iprop |= SCREEN_USAGE_OPENGL_ES2;
    if (screen_set_window_property_iv(wnd->handle.scrWin, SCREEN_PROPERTY_USAGE, &iprop))
        goto error;

    /* Change transparency as QT sets it to DISCARD which makes it permanently invisible */
    if (screen_get_window_property_iv(wnd->handle.scrWin, SCREEN_PROPERTY_TRANSPARENCY, &iprop))
        goto error;
    if (iprop == SCREEN_TRANSPARENCY_DISCARD) {
        iprop = SCREEN_TRANSPARENCY_NONE;
        if (screen_set_window_property_iv(wnd->handle.scrWin, SCREEN_PROPERTY_TRANSPARENCY, &iprop))
            goto error;
    }

    sys->window = wnd->handle.scrWin;
    sys->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
#endif

    if (sys->display == EGL_NO_DISPLAY)
        goto error;

    /* Initialize EGL display */
    EGLint major, minor;
    if (eglInitialize(sys->display, &major, &minor) != EGL_TRUE)
        goto error;
    msg_Dbg(obj, "EGL version %s by %s",
            eglQueryString(sys->display, EGL_VERSION),
            eglQueryString(sys->display, EGL_VENDOR));

    const char *ext = eglQueryString(sys->display, EGL_EXTENSIONS);
    if (*ext)
        msg_Dbg(obj, " extensions: %s", ext);

    if (major != 1 || minor < api->min_minor
     || !CheckAPI(sys->display, api->name))
    {
        msg_Err(obj, "cannot select %s API", api->name);
        goto error;
    }

    const EGLint conf_attr[] = {
        EGL_RED_SIZE, 5,
        EGL_GREEN_SIZE, 5,
        EGL_BLUE_SIZE, 5,
        EGL_RENDERABLE_TYPE, api->render_bit,
        EGL_NONE
    };
    EGLConfig cfgv[1];
    EGLint cfgc;

    if (eglChooseConfig(sys->display, conf_attr, cfgv, 1, &cfgc) != EGL_TRUE
     || cfgc == 0)
    {
        msg_Err (obj, "cannot choose EGL configuration");
        goto error;
    }
#if defined (USE_PLATFORM_QNX)
    sys->cfg = cfgv[0];
#endif

    /* Create a drawing surface */
    sys->surface = createSurface(sys->display, cfgv[0], window, NULL);
    if (sys->surface == EGL_NO_SURFACE)
    {
        msg_Err (obj, "cannot create EGL window surface");
        goto error;
    }

    if (eglBindAPI (api->api) != EGL_TRUE)
    {
        msg_Err (obj, "cannot bind EGL API");
        goto error;
    }

    EGLContext ctx = eglCreateContext(sys->display, cfgv[0], EGL_NO_CONTEXT,
                                      api->attr);
    if (ctx == EGL_NO_CONTEXT)
    {
        msg_Err (obj, "cannot create EGL context");
        goto error;
    }
    sys->context = ctx;

    /* Initialize OpenGL callbacks */
    gl->ext = VLC_GL_EXT_EGL;
    gl->makeCurrent = MakeCurrent;
    gl->releaseCurrent = ReleaseCurrent;
    gl->resize = Resize;
    gl->swap = SwapBuffers;
    gl->getProcAddress = GetSymbol;
    gl->egl.queryString = QueryString;

    sys->eglCreateImageKHR = (void *)eglGetProcAddress("eglCreateImageKHR");
    sys->eglDestroyImageKHR = (void *)eglGetProcAddress("eglDestroyImageKHR");
    if (sys->eglCreateImageKHR != NULL && sys->eglDestroyImageKHR != NULL)
    {
        gl->egl.createImageKHR = CreateImageKHR;
        gl->egl.destroyImageKHR = DestroyImageKHR;
    }

    return VLC_SUCCESS;

error:
    Close (obj);
    return VLC_EGENERIC;
}

static int OpenGLES2 (vlc_object_t *obj)
{
    static const struct gl_api api = {
        "OpenGL_ES", EGL_OPENGL_ES_API, 3, EGL_OPENGL_ES2_BIT,
        { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE },
    };
    return Open (obj, &api);
}

static int OpenGL (vlc_object_t *obj)
{
    static const struct gl_api api = {
        "OpenGL", EGL_OPENGL_API, 4, EGL_OPENGL_BIT,
        { EGL_NONE },
    };
    return Open (obj, &api);
}

vlc_module_begin ()
    set_shortname (N_("EGL"))
    set_description (N_("EGL extension for OpenGL"))
    set_category (CAT_VIDEO)
    set_subcategory (SUBCAT_VIDEO_VOUT)
    set_capability ("opengl", 50)
    set_callbacks (OpenGL, Close)
    add_shortcut ("egl")

    add_submodule ()
    set_capability ("opengl es2", 50)
    set_callbacks (OpenGLES2, Close)
    add_shortcut ("egl")

vlc_module_end ()
