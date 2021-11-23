/* 
 *  This file is part of Permafrost Engine. 
 *  Copyright (C) 2017-2020 Eduard Permyakov 
 *
 *  Permafrost Engine is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Permafrost Engine is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 *  Linking this software statically or dynamically with other modules is making 
 *  a combined work based on this software. Thus, the terms and conditions of 
 *  the GNU General Public License cover the whole combination. 
 *  
 *  As a special exception, the copyright holders of Permafrost Engine give 
 *  you permission to link Permafrost Engine with independent modules to produce 
 *  an executable, regardless of the license terms of these independent 
 *  modules, and to copy and distribute the resulting executable under 
 *  terms of your choice, provided that you also meet, for each linked 
 *  independent module, the terms and conditions of the license of that 
 *  module. An independent module is a module which is not derived from 
 *  or based on Permafrost Engine. If you modify Permafrost Engine, you may 
 *  extend this exception to your version of Permafrost Engine, but you are not 
 *  obliged to do so. If you do not wish to do so, delete this exception 
 *  statement from your version.
 *
 */

#include "public/render.h"
#include "public/render_ctrl.h"
#include "gl_shader.h"
#include "gl_texture.h"
#include "gl_render.h"
#include "gl_assert.h"
#include "gl_state.h"
#include "gl_batch.h"
#include "../settings.h"
#include "../main.h"
#include "../ui.h"
#include "../game/public/game.h"

#include <assert.h>
#include <math.h>

#include <SDL.h>
#include <GL/glew.h>
#include <SDL_opengl.h>


#define EPSILON     (1.0f/1024)
#define ARR_SIZE(a) (sizeof(a)/sizeof(a[0]))

/*****************************************************************************/
/* GLOBAL VARIABLES                                                          */
/*****************************************************************************/

bool g_trace_gpu;

/*****************************************************************************/
/* STATIC VARIABLES                                                          */
/*****************************************************************************/

static SDL_GLContext s_context;

/* write-once strings. Set by render thread at initialization */
char                 s_info_vendor[128];
char                 s_info_renderer[128];
char                 s_info_version[128];
char                 s_info_sl_version[128];

/*****************************************************************************/
/* STATIC FUNCTIONS                                                          */
/*****************************************************************************/

static bool ar_validate(const struct sval *new_val)
{
    if(new_val->type != ST_TYPE_VEC2)
        return false;

    float AR_MIN = 0.5f, AR_MAX = 2.5f;
    return (new_val->as_vec2.x / new_val->as_vec2.y >= AR_MIN)
        && (new_val->as_vec2.x / new_val->as_vec2.y <= AR_MAX);
}

static void ar_commit(const struct sval *new_val)
{
    struct sval res;
    ss_e status = Settings_Get("pf.video.resolution", &res);
    if(status == SS_NO_SETTING)
        return;

    assert(status == SS_OKAY);
    float curr_ratio = res.as_vec2.x/res.as_vec2.y;
    float new_ratio = new_val->as_vec2.x/new_val->as_vec2.y;
    if(fabs(new_ratio - curr_ratio) < EPSILON)
        return;

    /* Here, we choose to always decrease a dimension rather than 
     * increase one so the window continues to fit on the screen */
    struct sval new_res = {.type = ST_TYPE_VEC2};
    if(new_ratio > curr_ratio) {

        new_res.as_vec2 = (vec2_t){
            .x = res.as_vec2.x,
            .y = res.as_vec2.y / (new_ratio/curr_ratio)
        };
    }else{
    
        new_res.as_vec2 = (vec2_t){
            .x = res.as_vec2.x / (curr_ratio/new_ratio),
            .y = res.as_vec2.y
        };
    }

    status = Settings_SetNoValidate("pf.video.resolution", &new_res);
    assert(status == SS_OKAY);
}

static bool res_validate(const struct sval *new_val)
{
    if(new_val->type != ST_TYPE_VEC2)
        return false;

    struct sval ar;
    ss_e status = Settings_Get("pf.video.aspect_ratio", &ar);
    if(status != SS_NO_SETTING) {

        assert(status == SS_OKAY);
        float set_ar = ar.as_vec2.x / ar.as_vec2.y;
        if(fabs(new_val->as_vec2.x / new_val->as_vec2.y - set_ar) > EPSILON)
            return false;
    }

    const int DIM_MIN = 360, DIM_MAX = 5120;

    return (new_val->as_vec2.x >= DIM_MIN && new_val->as_vec2.x <= DIM_MAX)
        && (new_val->as_vec2.y >= DIM_MIN && new_val->as_vec2.y <= DIM_MAX);
}

static void res_commit(const struct sval *new_val)
{
    int rval = Engine_SetRes(new_val->as_vec2.x, new_val->as_vec2.y);
    assert(0 == rval || fprintf(stderr, "Failed to set window resolution:%s\n", SDL_GetError()));

    int width, height;
    Engine_WinDrawableSize(&width, &height);

    int viewport[4] = {0, 0, width, height};
    R_PushCmd((struct rcmd){
        .func = R_GL_SetViewport,
        .nargs = 4,
        .args = {
            R_PushArg(&viewport[0], sizeof(viewport[0])),
            R_PushArg(&viewport[1], sizeof(viewport[1])),
            R_PushArg(&viewport[2], sizeof(viewport[2])),
            R_PushArg(&viewport[3], sizeof(viewport[3])),
        },
    });
}

static bool dm_validate(const struct sval *new_val)
{
    assert(new_val->type == ST_TYPE_INT);
    if(new_val->type != ST_TYPE_INT)
        return false;

    return new_val->as_int == PF_WF_FULLSCREEN
        || new_val->as_int == PF_WF_BORDERLESS_WIN
        || new_val->as_int == PF_WF_WINDOW;
}

static void dm_commit(const struct sval *new_val)
{
    Engine_SetDispMode(new_val->as_int);
}

static bool bool_val_validate(const struct sval *new_val)
{
    return (new_val->type == ST_TYPE_BOOL);
}

static void render_set_swap(const bool *on)
{
    SDL_GL_SetSwapInterval(*on);
}

static void vsync_commit(const struct sval *new_val)
{
    R_PushCmd((struct rcmd){
        .func = render_set_swap,
        .nargs = 1,
        .args = { R_PushArg(&new_val->as_bool, sizeof(bool)) }
    });
}

static bool int_val_validate(const struct sval *new_val)
{
    return (new_val->type == ST_TYPE_INT);
}

static void render_set_logmask(int *mask)
{
    if(!GLEW_KHR_debug)
        return;

    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, 
        GL_DEBUG_SEVERITY_HIGH, 0, NULL, (*mask & 0x1) ? GL_TRUE : GL_FALSE);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, 
        GL_DEBUG_SEVERITY_MEDIUM, 0, NULL, (*mask & 0x2) ? GL_TRUE : GL_FALSE);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, 
        GL_DEBUG_SEVERITY_LOW, 0, NULL, (*mask & 0x4) ? GL_TRUE : GL_FALSE);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, 
        GL_DEBUG_SEVERITY_NOTIFICATION, 0, NULL, (*mask & 0x8) ? GL_TRUE : GL_FALSE);
}

static void debug_logmask_commit(const struct sval *new_val)
{
    R_PushCmd((struct rcmd){
        .func = render_set_logmask,
        .nargs = 1,
        .args = { R_PushArg(&new_val->as_int, sizeof(int)) },
    });
}

static void render_set_trace_gpu(const bool *on)
{
    g_trace_gpu = *on; 
}

static void trace_gpu_commit(const struct sval *new_val)
{
    R_PushCmd((struct rcmd){
        .func = render_set_trace_gpu,
        .nargs = 1,
        .args = { R_PushArg(&new_val->as_bool, sizeof(bool)) }
    });
}

static bool render_wait_cmd(struct render_sync_state *rstate)
{
    SDL_LockMutex(rstate->sq_lock);
    while(!rstate->start && !rstate->quit)
        SDL_CondWait(rstate->sq_cond, rstate->sq_lock);

    if(rstate->quit) {

        rstate->quit = false;
        SDL_UnlockMutex(rstate->sq_lock);
        return true;
    }
    
    assert(rstate->start == true);
    rstate->start = false;
    SDL_UnlockMutex(rstate->sq_lock);
    return false;
}

static void render_signal_done(struct render_sync_state *rstate)
{
    SDL_LockMutex(rstate->done_lock);
    rstate->done = true;
    SDL_CondSignal(rstate->done_cond);
    SDL_UnlockMutex(rstate->done_lock);
}

static const char *source_str(GLenum source)
{
    switch(source) {
    case GL_DEBUG_SOURCE_API:             return "API";
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   return "Window System";
    case GL_DEBUG_SOURCE_SHADER_COMPILER: return "Shader Compiler";
    case GL_DEBUG_SOURCE_THIRD_PARTY:     return "Third Party";
    case GL_DEBUG_SOURCE_APPLICATION:     return "Application";
    case GL_DEBUG_SOURCE_OTHER:           return "Other";
    default: assert(0); return NULL;
    }
}

static const char *type_str(GLenum type)
{
    switch(type) {
    case GL_DEBUG_TYPE_ERROR:               return "Error";
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: return "Depricated Behavior";
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  return "Undefined Behavior";
    case GL_DEBUG_TYPE_PORTABILITY:         return "Portability";
    case GL_DEBUG_TYPE_PERFORMANCE:         return "Performance";
    case GL_DEBUG_TYPE_MARKER:              return "Marker";
    case GL_DEBUG_TYPE_PUSH_GROUP:          return "Push Group";
    case GL_DEBUG_TYPE_POP_GROUP:           return "Pop Group";
    case GL_DEBUG_TYPE_OTHER:               return "Other";
    default: assert(0); return NULL;
    }
}

static const char *severity_str(GLenum severity)
{
    switch(severity) {
    case GL_DEBUG_SEVERITY_HIGH:         return "High";
    case GL_DEBUG_SEVERITY_MEDIUM:       return "Medium";
    case GL_DEBUG_SEVERITY_LOW:          return "Low";
    case GL_DEBUG_SEVERITY_NOTIFICATION: return "Notification";
    default: assert(0); return NULL;
    }
}

void debug_callback(GLenum source, GLenum type, GLuint id, GLenum severity,
                    GLsizei length, const GLchar *message, const void *user)
{
    fprintf(stderr, " *** [%s][%s][%s] %s\n", source_str(source), type_str(type), 
        severity_str(severity), message);
    fflush(stderr);
}

static void render_init_ctx(struct render_init_arg *arg)
{
    SDL_GL_MakeCurrent(arg->in_window, s_context);

    glewExperimental = GL_TRUE;
    if(glewInit() != GLEW_OK) {
        fprintf(stderr, "Failed to initialize GLEW\n");
        arg->out_success = false;
        return;
    }

    if(!GLEW_VERSION_3_3) {
        fprintf(stderr, "Required OpenGL version not supported in GLEW\n");
        arg->out_success = false;
        return;
    }

    if(GLEW_KHR_debug) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(debug_callback, NULL);
    }

    int vp[4] = {0 ,0, arg->in_width, arg->in_height};
    R_GL_SetViewport(&vp[0], &vp[1], &vp[2], &vp[3]);
    R_GL_GlobalConfig();

    if(!R_GL_Shader_InitAll(g_basepath)
    || !R_GL_Texture_Init()
    || !R_GL_StateInit()
    || !R_GL_Batch_Init()) {

        arg->out_success = false;
        return;
    }

    R_GL_InitShadows();

    strncpy(s_info_vendor,     (const char*)glGetString(GL_VENDOR),   ARR_SIZE(s_info_vendor)-1);
    strncpy(s_info_renderer,   (const char*)glGetString(GL_RENDERER), ARR_SIZE(s_info_renderer)-1);
    strncpy(s_info_version,    (const char*)glGetString(GL_VERSION),  ARR_SIZE(s_info_version)-1);
    strncpy(s_info_sl_version, (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION), ARR_SIZE(s_info_sl_version)-1);

    arg->out_success = true;
}

static void render_destroy_ctx(void)
{
    R_GL_Batch_Shutdown();
    R_GL_StateShutdown();
    R_GL_Texture_Shutdown();
    SDL_GL_DeleteContext(s_context);
}

static void render_dispatch_cmd(struct rcmd cmd)
{
    switch(cmd.nargs) {
    case 0:
        ((void(*)(void)) cmd.func)();
        break;
    case 1:
        ((void(*)(void*)) cmd.func)(
            cmd.args[0]
        );
        break;
    case 2:
        ((void(*)(void*, void*)) cmd.func)(
            cmd.args[0],
            cmd.args[1]
        );
        break;
    case 3:
        ((void(*)(void*, void*, 
                  void*)) cmd.func)(
            cmd.args[0],
            cmd.args[1],
            cmd.args[2]
        );
        break;
    case 4:
        ((void(*)(void*, void*,
                  void*, void*)) cmd.func)(
            cmd.args[0],
            cmd.args[1],
            cmd.args[2],
            cmd.args[3]
        );
        break;
    case 5:
        ((void(*)(void*, void*, 
                  void*, void*, 
                  void*)) cmd.func)(
            cmd.args[0],
            cmd.args[1],
            cmd.args[2],
            cmd.args[3],
            cmd.args[4]
        );
        break;
    case 6:
        ((void(*)(void*, void*,
                  void*, void*,
                  void*, void*)) cmd.func)(
            cmd.args[0],
            cmd.args[1],
            cmd.args[2],
            cmd.args[3],
            cmd.args[4],
            cmd.args[5]
        );
        break;
    case 7:
        ((void(*)(void*, void*, 
                  void*, void*, 
                  void*, void*, 
                  void*)) cmd.func)(
            cmd.args[0],
            cmd.args[1],
            cmd.args[2],
            cmd.args[3],
            cmd.args[4],
            cmd.args[5],
            cmd.args[6]
        );
        break;
    case 8:
        ((void(*)(void*, void*,
                  void*, void*,
                  void*, void*,
                  void*, void*)) cmd.func)(
            cmd.args[0],
            cmd.args[1],
            cmd.args[2],
            cmd.args[3],
            cmd.args[4],
            cmd.args[5],
            cmd.args[6],
            cmd.args[7]
        );
        break;
    default: assert(0);
    }
}

static void render_process_cmds(queue_rcmd_t *cmds)
{
    while(queue_size(*cmds) > 0) {

        struct rcmd curr;
        queue_rcmd_pop(cmds, &curr);
        render_dispatch_cmd(curr);
        GL_ASSERT_OK();
    }
}

#ifdef __APPLE__
typedef void* SDLTranslatorResponder;
typedef long NSInteger; // https://developer.apple.com/documentation/objectivec/nsinteger?language=objc

//typedef bool BOOL; // https://developer.apple.com/documentation/objectivec/bool?language=objc
#import <objc/runtime.h>
#import <objc/message.h>

typedef uint32_t IOPMAssertionID; // https://developer.apple.com/documentation/iokit/iopmassertionid
typedef struct SDL_VideoData_impl // SDL/src/video/cocoa/SDL_cocoavideo.h
{
    int allow_spaces;
    unsigned int modifierFlags;
    void *key_layout;
    SDLTranslatorResponder *fieldEdit;
    NSInteger clipboard_count;
    Uint32 screensaver_activity;
    BOOL screensaver_use_iopm;
    IOPMAssertionID screensaver_assertion;
    SDL_mutex *swaplock;
} SDL_VideoData_impl;

typedef void* SDL_WindowShaper;
typedef void* SDL_WindowUserData;
// SDL/src/video/SDL_sysvideo.h from SDL version 2.0.14 (tag release-2.0.14 in https://github.com/libsdl-org/SDL )
/* Define the SDL window structure, corresponding to toplevel windows */
struct SDL_Window_impl
{
    const void *magic;
    Uint32 id;
    char *title;
    SDL_Surface *icon;
    int x, y;
    int w, h;
    int min_w, min_h;
    int max_w, max_h;
    Uint32 flags;
    Uint32 last_fullscreen_flags;
    //Uint32 display_index;

    /* Stored position and size for windowed mode */
    SDL_Rect windowed;

    SDL_DisplayMode fullscreen_mode;

    float opacity;

    float brightness;
    Uint16 *gamma;
    Uint16 *saved_gamma;        /* (just offset into gamma) */

    SDL_Surface *surface;
    SDL_bool surface_valid;

    SDL_bool is_hiding;
    SDL_bool is_destroying;
    SDL_bool is_dropping;       /* drag/drop in progress, expecting SDL_SendDropComplete(). */

    //SDL_Rect mouse_rect;

    SDL_WindowShaper *shaper;

    SDL_HitTest hit_test;
    void *hit_test_data;

    SDL_WindowUserData *data;

    void *driverdata;

    SDL_Window *prev;
    SDL_Window *next;
};

typedef void* NSWindow;
typedef void* NSView;
typedef void* NSMutableArray;
typedef void* Cocoa_WindowListener;
typedef void *EGLSurface; // https://www.khronos.org/registry/EGL/api/EGL/egl.h
struct SDL_WindowData_impl // SDL/src/video/cocoa/SDL_cocoawindow.h from SDL version 2.0.14 (tag release-2.0.14 in https://github.com/libsdl-org/SDL )
{
    SDL_Window *window;
    NSWindow *nswindow;
    NSView *sdlContentView;
    NSMutableArray *nscontexts;
    SDL_bool created;
    SDL_bool inWindowFullscreenTransition;
    //NSInteger flash_request;
    Cocoa_WindowListener *listener;
    struct SDL_VideoData_impl *videodata;
#if SDL_VIDEO_OPENGL_EGL
    EGLSurface egl_surface;
#endif
};


// https://gist.github.com/mmassaki/3892543
void Swizzle(Class c, SEL orig, SEL new)
{
    Method origMethod = class_getInstanceMethod(c, orig);
    Method newMethod = class_getInstanceMethod(c, new);
    if(class_addMethod(c, orig, method_getImplementation(newMethod), method_getTypeEncoding(newMethod))) // Returns: "YES if the method was added successfully, otherwise NO (for example, the class already contains a method implementation with that name)." ( https://developer.apple.com/documentation/objectivec/1418901-class_addmethod?language=objc )
        class_replaceMethod(c, new, method_getImplementation(origMethod), method_getTypeEncoding(origMethod));
    else
        method_exchangeImplementations(origMethod, newMethod);
}

// More hacks //

void Replace(Class c, SEL orig, SEL new)
{
    Method origMethod = class_getInstanceMethod(c, orig);
    Method newMethod = class_getInstanceMethod(c, new);
    if(class_addMethod(c, orig, method_getImplementation(newMethod), method_getTypeEncoding(newMethod))) // Returns: "YES if the method was added successfully, otherwise NO (for example, the class already contains a method implementation with that name)." ( https://developer.apple.com/documentation/objectivec/1418901-class_addmethod?language=objc )
        class_replaceMethod(c, new, method_getImplementation(origMethod), method_getTypeEncoding(origMethod));
    else
        method_setImplementation(origMethod, method_getImplementation(newMethod));
}
void Replace_withImp(Class c, SEL orig, IMP new, const char* newImpTypeEncoding)
{
    Method origMethod = class_getInstanceMethod(c, orig);
    if(origMethod == NULL)
        class_addMethod(c, orig, new, newImpTypeEncoding);
    else
        method_setImplementation(origMethod, new);
}

#include <CoreFoundation/CoreFoundation.h> // "CFString is “toll-free bridged” with its Cocoa Foundation counterpart, NSString. This means that the Core Foundation type is interchangeable in function or method calls with the bridged Foundation object. Therefore, in a method where you see an NSString * parameter, you can pass in a CFStringRef, and in a function where you see a CFStringRef parameter, you can pass in an NSString instance. This also applies to concrete subclasses of NSString. See Toll-Free Bridged Types for more information on toll-free bridging." ( https://developer.apple.com/documentation/corefoundation/cfstring?language=objc )

// https://developer.apple.com/documentation/foundation/1395135-nsclassfromstring?language=objc
Class NSClassFromString(CFStringRef /*NSString **/aClassName);

// https://developer.apple.com/documentation/foundation/1395294-nsselectorfromstring?language=objc
SEL NSSelectorFromString(CFStringRef /*NSString **/aSelectorName);

// //

// https://www.mikeash.com/pyblog/objc_msgsends-new-prototype.html
void newExplicitUpdate(id self, SEL _cmd) {
    // Don't run on a dispatch queue! Original code from SDL/src/video/cocoa/SDL_cocoaopengl.m :
    /*
     - (void)explicitUpdate
     {
         if ([NSThread isMainThread]) {
             [super update];
         } else {
             dispatch_sync(dispatch_get_main_queue(), ^{ [super update]; });
         }
     }
     */
    
    CFStringRef aCFString;
    aCFString = CFStringCreateWithCString(NULL, "SDLOpenGLContext", kCFStringEncodingUTF8);
    CFStringRef aCFString2;
    aCFString2 = CFStringCreateWithCString(NULL, "update", kCFStringEncodingUTF8);
    Class cls = NSClassFromString(aCFString);
    
    // https://www.programmerall.com/article/7493171948/
    Class superCls = class_getSuperclass(cls);
    // See /usr/include/objc/message.h :
    struct objc_super obj_super_class = {
        .receiver = self,
        #if !defined(__cplusplus)  &&  !__OBJC2__
            .class = superCls
        #else
            .super_class = superCls
        #endif
        
    };
    
    ((void (*)(void*, SEL))objc_msgSendSuper)(&obj_super_class, NSSelectorFromString(aCFString2));
    
    CFRelease(aCFString);
    CFRelease(aCFString2);
}
#endif
static int render(void *data)
{
    // Hack for macOS //
    threadID_t id = thisThreadID();
    if (id != g_main_thread_id) {
        g_render_thread_id = id;
    }
    
#ifdef __APPLE__
    // Another hack:
    CFStringRef aCFString;
    aCFString = CFStringCreateWithCString(NULL, "SDLOpenGLContext", kCFStringEncodingUTF8);
    CFStringRef aCFString2;
    aCFString2 = CFStringCreateWithCString(NULL, "explicitUpdate", kCFStringEncodingUTF8);
    Replace_withImp(NSClassFromString(aCFString), NSSelectorFromString(aCFString2), (void(*)(void))newExplicitUpdate, "v@"); // Change the method `explicitUpdate` on class `SDLOpenGLContext` to be a call to our overriden one.
    CFRelease(aCFString);
    CFRelease(aCFString2);
#endif
    // //
    
    // https://wiki.libsdl.org/SDL_GetVersion //
    SDL_version compiled;
    SDL_version linked;

    SDL_VERSION(&compiled);
    SDL_GetVersion(&linked);
    printf("We compiled against SDL version %d.%d.%d ...\n",
           compiled.major, compiled.minor, compiled.patch);
    printf("But we are linking against SDL version %d.%d.%d.\n",
           linked.major, linked.minor, linked.patch);
    // //
    
    struct render_sync_state *rstate = data; 
    SDL_Window *window = rstate->arg->in_window; /* cache window ptr */

    SDL_GL_MakeCurrent(window, s_context);

    bool quit = render_wait_cmd(rstate);
    assert(!quit);
    render_init_ctx(rstate->arg);
    bool initialized = rstate->arg->out_success;

    rstate->arg = NULL; /* arg is stale after signalling main thread */
    render_signal_done(rstate);

    while(true) {
    
        quit = render_wait_cmd(rstate);
        if(quit)
            break;

        render_process_cmds(&G_GetRenderWS()->commands);
        if(rstate->swap_buffers) {
            #if defined(__APPLE__) && !defined(NDEBUG)
            SDL_mutex* swaplock = (((struct SDL_WindowData_impl*)((struct SDL_Window_impl*)window)->driverdata)->videodata)->swaplock;
            int ret = SDL_TryLockMutex(swaplock);
            if (ret == SDL_MUTEX_TIMEDOUT) {
                // Locked already!
                printf("Locked already\n");
                assert(0);
            }
            else if (ret == -1) {
                printf("swaplock error: %s\n", SDL_GetError());
                exit(1);
            }
            else {
                assert(ret == 0);
            }
            #endif
            
            SDL_GL_SwapWindow(window);
        }

        render_signal_done(rstate);
    }

    if(initialized) {
        render_destroy_ctx();
    }
    return 0;
}

/*****************************************************************************/
/* EXTERN FUNCTIONS                                                          */
/*****************************************************************************/

bool R_Init(const char *base_path)
{
    ss_e status;
    (void)status;

    SDL_DisplayMode dm;
    SDL_GetDesktopDisplayMode(0, &dm);

    status = Settings_Create((struct setting){
        .name = "pf.video.aspect_ratio",
        .val = (struct sval) {
            .type = ST_TYPE_VEC2,
            .as_vec2 = (vec2_t){dm.w, dm.h}
        },
        .prio = 0,
        .validate = ar_validate,
        .commit = ar_commit,
    });
    assert(status == SS_OKAY);

    struct sval ar_pair;
    Settings_Get("pf.video.aspect_ratio", &ar_pair);
    float ar = ar_pair.as_vec2.x / ar_pair.as_vec2.y;
    float native_ar = (float)dm.w / dm.h;

    vec2_t res_default;
    if(ar < native_ar) {
        res_default = (vec2_t){dm.h * ar, dm.h};
    }else{
        res_default = (vec2_t){dm.w, dm.w / ar}; 
    }

    status = Settings_Create((struct setting){
        .name = "pf.video.resolution",
        .val = (struct sval) {
            .type = ST_TYPE_VEC2,
            .as_vec2 = res_default
        },
        .prio = 1, /* Depends on aspect_ratio */
        .validate = res_validate,
        .commit = res_commit,
    });
    assert(status == SS_OKAY);

    status = Settings_Create((struct setting){
        .name = "pf.video.display_mode",
        .val = (struct sval) {
            .type = ST_TYPE_INT,
            .as_int = PF_WF_BORDERLESS_WIN
        },
        .prio = 0,
        .validate = dm_validate,
        .commit = dm_commit,
    });
    assert(status == SS_OKAY);

    status = Settings_Create((struct setting){
        .name = "pf.video.window_always_on_top",
        .val = (struct sval) {
            .type = ST_TYPE_BOOL,
            .as_bool = false 
        },
        .prio = 0,
        .validate = bool_val_validate,
        .commit = NULL,
    });
    assert(status == SS_OKAY);

    status = Settings_Create((struct setting){
        .name = "pf.video.vsync",
        .val = (struct sval) {
            .type = ST_TYPE_BOOL,
            .as_bool = false 
        },
        .prio = 0,
        .validate = bool_val_validate,
        .commit = vsync_commit,
    });
    assert(status == SS_OKAY);

    status = Settings_Create((struct setting){
        .name = "pf.video.water_reflection",
        .val = (struct sval) {
            .type = ST_TYPE_BOOL,
            .as_bool = true 
        },
        .prio = 0,
        .validate = bool_val_validate,
        .commit = NULL,
    });
    assert(status == SS_OKAY);

    status = Settings_Create((struct setting){
        .name = "pf.video.water_refraction",
        .val = (struct sval) {
            .type = ST_TYPE_BOOL,
            .as_bool = true 
        },
        .prio = 0,
        .validate = bool_val_validate,
        .commit = NULL,
    });
    assert(status == SS_OKAY);

    status = Settings_Create((struct setting){
        .name = "pf.debug.render_log_mask",
        .val = (struct sval) {
            .type = ST_TYPE_INT,
            .as_int = 0x1,
        },
        .prio = 0,
        .validate = int_val_validate,
        .commit = debug_logmask_commit,
    });
    assert(status == SS_OKAY);

    status = Settings_Create((struct setting){
        .name = "pf.debug.trace_gpu",
        .val = (struct sval) {
            .type = ST_TYPE_BOOL,
            .as_bool = false,
        },
        .prio = 0,
        .validate = bool_val_validate,
        .commit = trace_gpu_commit,
    });
    assert(status == SS_OKAY);

    return true; 
}

SDL_Thread *R_Run(struct render_sync_state *rstate)
{
    ASSERT_IN_MAIN_THREAD();

    /* Create the GL context in the main thread and then hand it off to the render thread. 
     * Certain drivers crap out when trying to make the context in the render thread directly. 
     */
    s_context = SDL_GL_CreateContext(rstate->arg->in_window);
    if (s_context == NULL) {
        fprintf(stderr, "Failed to create an OpenGL context: %s\n", SDL_GetError());
        return NULL;
    }
    SDL_GL_MakeCurrent(rstate->arg->in_window, NULL);

    return SDL_CreateThread(render, "render", rstate);
}

void *R_PushArg(const void *src, size_t size)
{
    struct render_workspace *ws = (thisThreadID() == g_render_thread_id) ? G_GetRenderWS()
                                                                         : G_GetSimWS();
    void *ret = stalloc(&ws->args, size);
    if(!ret)
        return ret;

    memcpy(ret, src, size);
    return ret;
}

void R_PushCmd(struct rcmd cmd)
{
    /* If invoking from the render thread, execute immediately
     * as if it were a function call */
    if(thisThreadID() == g_render_thread_id) {

        render_dispatch_cmd(cmd);
        return;
    }

    struct render_workspace *ws = G_GetSimWS();
    queue_rcmd_push(&ws->commands, &cmd);
}

bool R_InitWS(struct render_workspace *ws)
{
    if(!stalloc_init(&ws->args)) 
        goto fail_args;

    if(!queue_rcmd_init(&ws->commands, 2048))
        goto fail_queue;

    return true;

fail_queue:
    stalloc_destroy(&ws->args);
fail_args:
    return false;
}

void R_DestroyWS(struct render_workspace *ws)
{
    queue_rcmd_destroy(&ws->commands);
    stalloc_destroy(&ws->args);
}

void R_ClearWS(struct render_workspace *ws)
{
    queue_rcmd_clear(&ws->commands);
    stalloc_clear(&ws->args);
}

const char *R_GetInfo(enum render_info attr)
{
    switch(attr) {
    case RENDER_INFO_VENDOR:        return s_info_vendor;
    case RENDER_INFO_RENDERER:      return s_info_renderer;
    case RENDER_INFO_VERSION:       return s_info_version;
    case RENDER_INFO_SL_VERSION:    return s_info_sl_version;
    default: assert(0);             return NULL;
    }
}

void R_InitAttributes(void)
{
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);

    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    int ctx_flags = 0;
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_FLAGS, &ctx_flags);
    ctx_flags |= SDL_GL_CONTEXT_DEBUG_FLAG;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, ctx_flags);
}

